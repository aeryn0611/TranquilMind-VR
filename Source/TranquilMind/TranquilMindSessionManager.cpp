// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindSessionManager.h"

#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindSessionManager, Log, All);

// ============================================================
//  SECTION A — CONSTRUCTION
// ============================================================

ATranquilMindSessionManager::ATranquilMindSessionManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// ============================================================
//  SECTION B — UNREAL LIFECYCLE
// ============================================================

void ATranquilMindSessionManager::BeginPlay()
{
    Super::BeginPlay();

    MasterClock_SEC = 0.0f;
    BlockClock_SEC = 0.0f;
    bBlockClockFrozen = false;

    CurrentPhase = ETMSessionPhase::Phase_1A_Desensitization;
    ActiveInterrupt = ETMInterruptType::None;
    bPhaseTransitionInProgress = false;

    SessionStats = FSessionStats();
    CurrentBlockStats = FBlockStats();

    SessionStats.SessionID = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    SessionStats.SessionStartUTC = FDateTime::UtcNow();
    SessionStats.TerminatedAtPhase = CurrentPhase;
    SessionStats.TerminationReason = ETMAbortReason::None;
    SessionStats.bProtocolCompliant = true;

    SessionStats.Current_ISI_MS = TranquilMind::MAX_ISI_MS;
    SessionStats.Current_NoiseAlpha = TranquilMind::MIN_NOISE_ALPHA;
    SessionStats.Deadband_ISI = 0;
    SessionStats.Deadband_Noise = 0;
    SessionStats.ConsecutiveSpamBlocks = 0;

    CurrentBlockStats.BlockIndex = 0;
    CurrentBlockStats.WallClockStartSec = 0.0f;
    CurrentBlockStats.Param_ISI_MS = SessionStats.Current_ISI_MS;
    CurrentBlockStats.Param_NoiseAlpha = SessionStats.Current_NoiseAlpha;

    SmoothedGazeDirection = FVector::ForwardVector;
    RawGazeDirection_World = FVector::ForwardVector;
    bHasValidGazeSample = false;

    GazeOffCenter_AccumSEC = 0.0f;
    GazeOnCenter_DwellSEC = 0.0f;
    bHardGateActive = false;
    HardGate_SuspendTimer_SEC = 0.0f;

    bHMDRemoved = false;
    HMDRemoval_CountdownSEC = 0.0f;

    ConsecutiveSpamBlocks = 0;

    EnterPhase_1A();

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Session] Started | ID=%s | ISI=%.0f ms | NoiseAlpha=%.2f"),
        *SessionStats.SessionID,
        SessionStats.Current_ISI_MS,
        SessionStats.Current_NoiseAlpha);
}

void ATranquilMindSessionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ActiveInterrupt != ETMInterruptType::SysAbort &&
        CurrentPhase != ETMSessionPhase::Phase_Terminated)
    {
        UE_LOG(
            LogTranquilMindSessionManager,
            Warning,
            TEXT("[Session] EndPlay before terminal state. Reason=%d"),
            static_cast<int32>(EndPlayReason));

        FinalizeSessionRecord(ETMAbortReason::HardwareLost);
    }

    Super::EndPlay(EndPlayReason);
}

void ATranquilMindSessionManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ActiveInterrupt == ETMInterruptType::SysAbort ||
        CurrentPhase == ETMSessionPhase::Phase_Terminated)
    {
        return;
    }

    // T_master never pauses.
    MasterClock_SEC += DeltaTime;

    TickHMDRemovalWatchdog(DeltaTime);

    if (bHardGateActive)
    {
        TickHardGateSuspendWatchdog(DeltaTime);
    }

    if (ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        return;
    }

    CheckPhaseTransitions();

    if (ActiveInterrupt == ETMInterruptType::SysAbort ||
        CurrentPhase == ETMSessionPhase::Phase_Terminated)
    {
        return;
    }

    if (CurrentPhase == ETMSessionPhase::Phase_II_CoreTraining)
    {
        TickGazeAndHardGate(DeltaTime);

        if (ActiveInterrupt == ETMInterruptType::SysAbort)
        {
            return;
        }
    }

    if (CurrentPhase == ETMSessionPhase::Phase_II_CoreTraining &&
        !bBlockClockFrozen)
    {
        BlockClock_SEC += DeltaTime;

        if (BlockClock_SEC >= TranquilMind::BLOCK_DURATION_SEC)
        {
            EvaluateMicroBlock();
        }
    }
}

