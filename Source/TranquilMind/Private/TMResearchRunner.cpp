#include "TMResearchRunner.h"

#include "TargetSpawnerComponent.h"
#include "../TranquilMindSessionManager.h"
#include "../Public/Private/Private/TranquilMindTargetActor.h"
#include "../TranquilMindPhysiologyReceiver.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogTMResearchRunner, Log, All);

double UTMResearchRunner::NowSec() const
{
    return FPlatformTime::Seconds();
}

bool UTMResearchRunner::Initialize(UTargetSpawnerComponent* InOwner,
                                   ATranquilMindSessionManager* InSessionManager,
                                   const FTMResearchConfig& InConfig,
                                   int32 InScheduleSeed,
                                   const FString& InSessionID)
{
    Owner = InOwner;
    SessionManager = InSessionManager;
    Config = InConfig;
    ScheduleSeed = InScheduleSeed;
    SessionID = InSessionID;
    bRunFinalized = false;

    // ---- Generate the block schedule (fail loud) ----
    const FTMSequenceParams Params =
        FTMSequenceParams::FromConfig(Config, static_cast<uint64>(static_cast<uint32>(ScheduleSeed)));

    FString GenError;
    bScheduleValid = FTMSequenceGenerator::Generate(Params, Schedule, GenError);

    if (!bScheduleValid)
    {
        UE_LOG(LogTMResearchRunner, Error,
            TEXT("[Research] FATAL: schedule generation failed (seed=%d): %s. Research Mode will not run."),
            ScheduleSeed, *GenError);
        State = ETMResearchState::Idle;
        return false;
    }

    // Segment counts string.
    FString SegStr;
    for (int32 i = 0; i < Schedule.SegmentNoGoCounts.Num(); ++i)
    {
        SegStr += FString::Printf(TEXT("%s%d"), (i == 0 ? TEXT("") : TEXT("/")), Schedule.SegmentNoGoCounts[i]);
    }

    UE_LOG(LogTMResearchRunner, Warning,
        TEXT("[Research] Schedule OK | trials=%d | GO=%d NOGO=%d | segNoGo=[%s] | seed=%d | hash=%llu"),
        Schedule.Sequence.Num(), Schedule.GoCount, Schedule.NoGoCount, *SegStr, ScheduleSeed,
        static_cast<unsigned long long>(Schedule.SequenceHash));

    {
        FString Preview;
        Preview.Reserve(Schedule.Sequence.Num());
        for (const ETMStimulusType S : Schedule.Sequence)
        {
            Preview.AppendChar(S == ETMStimulusType::NoGo ? TEXT('N') : TEXT('.'));
        }
        UE_LOG(LogTMResearchRunner, Warning, TEXT("[Research] Schedule: %s"), *Preview);
    }

    const bool bLogOpen = Logger.BeginSession(SessionID);
    if (!bLogOpen)
    {
        UE_LOG(LogTMResearchRunner, Error,
            TEXT("[Research] Trial log did NOT open; trials will still run but persistence is unavailable."));
    }

    ITIStream.Initialize(ScheduleSeed ^ 0x17171717);

    TrialIndex = -1;
    BlockIndex = 0;
    N_Hit = N_Omission = N_CorrectRejection = N_Commission = N_Void = 0;
    DuplicateResponseCount = 0;
    bHasPending = false;
    bFixedLocationComputed = false;

    const float InitialITI = DrawITIms();
    LastCompleteSec = NowSec();
    NextOnsetSec = LastCompleteSec + InitialITI / 1000.0;
    State = ETMResearchState::InterTrialInterval;

    UE_LOG(LogTMResearchRunner, Warning,
        TEXT("[Research] Initialized | window=%.0fms visible=%.0fms(FIXED) itiMean=%.0fms(+-%.0f%%) | firstOnset in %.0fms | logHealthy=%s"),
        Config.ResponseWindowMs, Config.StimulusVisibleMs, Config.ITIMeanMs, Config.ITIJitterFraction * 100.0f,
        InitialITI, Logger.IsHealthy() ? TEXT("true") : TEXT("false"));

    return true;
}

float UTMResearchRunner::DrawITIms()
{
    return ITIStream.FRandRange(Config.GetITIMinMs(), Config.GetITIMaxMs());
}