// ============================================================
//  SECTION C — INTERRUPTS
// ============================================================

void ATranquilMindSessionManager::TriggerHardGate()
{
    if (bHardGateActive ||
        ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        return;
    }

    bHardGateActive = true;
    bBlockClockFrozen = true;

    GazeOffCenter_AccumSEC = 0.0f;
    GazeOnCenter_DwellSEC = 0.0f;
    HardGate_SuspendTimer_SEC = 0.0f;

    CurrentBlockStats.HardGate_FireCount++;

    if (ActiveInterrupt < ETMInterruptType::HardGate_GazeLost)
    {
        ActiveInterrupt = ETMInterruptType::HardGate_GazeLost;
    }

    OnInterruptStateChanged.Broadcast(ETMInterruptType::HardGate_GazeLost, true);

    UE_LOG(
        LogTranquilMindSessionManager,
        Warning,
        TEXT("[HardGate] Triggered | T_master=%.2f | Block=%d"),
        MasterClock_SEC,
        CurrentBlockStats.BlockIndex);
}

void ATranquilMindSessionManager::ResumeFromHardGate()
{
    if (!bHardGateActive)
    {
        return;
    }

    bHardGateActive = false;
    bBlockClockFrozen = false;

    GazeOffCenter_AccumSEC = 0.0f;
    GazeOnCenter_DwellSEC = 0.0f;
    HardGate_SuspendTimer_SEC = 0.0f;

    if (ActiveInterrupt == ETMInterruptType::HardGate_GazeLost)
    {
        ActiveInterrupt = ETMInterruptType::None;
    }

    OnInterruptStateChanged.Broadcast(ETMInterruptType::HardGate_GazeLost, false);
    OnHardGateResumed.Broadcast();

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[HardGate] Resumed | T_master=%.2f | T_block=%.2f"),
        MasterClock_SEC,
        BlockClock_SEC);
}

void ATranquilMindSessionManager::TriggerSysAbort(ETMAbortReason Reason)
{
    if (ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        return;
    }

    ActiveInterrupt = ETMInterruptType::SysAbort;
    bBlockClockFrozen = true;

    FinalizeSessionRecord(Reason);

    OnInterruptStateChanged.Broadcast(ETMInterruptType::SysAbort, true);
    OnSessionTerminated.Broadcast(Reason);

    SetActorTickEnabled(false);

    UE_LOG(
        LogTranquilMindSessionManager,
        Error,
        TEXT("[SYS_ABORT] Reason=%d | T_master=%.2f | Phase=%d"),
        static_cast<int32>(Reason),
        MasterClock_SEC,
        static_cast<int32>(CurrentPhase));
}

void ATranquilMindSessionManager::ReportSpamInput()
{
    if (ActiveInterrupt == ETMInterruptType::SysAbort ||
        CurrentPhase != ETMSessionPhase::Phase_II_CoreTraining)
    {
        return;
    }

    CurrentBlockStats.Spam_Counter++;

    if (CurrentBlockStats.Spam_Counter > TranquilMind::SPAM_BLOCK_THRESHOLD &&
        !CurrentBlockStats.Flag_Spam_Suspect)
    {
        CurrentBlockStats.Flag_Spam_Suspect = true;

        if (ActiveInterrupt < ETMInterruptType::SpamArtifact_BlockQuarantine)
        {
            ActiveInterrupt = ETMInterruptType::SpamArtifact_BlockQuarantine;
            OnInterruptStateChanged.Broadcast(ETMInterruptType::SpamArtifact_BlockQuarantine, true);
        }

        UE_LOG(
            LogTranquilMindSessionManager,
            Warning,
            TEXT("[Spam] Block quarantined | Block=%d | SpamCounter=%d"),
            CurrentBlockStats.BlockIndex,
            CurrentBlockStats.Spam_Counter);
    }
}

void ATranquilMindSessionManager::NotifyHMDRemoved()
{
    if (bHMDRemoved)
    {
        return;
    }

    bHMDRemoved = true;
    HMDRemoval_CountdownSEC = 0.0f;

    UE_LOG(
        LogTranquilMindSessionManager,
        Warning,
        TEXT("[HMD] Removed | Countdown started"));
}

void ATranquilMindSessionManager::NotifyHMDResumed()
{
    if (!bHMDRemoved)
    {
        return;
    }

    bHMDRemoved = false;
    HMDRemoval_CountdownSEC = 0.0f;

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[HMD] Resumed | Countdown cancelled"));
}

// ============================================================
//  SECTION D — TRIAL DATA INGESTION
// ============================================================

void ATranquilMindSessionManager::RecordTrialOutcome(ETMTrialOutcome Outcome, float RT_MS)
{
    if (CurrentPhase == ETMSessionPhase::Phase_1A_Desensitization ||
        CurrentPhase == ETMSessionPhase::Phase_III_Cooldown ||
        CurrentPhase == ETMSessionPhase::Phase_Terminated ||
        ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        return;
    }

    if (Outcome == ETMTrialOutcome::Unresolved)
    {
        UE_LOG(
            LogTranquilMindSessionManager,
            Warning,
            TEXT("[Trial] Unresolved outcome discarded"));

        return;
    }

    CurrentBlockStats.Trial_Count_Total++;

    switch (Outcome)
    {
    case ETMTrialOutcome::Hit:
        CurrentBlockStats.Trial_Count_Valid++;
        CurrentBlockStats.N_Hit++;

        if (RT_MS > 0.0f && RT_MS <= TranquilMind::RESPONSE_WINDOW_MS)
        {
            CurrentBlockStats.RT_Samples_MS.Add(RT_MS);
        }
        break;

    case ETMTrialOutcome::Omission:
        CurrentBlockStats.Trial_Count_Valid++;
        CurrentBlockStats.N_Omission++;
        break;

    case ETMTrialOutcome::CorrectRejection:
        CurrentBlockStats.Trial_Count_Valid++;
        CurrentBlockStats.N_CorrectRejection++;
        break;

    case ETMTrialOutcome::Commission:
        CurrentBlockStats.Trial_Count_Valid++;
        CurrentBlockStats.N_Commission++;
        break;

    case ETMTrialOutcome::Void:
        CurrentBlockStats.N_Void++;
        break;

    case ETMTrialOutcome::Unresolved:
    default:
        break;
    }
}

void ATranquilMindSessionManager::UpdateGazeVector(FVector InRawGazeDirection_World)
{
    const FVector NormalizedGaze = InRawGazeDirection_World.GetSafeNormal();

    if (NormalizedGaze.IsNearlyZero())
    {
        return;
    }

    RawGazeDirection_World = NormalizedGaze;
    bHasValidGazeSample = true;
}

// ============================================================
//  SECTION E — PHASES
// ============================================================

void ATranquilMindSessionManager::EnterPhase_1A()
{
    CurrentPhase = ETMSessionPhase::Phase_1A_Desensitization;
    SessionStats.TerminatedAtPhase = CurrentPhase;

    OnPhaseChanged.Broadcast(CurrentPhase);

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Phase 1A] Desensitization | T_master=%.2f"),
        MasterClock_SEC);
}

void ATranquilMindSessionManager::EnterPhase_1B()
{
    CurrentPhase = ETMSessionPhase::Phase_1B_Baseline;
    SessionStats.TerminatedAtPhase = CurrentPhase;

    CurrentBlockStats = FBlockStats();
    CurrentBlockStats.BlockIndex = 0;
    CurrentBlockStats.WallClockStartSec = MasterClock_SEC;
    CurrentBlockStats.Param_ISI_MS = SessionStats.Current_ISI_MS;
    CurrentBlockStats.Param_NoiseAlpha = SessionStats.Current_NoiseAlpha;

    BlockClock_SEC = 0.0f;

    OnPhaseChanged.Broadcast(CurrentPhase);

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Phase 1B] Baseline | T_master=%.2f"),
        MasterClock_SEC);
}

void ATranquilMindSessionManager::EnterPhase_II()
{
    SealCurrentBlock();

    if (SessionStats.BlockHistory.Num() > 0)
    {
        const FBlockStats& BaselineBlock = SessionStats.BlockHistory[0];

        if (BaselineBlock.N_Hit > 0 && BaselineBlock.RT_Median_MS > 0.0f)
        {
            SessionStats.RT_Baseline_MS = BaselineBlock.RT_Median_MS;
            SessionStats.bBaselineEstablished = true;
        }
        else
        {
            SessionStats.RT_Baseline_MS = TranquilMind::RESPONSE_WINDOW_MS * 0.6f;
            SessionStats.bBaselineEstablished = false;
        }
    }

    ResetCurrentBlock();

    CurrentPhase = ETMSessionPhase::Phase_II_CoreTraining;
    SessionStats.TerminatedAtPhase = CurrentPhase;

    bBlockClockFrozen = false;
    BlockClock_SEC = 0.0f;

    BroadcastStaircaseParams();
    OnPhaseChanged.Broadcast(CurrentPhase);

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Phase II] Core Training | BaselineRT=%.1f | ISI=%.0f | Noise=%.2f"),
        SessionStats.RT_Baseline_MS,
        SessionStats.Current_ISI_MS,
        SessionStats.Current_NoiseAlpha);
}