FVector UTMResearchRunner::ComputeFixedStimulusLocation()
{
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    FVector CamLoc = FVector::ZeroVector;
    FVector CamFwd = FVector::ForwardVector;

    if (World)
    {
        if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(World, 0))
        {
            CamLoc = CamMgr->GetCameraLocation();
            CamFwd = CamMgr->GetCameraRotation().Vector();
        }
    }
    return CamLoc + CamFwd * Config.FixedForwardDistanceCm + FVector::UpVector * Config.FixedVerticalOffsetCm;
}

void UTMResearchRunner::Tick(float /*DeltaTime*/)
{
    if (!bScheduleValid)
    {
        return;
    }

    if (IsValid(SessionManager.Get()) &&
        SessionManager->GetActiveInterrupt() == ETMInterruptType::SysAbort &&
        State != ETMResearchState::BlockComplete)
    {
        AbortCurrentTrialAsVoid(TEXT("SysAbort"));
        return;
    }

    const double Now = NowSec();

    switch (State)
    {
    case ETMResearchState::InterTrialInterval:
        if (Now >= NextOnsetSec)
        {
            BeginTrial();
        }
        break;

    case ETMResearchState::TrialActive:
    {
        const ETMStimulusType Stim = Schedule.Sequence[TrialIndex];

        // (1) Fixed-exposure visual offset — happens on time REGARDLESS of any response.
        if (!bVisualHidden && Now >= CurScheduledOffsetSec)
        {
            HideStimulusVisual();
            CurRealizedOffsetSec = Now;
            bVisualHidden = true;
        }

        // (2) Behavioral deadline with no response -> Omission / CorrectRejection.
        if (!bBehaviorResolved && Now >= CurDeadlineSec)
        {
            CurOutcome = (Stim == ETMStimulusType::Go)
                ? ETMTrialOutcome::Omission : ETMTrialOutcome::CorrectRejection;
            CurBehavioralResolutionSec = Now;
            CurResponseTsSec = -1.0;
            CurRTMs = -1.0f;
            bBehaviorResolved = true;
        }

        // (3) Trial completes only when BOTH the fixed visual exposure has ended
        //     AND the behavior is resolved. ITI starts here (see CompleteTrial).
        if (bVisualHidden && bBehaviorResolved && !bTrialCompleted)
        {
            CompleteTrial();
        }
        break;
    }

    case ETMResearchState::Idle:
    case ETMResearchState::BlockComplete:
    default:
        break;
    }
}