void ATranquilMindSessionManager::EnterPhase_III()
{
    CurrentPhase = ETMSessionPhase::Phase_III_Cooldown;
    SessionStats.TerminatedAtPhase = CurrentPhase;

    bBlockClockFrozen = true;

    BeginAsyncSessionFlush();

    OnPhaseChanged.Broadcast(CurrentPhase);

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Phase III] Cooldown | T_master=%.2f"),
        MasterClock_SEC);
}

void ATranquilMindSessionManager::CheckPhaseTransitions()
{
    if (bPhaseTransitionInProgress)
    {
        return;
    }

    bPhaseTransitionInProgress = true;

    switch (CurrentPhase)
    {
    case ETMSessionPhase::Phase_1A_Desensitization:
        if (MasterClock_SEC >= TranquilMind::PHASE_1A_END_SEC)
        {
            EnterPhase_1B();
        }
        break;

    case ETMSessionPhase::Phase_1B_Baseline:
        if (MasterClock_SEC >= TranquilMind::PHASE_1B_END_SEC)
        {
            EnterPhase_II();
        }
        break;

    case ETMSessionPhase::Phase_II_CoreTraining:
        if (MasterClock_SEC >= TranquilMind::PHASE_II_END_SEC)
        {
            EnterPhase_III();
        }
        break;

    case ETMSessionPhase::Phase_III_Cooldown:
        if (MasterClock_SEC >= TranquilMind::SESSION_TOTAL_SEC)
        {
            CurrentPhase = ETMSessionPhase::Phase_Terminated;
            SessionStats.TerminatedAtPhase = CurrentPhase;

            FinalizeSessionRecord(ETMAbortReason::None);

            OnSessionTerminated.Broadcast(ETMAbortReason::None);
            SetActorTickEnabled(false);

            UE_LOG(
                LogTranquilMindSessionManager,
                Log,
                TEXT("[Session] Natural completion | T_master=%.2f"),
                MasterClock_SEC);
        }
        break;

    case ETMSessionPhase::Phase_Terminated:
    default:
        break;
    }

    bPhaseTransitionInProgress = false;
}

// ============================================================
//  SECTION F — BLOCK EVALUATION
// ============================================================

void ATranquilMindSessionManager::EvaluateMicroBlock()
{
    SealCurrentBlock();

    if (SessionStats.BlockHistory.Num() <= 0)
    {
        ResetCurrentBlock();
        return;
    }

    FBlockStats& SealedBlock = SessionStats.BlockHistory.Last();

    if (SealedBlock.Flag_Spam_Suspect)
    {
        ConsecutiveSpamBlocks++;
        SessionStats.ConsecutiveSpamBlocks = ConsecutiveSpamBlocks;

        OnBlockEvaluated.Broadcast(SealedBlock);

        UE_LOG(
            LogTranquilMindSessionManager,
            Warning,
            TEXT("[BlockEval] Quarantined | Block=%d | Spam=%d | Consecutive=%d"),
            SealedBlock.BlockIndex,
            SealedBlock.Spam_Counter,
            ConsecutiveSpamBlocks);

        if (ConsecutiveSpamBlocks >= TranquilMind::SPAM_CONSECUTIVE_ABORT)
        {
            TriggerSysAbort(ETMAbortReason::ConsecutiveSpamBlocks);
            return;
        }

        if (ActiveInterrupt == ETMInterruptType::SpamArtifact_BlockQuarantine)
        {
            ActiveInterrupt = ETMInterruptType::None;
            OnInterruptStateChanged.Broadcast(ETMInterruptType::SpamArtifact_BlockQuarantine, false);
        }

        ResetCurrentBlock();
        return;
    }

    ConsecutiveSpamBlocks = 0;
    SessionStats.ConsecutiveSpamBlocks = 0;

    if (ActiveInterrupt == ETMInterruptType::SpamArtifact_BlockQuarantine)
    {
        ActiveInterrupt = ETMInterruptType::None;
        OnInterruptStateChanged.Broadcast(ETMInterruptType::SpamArtifact_BlockQuarantine, false);
    }

    const FBlockStats* ReferenceBlock = FindLastCleanBlock();

    StepTrackA_ISI(SealedBlock, ReferenceBlock);
    StepTrackB_Noise(SealedBlock);

    BroadcastStaircaseParams();

    OnBlockEvaluated.Broadcast(SealedBlock);

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[BlockEval] Block=%d | Hit=%d | Omit=%d | CR=%d | Comm=%d | Void=%d | RTMedian=%.1f | ISI=%.0f | Noise=%.2f"),
        SealedBlock.BlockIndex,
        SealedBlock.N_Hit,
        SealedBlock.N_Omission,
        SealedBlock.N_CorrectRejection,
        SealedBlock.N_Commission,
        SealedBlock.N_Void,
        SealedBlock.RT_Median_MS,
        SessionStats.Current_ISI_MS,
        SessionStats.Current_NoiseAlpha);

    ResetCurrentBlock();
}