void UTMResearchRunner::BeginTrial()
{
    const double Now = NowSec();

    // Now that the next onset has arrived, the previous trial's realized ITI is known.
    if (bHasPending)
    {
        FlushPending(Now);
    }

    ++TrialIndex;
    if (TrialIndex >= Schedule.Sequence.Num())
    {
        FinishBlock();
        return;
    }

    const ETMStimulusType Type = Schedule.Sequence[TrialIndex];

    CurScheduledOnsetSec = NextOnsetSec;
    CurRealizedOnsetSec = Now;
    CurScheduledOffsetSec = Now + Config.StimulusVisibleMs / 1000.0;
    CurRealizedOffsetSec = -1.0;
    CurDeadlineSec = Now + Config.ResponseWindowMs / 1000.0;

    bVisualHidden = false;
    bBehaviorResolved = false;
    bTrialCompleted = false;
    CurOutcome = ETMTrialOutcome::Unresolved;
    CurResponseTsSec = -1.0;
    CurBehavioralResolutionSec = 0.0;
    CurRTMs = -1.0f;
    bCurResponseAfterOffset = false;

    if (!bFixedLocationComputed)
    {
        FixedStimulusLocation = ComputeFixedStimulusLocation();
        bFixedLocationComputed = true;
    }

    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (World)
    {
        UClass* Cls = (Owner && Owner->TargetActorClass.Get() != nullptr)
            ? Owner->TargetActorClass.Get()
            : ATranquilMindTargetActor::StaticClass();

        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SP.Owner = Owner ? Owner->GetOwner() : nullptr;

        ActiveStimulus = World->SpawnActor<ATranquilMindTargetActor>(
            Cls, FixedStimulusLocation, FRotator::ZeroRotator, SP);

        if (IsValid(ActiveStimulus.Get()))
        {
            ActiveStimulus->InitializeResearchVisual(Type, Config.StimulusScale);
            ActiveStimulus->SetActorLocation(FixedStimulusLocation);
            ActiveStimulus->SetActorHiddenInGame(false);
        }
    }

    State = ETMResearchState::TrialActive;

    UE_LOG(LogTMResearchRunner, Verbose,
        TEXT("[Research] Trial %d onset | %s | offset in %.0fms | deadline in %.0fms"),
        TrialIndex, (Type == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
        Config.StimulusVisibleMs, Config.ResponseWindowMs);
}

void UTMResearchRunner::HandleResponse()
{
    // Accept only during an active trial that has NOT yet been behaviorally resolved.
    if (State != ETMResearchState::TrialActive)
    {
        ++DuplicateResponseCount;
        UE_LOG(LogTMResearchRunner, Warning,
            TEXT("[Research] Response ignored (no active trial; state=%d). Spurious/duplicate #%d"),
            static_cast<int32>(State), DuplicateResponseCount);
        return;
    }
    if (bBehaviorResolved)
    {
        // A second press after the outcome is already decided: never alters the outcome,
        // never creates a record, never starts/ends a trial.
        ++DuplicateResponseCount;
        UE_LOG(LogTMResearchRunner, Warning,
            TEXT("[Research] Duplicate response ignored on trial %d (outcome already %s) #%d"),
            TrialIndex,
            (CurOutcome == ETMTrialOutcome::Hit) ? TEXT("Hit") :
            (CurOutcome == ETMTrialOutcome::Commission) ? TEXT("Commission") : TEXT("resolved"),
            DuplicateResponseCount);
        return;
    }

    const double Now = NowSec();
    if (Now > CurDeadlineSec)
    {
        // Input can be delivered before this frame's Tick resolves the expired trial.
        // Ignore it here without changing the outcome, creating a record, counting it
        // as a duplicate, or completing the trial early. Tick remains the authority
        // that resolves the expired trial as Omission / CorrectRejection.
        UE_LOG(LogTMResearchRunner, Verbose,
            TEXT("[Research] Late response ignored on trial %d | now=%.6f deadline=%.6f"),
            TrialIndex, Now, CurDeadlineSec);
        return;
    }

    const ETMStimulusType Stim = Schedule.Sequence[TrialIndex];

    CurRTMs = static_cast<float>((Now - CurRealizedOnsetSec) * 1000.0);
    CurResponseTsSec = Now;
    CurBehavioralResolutionSec = Now;
    CurOutcome = (Stim == ETMStimulusType::Go) ? ETMTrialOutcome::Hit : ETMTrialOutcome::Commission;
    bBehaviorResolved = true;
    bCurResponseAfterOffset = bVisualHidden; // true if the visual already ended

    // CRITICAL: do NOT hide or destroy the stimulus here. The fixed 400ms exposure is
    // preserved. If the visual already ended, the trial can complete promptly; otherwise
    // Tick completes it once the fixed offset fires.
    if (bVisualHidden && !bTrialCompleted)
    {
        CompleteTrial();
    }
}

void UTMResearchRunner::HideStimulusVisual()
{
    if (IsValid(ActiveStimulus.Get()))
    {
        ActiveStimulus->SetActorHiddenInGame(true);
    }
}

void UTMResearchRunner::DestroyStimulusActor()
{
    if (IsValid(ActiveStimulus.Get()))
    {
        ActiveStimulus->SetActorHiddenInGame(true);
        ActiveStimulus->Destroy();
    }
    ActiveStimulus = nullptr;
}

void UTMResearchRunner::CompleteTrial()
{
    if (bTrialCompleted)
    {
        return;
    }
    bTrialCompleted = true;

    const double Now = NowSec();
    DestroyStimulusActor(); // already hidden at the fixed offset; free the actor now

    switch (CurOutcome)
    {
    case ETMTrialOutcome::Hit:              ++N_Hit; break;
    case ETMTrialOutcome::Omission:         ++N_Omission; break;
    case ETMTrialOutcome::CorrectRejection: ++N_CorrectRejection; break;
    case ETMTrialOutcome::Commission:       ++N_Commission; break;
    default: break;
    }

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->RecordTrialOutcome(CurOutcome, (CurOutcome == ETMTrialOutcome::Hit) ? CurRTMs : 0.0f);
    }

    LastCompleteSec = Now;

    // Build the (single) final record for this trial.
    FTMTrialRecord Rec;
    Rec.SessionID = SessionID;
    Rec.OperatingMode = ETMOperatingMode::Research;
    Rec.BlockIndex = BlockIndex;
    Rec.TrialIndex = TrialIndex;
    Rec.StimulusType = Schedule.Sequence[TrialIndex];
    Rec.ScheduleSeed = ScheduleSeed;
    Rec.SequenceHash = Schedule.SequenceHash;
    Rec.ScheduledStimulusOnsetSec = CurScheduledOnsetSec;
    Rec.RealizedStimulusOnsetSec = CurRealizedOnsetSec;
    Rec.ScheduledStimulusOffsetSec = CurScheduledOffsetSec;
    Rec.RealizedStimulusOffsetSec = CurRealizedOffsetSec;
    Rec.ResponseWindowDeadlineSec = CurDeadlineSec;
    Rec.ResponseTimestampSec = CurResponseTsSec;
    Rec.bHadResponse = (CurResponseTsSec >= 0.0);
    Rec.BehavioralResolutionTimestampSec = CurBehavioralResolutionSec;
    Rec.TrialCompleteTimestampSec = Now;
    Rec.RTMs = CurRTMs;
    Rec.Outcome = CurOutcome;
    Rec.bVisualTerminatedByResponse = false; // guaranteed in Research Mode
    Rec.bResponseAcceptedAfterVisualOffset = bCurResponseAfterOffset;
    Rec.ResponseWindowMs = Config.ResponseWindowMs;
    Rec.bValid = (CurOutcome != ETMTrialOutcome::Void);
    FillGSR(Rec);

    const bool bIsLast = (TrialIndex >= Schedule.Sequence.Num() - 1);
    if (bIsLast)
    {
        Rec.ScheduledITIMs = 0.0f;
        Rec.RealizedITIMs = 0.0f;
        bHasPending = false;
        Logger.LogTrial(Rec);
        FinishBlock();
    }
    else
    {
        ScheduledITIms_AfterCurrent = DrawITIms();
        Rec.ScheduledITIMs = ScheduledITIms_AfterCurrent;
        Rec.RealizedITIMs = -1.0f; // filled at next onset
        NextOnsetSec = LastCompleteSec + ScheduledITIms_AfterCurrent / 1000.0;
        PendingRecord = Rec;
        bHasPending = true;
        State = ETMResearchState::InterTrialInterval;
    }
}