void ATranquilMindSessionManager::SealCurrentBlock()
{
    if (CurrentBlockStats.RT_Samples_MS.Num() > 0)
    {
        TArray<float> SortedSamples = CurrentBlockStats.RT_Samples_MS;
        SortedSamples.Sort();

        const int32 Count = SortedSamples.Num();
        const int32 MiddleIndex = Count / 2;

        if ((Count % 2) == 0)
        {
            CurrentBlockStats.RT_Median_MS =
                (SortedSamples[MiddleIndex - 1] + SortedSamples[MiddleIndex]) * 0.5f;
        }
        else
        {
            CurrentBlockStats.RT_Median_MS = SortedSamples[MiddleIndex];
        }
    }
    else
    {
        CurrentBlockStats.RT_Median_MS = 0.0f;
    }

    CurrentBlockStats.Param_ISI_MS = SessionStats.Current_ISI_MS;
    CurrentBlockStats.Param_NoiseAlpha = SessionStats.Current_NoiseAlpha;

    SessionStats.Total_Trial_Count_Total += CurrentBlockStats.Trial_Count_Total;
    SessionStats.Total_Trial_Count_Valid += CurrentBlockStats.Trial_Count_Valid;

    SessionStats.Total_N_Hit += CurrentBlockStats.N_Hit;
    SessionStats.Total_N_Omission += CurrentBlockStats.N_Omission;
    SessionStats.Total_N_CorrectRejection += CurrentBlockStats.N_CorrectRejection;
    SessionStats.Total_N_Commission += CurrentBlockStats.N_Commission;
    SessionStats.Total_N_Void += CurrentBlockStats.N_Void;

    SessionStats.BlockHistory.Add(CurrentBlockStats);
}

void ATranquilMindSessionManager::ResetCurrentBlock()
{
    const int32 NextBlockIndex = SessionStats.BlockHistory.Num();

    CurrentBlockStats = FBlockStats();
    CurrentBlockStats.BlockIndex = NextBlockIndex;
    CurrentBlockStats.WallClockStartSec = MasterClock_SEC;
    CurrentBlockStats.Param_ISI_MS = SessionStats.Current_ISI_MS;
    CurrentBlockStats.Param_NoiseAlpha = SessionStats.Current_NoiseAlpha;

    BlockClock_SEC = 0.0f;
}

const FBlockStats* ATranquilMindSessionManager::FindLastCleanBlock() const
{
    const int32 StartIndex = SessionStats.BlockHistory.Num() - 2;

    for (int32 Index = StartIndex; Index >= 0; --Index)
    {
        const FBlockStats& Candidate = SessionStats.BlockHistory[Index];

        if (!Candidate.Flag_Spam_Suspect)
        {
            return &Candidate;
        }
    }

    return nullptr;
}

// ============================================================
//  SECTION G — STAIRCASE
// ============================================================

void ATranquilMindSessionManager::StepTrackA_ISI(const FBlockStats& CurrentBlock, const FBlockStats* ReferenceBlock)
{
    ETMStaircaseStep StepTaken = ETMStaircaseStep::Hold_Deadband;

    const bool bMustDescend = CurrentBlock.N_Omission >= 2;

    const bool bCanAscend =
        ReferenceBlock != nullptr &&
        CurrentBlock.N_Omission == 0 &&
        CurrentBlock.RT_Median_MS > 0.0f &&
        CurrentBlock.RT_Median_MS < SessionStats.RT_Baseline_MS;

    if (bMustDescend)
    {
        SessionStats.Current_ISI_MS += TranquilMind::ISI_STEP_MAJOR_MS;
        SessionStats.Deadband_ISI = 0;
        StepTaken = ETMStaircaseStep::Descend;
    }
    else if (bCanAscend)
    {
        SessionStats.Current_ISI_MS -= TranquilMind::ISI_STEP_MAJOR_MS;
        SessionStats.Deadband_ISI = 0;
        StepTaken = ETMStaircaseStep::Ascend;
    }
    else
    {
        SessionStats.Deadband_ISI++;

        if (SessionStats.Deadband_ISI >= TranquilMind::DEADBAND_ISI_THRESHOLD)
        {
            SessionStats.Current_ISI_MS -= TranquilMind::ISI_STEP_MICRO_MS;
            SessionStats.Deadband_ISI = 0;
            StepTaken = ETMStaircaseStep::Hold_Microperturbation;
        }
        else
        {
            StepTaken = ETMStaircaseStep::Hold_Deadband;
        }
    }

    SessionStats.Current_ISI_MS = FMath::Clamp(
        SessionStats.Current_ISI_MS,
        TranquilMind::MIN_ISI_MS,
        TranquilMind::MAX_ISI_MS);

    if (SessionStats.BlockHistory.Num() > 0)
    {
        FBlockStats& SealedBlock = SessionStats.BlockHistory.Last();
        SealedBlock.StepTaken_TrackA = StepTaken;
        SealedBlock.Param_ISI_MS = SessionStats.Current_ISI_MS;
    }
}

void ATranquilMindSessionManager::StepTrackB_Noise(const FBlockStats& CurrentBlock)
{
    ETMStaircaseStep StepTaken = ETMStaircaseStep::Hold_Deadband;

    if (CurrentBlock.N_Commission == 0)
    {
        SessionStats.Current_NoiseAlpha += TranquilMind::NOISE_STEP_MAJOR;
        SessionStats.Deadband_Noise = 0;
        StepTaken = ETMStaircaseStep::Ascend;
    }
    else if (CurrentBlock.N_Commission >= 1)
    {
        SessionStats.Current_NoiseAlpha -= TranquilMind::NOISE_STEP_MAJOR;
        SessionStats.Deadband_Noise = 0;
        StepTaken = ETMStaircaseStep::Descend;
    }
    else
    {
        SessionStats.Deadband_Noise++;

        if (SessionStats.Deadband_Noise >= TranquilMind::DEADBAND_NOISE_THRESHOLD)
        {
            SessionStats.Current_NoiseAlpha += TranquilMind::NOISE_STEP_MICRO;
            SessionStats.Deadband_Noise = 0;
            StepTaken = ETMStaircaseStep::Hold_Microperturbation;
        }
        else
        {
            StepTaken = ETMStaircaseStep::Hold_Deadband;
        }
    }

    SessionStats.Current_NoiseAlpha = FMath::Clamp(
        SessionStats.Current_NoiseAlpha,
        TranquilMind::MIN_NOISE_ALPHA,
        TranquilMind::MAX_NOISE_ALPHA);

    if (SessionStats.BlockHistory.Num() > 0)
    {
        FBlockStats& SealedBlock = SessionStats.BlockHistory.Last();
        SealedBlock.StepTaken_TrackB = StepTaken;
        SealedBlock.Param_NoiseAlpha = SessionStats.Current_NoiseAlpha;
    }
}

void ATranquilMindSessionManager::BroadcastStaircaseParams()
{
    OnStaircaseParamsUpdated.Broadcast(
        SessionStats.Current_ISI_MS,
        SessionStats.Current_NoiseAlpha);
}

// ============================================================
//  SECTION H — GAZE + HARD GATE
// ============================================================