void UTMResearchRunner::FlushPending(double RealizedNextOnsetSec)
{
    if (!bHasPending)
    {
        return;
    }
    PendingRecord.RealizedITIMs = static_cast<float>((RealizedNextOnsetSec - LastCompleteSec) * 1000.0);
    Logger.LogTrial(PendingRecord);
    bHasPending = false;
}

void UTMResearchRunner::AbortCurrentTrialAsVoid(const FString& Reason)
{
    ShutdownResearchRun(Reason);
}

void UTMResearchRunner::ShutdownResearchRun(const FString& Reason)
{
    if (bRunFinalized)
    {
        UE_LOG(LogTMResearchRunner, Verbose,
            TEXT("[Research] Shutdown ignored; run already finalized | reason=%s"), *Reason);
        return;
    }

    const FString FinalizationReason =
        Reason.IsEmpty() ? FString(TEXT("ResearchShutdown")) : Reason;

    // Prevent Tick from starting or resolving any more trials while shutdown runs.
    bScheduleValid = false;

    if (State == ETMResearchState::TrialActive && !bTrialCompleted &&
        TrialIndex >= 0 && TrialIndex < Schedule.Sequence.Num())
    {
        bTrialCompleted = true;
        const double Now = NowSec();

        if (!bVisualHidden)
        {
            HideStimulusVisual();
            CurRealizedOffsetSec = Now;
            bVisualHidden = true;
        }
        DestroyStimulusActor();
        ++N_Void;

        FTMTrialRecord Rec;
        Rec.SessionID = SessionID;
        Rec.OperatingMode = ETMOperatingMode::Research;
        Rec.BlockIndex = BlockIndex;
        Rec.TrialIndex = TrialIndex;
        Rec.StimulusType = Schedule.Sequence[TrialIndex];
        Rec.ScheduleSeed = ScheduleSeed;
        Rec.SequenceHash = Schedule.SequenceHash;
        Rec.ScheduledStimulusOnsetSec = CurScheduledOnsetSec;
        Rec.RealizedStimulusOnsetSec = CurRealizedOnsetSec;
        Rec.ScheduledStimulusOffsetSec = CurScheduledOffsetSec;
        Rec.RealizedStimulusOffsetSec = CurRealizedOffsetSec;
        Rec.ResponseWindowDeadlineSec = CurDeadlineSec;
        Rec.ResponseTimestampSec = CurResponseTsSec;
        Rec.bHadResponse = (CurResponseTsSec >= 0.0);
        Rec.BehavioralResolutionTimestampSec = Now;
        Rec.TrialCompleteTimestampSec = Now;
        Rec.RTMs = CurRTMs;
        Rec.Outcome = ETMTrialOutcome::Void;
        Rec.bVisualTerminatedByResponse = false;
        Rec.bResponseAcceptedAfterVisualOffset = bCurResponseAfterOffset;
        Rec.ResponseWindowMs = Config.ResponseWindowMs;
        Rec.ScheduledITIMs = 0.0f;
        Rec.RealizedITIMs = 0.0f;
        Rec.bValid = false;
        Rec.VoidReason = FinalizationReason;
        FillGSR(Rec);
        Logger.LogTrial(Rec);

        if (IsValid(SessionManager.Get()))
        {
            SessionManager->RecordTrialOutcome(ETMTrialOutcome::Void, 0.0f);
        }
    }
    else
    {
        // Defensive cleanup for teardown after the trial record was already completed.
        DestroyStimulusActor();
    }

    // Do not clear bHasPending: a completed trial waiting for its realized ITI still
    // needs exactly one final record before the interrupted footer is written.
    FinishBlock(FinalizationReason);
    UE_LOG(LogTMResearchRunner, Warning,
        TEXT("[Research] INTERRUPTED RUN FINALIZED | reason=%s"), *FinalizationReason);
}