void ATranquilMindSessionManager::TickGazeAndHardGate(float DeltaTime)
{
    if (!bHasValidGazeSample)
    {
        return;
    }

    const FVector NewSmoothedGaze = SmoothenGazeVector(RawGazeDirection_World);
    const bool bOnCenter = IsGazeOnCenter(NewSmoothedGaze);

    if (!bHardGateActive)
    {
        if (!bOnCenter)
        {
            GazeOffCenter_AccumSEC += DeltaTime;
            CurrentBlockStats.HardGate_AccumOffcenter_SEC += DeltaTime;

            if (GazeOffCenter_AccumSEC >= TranquilMind::HARDGATE_OFFCENTER_ACCUM_SEC)
            {
                TriggerHardGate();
            }
        }
        else
        {
            GazeOffCenter_AccumSEC = 0.0f;
        }

        return;
    }

    if (bOnCenter)
    {
        GazeOnCenter_DwellSEC += DeltaTime;

        if (GazeOnCenter_DwellSEC >= TranquilMind::HARDGATE_RESUME_DWELL_SEC)
        {
            ResumeFromHardGate();
        }
    }
    else
    {
        GazeOnCenter_DwellSEC = 0.0f;
    }
}

FVector ATranquilMindSessionManager::SmoothenGazeVector(const FVector& RawGaze_World)
{
    const float DeltaSeconds = GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.016f;
    const float SmoothingSeconds = FMath::Max(TranquilMind::HARDGATE_GAZE_SMOOTHING_MS / 1000.0f, KINDA_SMALL_NUMBER);
    const float Alpha = FMath::Clamp(DeltaSeconds / (SmoothingSeconds + DeltaSeconds), 0.0f, 1.0f);

    SmoothedGazeDirection = FMath::Lerp(
        SmoothedGazeDirection,
        RawGaze_World.GetSafeNormal(),
        Alpha).GetSafeNormal();

    return SmoothedGazeDirection;
}

bool ATranquilMindSessionManager::IsGazeOnCenter(const FVector& SmoothedGaze) const
{
    const FVector CenterDirection = FVector::ForwardVector;
    const float Dot = FVector::DotProduct(SmoothedGaze.GetSafeNormal(), CenterDirection);
    const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(HardGate_CPTZoneHalfAngle_DEG));

    return Dot >= CosThreshold;
}

// ============================================================
//  SECTION I — WATCHDOGS
// ============================================================

void ATranquilMindSessionManager::TickHMDRemovalWatchdog(float DeltaTime)
{
    if (!bHMDRemoved)
    {
        return;
    }

    HMDRemoval_CountdownSEC += DeltaTime;

    if (HMDRemoval_CountdownSEC >= TranquilMind::HMD_REMOVAL_ABORT_SEC)
    {
        TriggerSysAbort(ETMAbortReason::HMDRemovedTimeout);
    }
}

void ATranquilMindSessionManager::TickHardGateSuspendWatchdog(float DeltaTime)
{
    if (!bHardGateActive)
    {
        return;
    }

    HardGate_SuspendTimer_SEC += DeltaTime;

    if (HardGate_SuspendTimer_SEC >= TranquilMind::HARDGATE_SUSPEND_ABORT_SEC)
    {
        TriggerSysAbort(ETMAbortReason::HardGateSuspendTimeout);
    }
}

// ============================================================
//  SECTION J — SESSION PERSISTENCE
// ============================================================

void ATranquilMindSessionManager::BeginAsyncSessionFlush()
{
    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Flush] Async session flush requested | SealedBlocks=%d"),
        SessionStats.BlockHistory.Num());
}

void ATranquilMindSessionManager::FinalizeSessionRecord(ETMAbortReason Reason)
{
    SessionStats.TerminationReason = Reason;
    SessionStats.TerminatedAtPhase = CurrentPhase;
    SessionStats.bProtocolCompliant = Reason == ETMAbortReason::None;

    if (SessionStats.Total_Trial_Count_Total > 0)
    {
        SessionStats.Void_Rate =
            static_cast<float>(SessionStats.Total_N_Void) /
            static_cast<float>(SessionStats.Total_Trial_Count_Total);
    }
    else
    {
        SessionStats.Void_Rate = 0.0f;
    }

    SessionStats.bQA_LowExposure = SessionStats.Void_Rate > TranquilMind::QA_MAX_VOID_RATE;

    UE_LOG(
        LogTranquilMindSessionManager,
        Log,
        TEXT("[Session] Finalized | Compliant=%s | Reason=%d | VoidRate=%.2f | LowExposure=%s | Blocks=%d"),
        SessionStats.bProtocolCompliant ? TEXT("YES") : TEXT("NO"),
        static_cast<int32>(Reason),
        SessionStats.Void_Rate,
        SessionStats.bQA_LowExposure ? TEXT("YES") : TEXT("NO"),
        SessionStats.BlockHistory.Num());
}