void UTMResearchRunner::FinishBlock(const FString& FinalizationReason)
{
    if (bRunFinalized)
    {
        return;
    }
    State = ETMResearchState::BlockComplete;

    // Flush any completed-but-unflushed record (e.g., abort during ITI).
    if (bHasPending)
    {
        PendingRecord.RealizedITIMs = 0.0f;
        Logger.LogTrial(PendingRecord);
        bHasPending = false;
    }

    // Persist the final counts with an explicit normal/interrupted completion status.
    Logger.LogBlockSummary(BlockIndex, Schedule.GoCount, Schedule.NoGoCount,
        N_Hit, N_Omission, N_CorrectRejection, N_Commission, N_Void,
        ScheduleSeed, Schedule.SequenceHash, FinalizationReason);

    // Persistence status (distinct from behavioral completion).
    Logger.Finalize(FinalizationReason);
    bRunFinalized = true;

    if (FinalizationReason.IsEmpty())
    {
        UE_LOG(LogTMResearchRunner, Warning,
            TEXT("[Research] BLOCK BEHAVIORAL-COMPLETE | trials=%d | Hit=%d Omit=%d CR=%d Comm=%d Void=%d | duplicateResponses=%d | logHealthy=%s records=%d failures=%d"),
            Schedule.Sequence.Num(), N_Hit, N_Omission, N_CorrectRejection, N_Commission, N_Void,
            DuplicateResponseCount, Logger.IsHealthy() ? TEXT("true") : TEXT("false"),
            Logger.GetRecordsWritten(), Logger.GetWriteFailureCount());
    }
    else
    {
        UE_LOG(LogTMResearchRunner, Warning,
            TEXT("[Research] RUN INTERRUPTED | reason=%s | startedThroughTrial=%d | Hit=%d Omit=%d CR=%d Comm=%d Void=%d | duplicateResponses=%d | logHealthy=%s records=%d failures=%d"),
            *FinalizationReason, TrialIndex, N_Hit, N_Omission, N_CorrectRejection, N_Commission, N_Void,
            DuplicateResponseCount, Logger.IsHealthy() ? TEXT("true") : TEXT("false"),
            Logger.GetRecordsWritten(), Logger.GetWriteFailureCount());
    }
}

void UTMResearchRunner::FillGSR(FTMTrialRecord& Record) const
{
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }
    if (UTranquilMindPhysiologyReceiver* Phys = World->GetSubsystem<UTranquilMindPhysiologyReceiver>())
    {
        if (Phys->HasReceivedAnyPacket())
        {
            const FTMPhysiologySample S = Phys->GetLatestSample();
            Record.bGSRAvailable = true;
            Record.GSR_Raw = S.Raw;
            Record.GSR_Smoothed = S.Smoothed;
            Record.GSR_Quality = S.Quality;
            Record.GSR_DeviceTimestampMs = S.TimestampMs;
        }
    }
}
