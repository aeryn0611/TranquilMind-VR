// Fill out your copyright notice in the Description page of Project Settings.

#include "TargetSpawnerComponent.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "../TranquilMindSessionManager.h"
#include "../Public/Private/Private/TranquilMindTargetActor.h"
#include "TMDemoLifecycleComponent.h"
#include "TMDemoSessionFrame.h"
#include "TMVisualMotionComponent.h"
#include "TMResearchSettings.h"
#include "TMStage1Trace.h"
#include "TMResearchRunner.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargetSpawnerComponent, Log, All);

namespace TranquilMindSpawner
{
    constexpr float SpawnDepth_CM = 2000.0f;
    constexpr float GoProbability = 0.80f;

    // TEMP DEBUG POSITION
    constexpr float DebugSpawnX_CM = 800.0f;
    constexpr float DebugSpawnY_CM = 0.0f;
    constexpr float DebugSpawnZ_CM = 150.0f;
}

UTargetSpawnerComponent::UTargetSpawnerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    TargetActorClass = ATranquilMindTargetActor::StaticClass();

    CurrentISI_MS = TranquilMind::MAX_ISI_MS;
    CurrentNoiseAlpha = TranquilMind::MIN_NOISE_ALPHA;
    LastTriggerTimestamp_SEC = -1.0f;
    LastSpawnTimestamp_SEC = -1.0f;

    StimulusRandomSeed = 5202;
    StimulusRandomStream.Initialize(StimulusRandomSeed);
}

void UTargetSpawnerComponent::BeginPlay()
{
    Super::BeginPlay();

    StimulusRandomStream.Initialize(StimulusRandomSeed);

    if (TargetActorClass.Get() == nullptr)
    {
        TargetActorClass = ATranquilMindTargetActor::StaticClass();
    }

    if (!IsValid(SessionManager.Get()))
    {
        AActor* FoundActor = UGameplayStatics::GetActorOfClass(
            this,
            ATranquilMindSessionManager::StaticClass());

        ATranquilMindSessionManager* FoundSessionManager =
            Cast<ATranquilMindSessionManager>(FoundActor);

        if (IsValid(FoundSessionManager))
        {
            RegisterSessionManager(FoundSessionManager);

            UE_LOG(
                LogTargetSpawnerComponent,
                Warning,
                TEXT("[TargetSpawner] Auto-registered SessionManager: %s"),
                *FoundSessionManager->GetName());
        }
        else
        {
            UE_LOG(
                LogTargetSpawnerComponent,
                Error,
                TEXT("[TargetSpawner] No ATranquilMindSessionManager found in level. Spawning disabled."));
        }
    }

    if (bDebugHardwareMode)
    {
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[HardwareDebug] Enabled | ISI=%.0f ms | Alternating GO/NOGO | HardGate bypassed"),
            DebugHardware_ISI_MS);
    }

    if (bDemoMode)
    {
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[DemoMode] Enabled | ISI=%.0f ms | ResponseWindow=%.0f ms | Duration=%.0f s"),
            DemoMode_ISI_MS, DemoMode_ResponseWindow_MS, DemoMode_Duration_SEC);
    }

    // ============================================================
    //  OPERATING MODE (single authority) — Demo vs Research
    // ============================================================
    OperatingMode = UTMResearchSettings::GetEffectiveOperatingMode();

    UE_LOG(LogTargetSpawnerComponent, Warning,
        TEXT("[Mode] ===== TranquilMind OperatingMode = %s ====="),
        (OperatingMode == ETMOperatingMode::Research) ? TEXT("RESEARCH") : TEXT("DEMO"));

    // ---- STAGE 1 INSTRUMENTATION, schema 2 (log-only) ----
    // Opened here because this is the earliest TranquilMind hook in a world and
    // it necessarily precedes every target presentation (this component is what
    // spawns them). The mode string is passed in rather than read by the tracer,
    // so the tracer never touches settings or protected state.
    FTMStage1Trace::LogSessionBegin(
        GetWorld(),
        (OperatingMode == ETMOperatingMode::Research) ? TEXT("Research") : TEXT("Demo"));

    if (OperatingMode == ETMOperatingMode::Research)
    {
        const UTMResearchSettings* Settings = UTMResearchSettings::Get();
        const FTMResearchConfig ResearchConfig =
            Settings != nullptr ? Settings->ResearchConfig : FTMResearchConfig();

        // Session ID: reuse the SessionManager's if available, else a fresh GUID.
        FString ResearchSessionID;
        if (IsValid(SessionManager.Get()))
        {
            ResearchSessionID = SessionManager->GetSessionStats().SessionID;
        }
        if (ResearchSessionID.IsEmpty())
        {
            ResearchSessionID = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        }

        // Phase 1 uses the training seed for the (single) block.
        const int32 Seed = ResearchConfig.TrainingSeed;

        ResearchRunner = NewObject<UTMResearchRunner>(this, UTMResearchRunner::StaticClass());
        const bool bOK = ResearchRunner->Initialize(
            this, SessionManager.Get(), ResearchConfig, Seed, ResearchSessionID);

        if (!bOK)
        {
            UE_LOG(LogTargetSpawnerComponent, Error,
                TEXT("[Mode] Research runner failed to initialize; no trials will run."));
        }
    }

    // Demo only: consume the frame-captured notification for the pending-spawn
    // exactly-once handshake. In Research the subsystem does not exist and this
    // block never runs, so no subscription can exist either.
    if (OperatingMode == ETMOperatingMode::Demo)
    {
        if (UWorld* World = GetWorld())
        {
            if (UTMDemoSessionFrameSubsystem* Frame =
                    World->GetSubsystem<UTMDemoSessionFrameSubsystem>())
            {
                Frame->OnDemoFrameCaptured.AddUObject(
                    this, &UTargetSpawnerComponent::HandleDemoFrameCaptured);
            }
        }

        // ---- STAGE 1 INSTRUMENTATION (log-only) ----
        // Publish a read-only view of ActiveTargets.Num() so SNAP can record
        // the registered-target count alongside the visible-presentation count
        // (Kit §6.3). Weak-captured: a stale provider answers -1 rather than
        // touching a dead component. Demo only — Research is left untouched.
        TWeakObjectPtr<UTargetSpawnerComponent> WeakSelf(this);
        FTMStage1Trace::SetActiveTargetsProvider(
            [WeakSelf](UWorld*) -> int32
            {
                return WeakSelf.IsValid() ? WeakSelf->ActiveTargets.Num() : -1;
            });

        // ---- STAGE 2 INSTRUMENTATION (log-only) ----
        // The official live Demo presentation count (0 or 1). This is what
        // validates Spec §11.1 S2-A directly, instead of inferring it from the
        // world-scan visiblepresentations figure. Same weak-capture discipline.
        FTMStage1Trace::SetLivePresentationProvider(
            [WeakSelf](UWorld*) -> int32
            {
                if (!WeakSelf.IsValid())
                {
                    return -1;
                }
                return WeakSelf->HasLiveDemoPresentation() ? 1 : 0;
            });
    }
}

void UTargetSpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Teardown safety for the pending-spawn handshake: drop the request and the
    // subscription so a capture during world teardown cannot reach a dying
    // spawner. The subsystem may already be deinitialized here; guard for it.
    bDemoSpawnPendingFrame = false;
    if (UWorld* World = GetWorld())
    {
        if (UTMDemoSessionFrameSubsystem* Frame =
                World->GetSubsystem<UTMDemoSessionFrameSubsystem>())
        {
            Frame->OnDemoFrameCaptured.RemoveAll(this);
        }
    }

    if (OperatingMode == ETMOperatingMode::Research && IsValid(ResearchRunner.Get()))
    {
        ResearchRunner->ShutdownResearchRun(FString::Printf(
            TEXT("ComponentEndPlay:%d"), static_cast<int32>(EndPlayReason)));
    }

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->OnStaircaseParamsUpdated.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::ApplyStaircaseParams);

        SessionManager->OnHardGateResumed.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleHardGateResumed);

        SessionManager->OnInterruptStateChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleInterruptStateChanged);

        SessionManager->OnPhaseChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandlePhaseChanged);
    }

    DestroyAllActiveTargetsAsVoid();

    // STAGE 2 (Spec §4.9.1, "world teardown" and "map transition"): release the
    // official presentation explicitly. The weak reference would also self-clear
    // when the actor dies with the world, but an explicit release here means no
    // stale ownership can survive this component under any teardown order.
    LiveDemoPresentation.Reset();

    // Stage 1 instrumentation teardown (log-only state; no gameplay effect).
    // SESSION_END records the real EEndPlayReason so a manually-stopped PIE run
    // is distinguishable from a natural session end.
    FTMStage1Trace::ClearActiveTargetsProvider();
    FTMStage1Trace::ClearLivePresentationProvider();
    FTMStage1Trace::LogSessionEnd(
        GetWorld(),
        static_cast<int32>(EndPlayReason),
        *UEnum::GetValueAsString(EndPlayReason));

    Super::EndPlay(EndPlayReason);
}

void UTargetSpawnerComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ---- Research Mode: drive the discrete-flashed state machine; skip ALL demo logic ----
    if (OperatingMode == ETMOperatingMode::Research)
    {
        if (IsValid(ResearchRunner.Get()))
        {
            ResearchRunner->Tick(DeltaTime);
        }
        return;
    }

    CleanupResolvedTargets();

    if (bDemoMode)
    {
        if (bDemoEnded)
        {
            return;
        }
        DemoElapsed_SEC += DeltaTime;
        if (DemoElapsed_SEC >= DemoMode_Duration_SEC)
        {
            bDemoEnded = true;
            DestroyAllActiveTargetsAsVoid();
            UE_LOG(LogTargetSpawnerComponent, Warning,
                TEXT("[DemoMode] Session ended | Elapsed=%.1f s"),
                DemoElapsed_SEC);
            return;
        }
    }

    if (!CanSpawnTargetsNow())
    {
        if (IsValid(SessionManager.Get()))
        {
            const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

            if (CurrentPhase == ETMSessionPhase::Phase_III_Cooldown ||
                CurrentPhase == ETMSessionPhase::Phase_Terminated)
            {
                DestroyAllActiveTargetsAsVoid();
            }
        }

        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    const float Now_SEC = World->GetTimeSeconds();

    if (LastSpawnTimestamp_SEC < 0.0f)
    {
        SpawnNextTarget();
        return;
    }

    const float ElapsedSinceLastSpawn_MS = (Now_SEC - LastSpawnTimestamp_SEC) * 1000.0f;

    const float EffectiveISI_MS = bDemoMode
        ? DemoMode_ISI_MS
        : (bDebugHardwareMode ? DebugHardware_ISI_MS : CurrentISI_MS);
    if (ElapsedSinceLastSpawn_MS >= EffectiveISI_MS)
    {
        SpawnNextTarget();
    }
}

void UTargetSpawnerComponent::ApplyStaircaseParams(float NewISI_MS, float NewNoiseAlpha)
{
    CurrentISI_MS = FMath::Clamp(
        NewISI_MS,
        TranquilMind::MIN_ISI_MS,
        TranquilMind::MAX_ISI_MS);

    CurrentNoiseAlpha = FMath::Clamp(
        NewNoiseAlpha,
        TranquilMind::MIN_NOISE_ALPHA,
        TranquilMind::MAX_NOISE_ALPHA);

    UE_LOG(
        LogTargetSpawnerComponent,
        Log,
        TEXT("[TargetSpawner] Staircase params applied | ISI=%.0f ms | NoiseAlpha=%.2f"),
        CurrentISI_MS,
        CurrentNoiseAlpha);
}

void UTargetSpawnerComponent::RestartTrial()
{
    // RESEARCH INTEGRITY GUARD (defence in depth).
    // The Demo trial machinery must never run while OperatingMode is Research,
    // no matter which caller reaches it. This function is BlueprintCallable and
    // is also reached from HandleHardGateResumed(), which is bound to
    // SessionManager::OnHardGateResumed unconditionally in both modes — so a
    // gaze hard-gate resume during a Research block would otherwise spawn a
    // self-scoring Demo target at the Research stimulus location and record a
    // trial outcome that has no corresponding JSONL line.
    if (OperatingMode == ETMOperatingMode::Research)
    {
        LogResearchSuppressionOnce(TEXT("RestartTrial"));
        return;
    }

    DestroyAllActiveTargetsAsVoid();

    LastSpawnTimestamp_SEC = -1.0f;

    if (CanSpawnTargetsNow())
    {
        SpawnNextTarget();
    }
}

bool UTargetSpawnerComponent::HasLiveDemoPresentation() const
{
    const ATranquilMindTargetActor* Live = LiveDemoPresentation.Get();
    return IsValid(Live) && !Live->IsActorBeingDestroyed();
}

void UTargetSpawnerComponent::RegisterPhase4Target(ATranquilMindTargetActor* Target)
{
    if (!IsValid(Target))
    {
        return;
    }

    // Spec §11.1 S2-B: max ActiveTargets = 1. The spawn gate already guarantees
    // it; this is the assertion that makes the guarantee local and checkable
    // rather than an emergent property of a gate two call frames away.
    if (ActiveTargets.Num() > 0 && !ActiveTargets.Contains(Target))
    {
        UE_LOG(LogTargetSpawnerComponent, Error,
            TEXT("[Ownership] Phase-4 registration refused: %d target(s) already registered. "
                 "This must be impossible — the spawn gate should have blocked the spawn."),
            ActiveTargets.Num());
        return;
    }

    ActiveTargets.AddUnique(Target);
}

void UTargetSpawnerComponent::DeregisterPhase4Target(ATranquilMindTargetActor* Target)
{
    if (Target == nullptr)
    {
        return;
    }

    ActiveTargets.Remove(Target);
}

void UTargetSpawnerComponent::DestroyAllActiveTargetsAsVoid()
{
    // STAGE 2 RE-ENTRANCY: deregistration now happens at the outcome lock, so
    // ResolveAsVoid() below reaches DeregisterPhase4Target() and mutates
    // ActiveTargets while this function is iterating it. Take the array first
    // and clear the member, then resolve from the snapshot.
    TArray<TObjectPtr<ATranquilMindTargetActor>> Snapshot = MoveTemp(ActiveTargets);
    ActiveTargets.Reset();

    for (int32 Index = Snapshot.Num() - 1; Index >= 0; --Index)
    {
        ATranquilMindTargetActor* Target = Snapshot[Index].Get();

        if (IsValid(Target) && !Target->IsResolved())
        {
            Target->ResolveAsVoid();
        }
    }

    // NOTE: this function does NOT release LiveDemoPresentation. A Void starts
    // the presentation's prompt exit; ownership is released when the actor
    // actually dies at the end of Phase 9. That is precisely what stops a
    // hard-gate resume or a RestartTrial from spawning into a predecessor that
    // is still visibly fading — Stage 1 §3.3's deterministic C7 path.
}

void UTargetSpawnerComponent::HandleTriggerPulled(FVector InSmoothedGazeDirection_World)
{
    // ---- Research Mode: resolve ONLY the current active research trial ----
    if (OperatingMode == ETMOperatingMode::Research)
    {
        if (IsValid(ResearchRunner.Get()))
        {
            ResearchRunner->HandleResponse();
        }
        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr || !IsValid(SessionManager.Get()))
    {
        return;
    }

    const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

    if (CurrentPhase == ETMSessionPhase::Phase_1A_Desensitization ||
        CurrentPhase == ETMSessionPhase::Phase_III_Cooldown ||
        CurrentPhase == ETMSessionPhase::Phase_Terminated)
    {
        return;
    }

    if (CurrentPhase != ETMSessionPhase::Phase_1B_Baseline &&
        CurrentPhase != ETMSessionPhase::Phase_II_CoreTraining)
    {
        return;
    }

    const ETMInterruptType ActiveInterrupt = SessionManager->GetActiveInterrupt();

    if ((!bDebugHardwareMode && ActiveInterrupt == ETMInterruptType::HardGate_GazeLost) ||
        ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        if (bDebugHardwareMode && ActiveInterrupt == ETMInterruptType::SysAbort)
        {
            UE_LOG(LogTargetSpawnerComponent, Warning,
                TEXT("[HardwareDebug] Trigger ignored because SysAbort is active"));
        }
        return;
    }

    const float TriggerTimestamp_SEC = World->GetTimeSeconds();

    if (LastTriggerTimestamp_SEC >= 0.0f)
    {
        const float InterTrigger_MS =
            (TriggerTimestamp_SEC - LastTriggerTimestamp_SEC) * 1000.0f;

        LastTriggerTimestamp_SEC = TriggerTimestamp_SEC;

        if (InterTrigger_MS < TranquilMind::DEBOUNCE_MIN_MS)
        {
            SessionManager->ReportSpamInput();

            UE_LOG(
                LogTargetSpawnerComponent,
                Warning,
                TEXT("[TargetSpawner] Trigger rejected by debounce | Delta=%.2f ms"),
                InterTrigger_MS);

            return;
        }
    }
    else
    {
        LastTriggerTimestamp_SEC = TriggerTimestamp_SEC;
    }

    CleanupResolvedTargets();

    // ============================================================
    // TEMP DEBUG MODE
    //
    // 当前阶段先不用 Tie-Break，只验证：
    // Space / Trigger 是否能一键让场上目标立即消失。
    //
    // 正式版之后再恢复：
    // ATranquilMindTargetActor* BoundTarget =
    //     SelectTargetByTieBreak(InSmoothedGazeDirection_World);
    // BoundTarget->ResolveAsTriggered(TriggerTimestamp_SEC);
    // ============================================================

    // STAGE 2, Spec §4.2. ActiveTargets is now exactly "a presentation is in
    // Phase 4 and input is open", so this branch means the input arrived in a
    // phase that does not accept it (Entrance, Readable Hold, Outcome Lock,
    // Persistence, Exit, Hidden) or with nothing on screen at all. The input is
    // discarded HERE, at the point of receipt: it is not buffered, not queued,
    // not replayed into a later Phase 4, and not scored.
    //
    // ReportSpamInput is retained unchanged from the Stage 1 baseline. It is the
    // anti-spam watchdog, not a scoring path — it records no trial outcome and
    // produces no commission error — so §4.2's prohibition is satisfied.
    if (ActiveTargets.Num() <= 0)
    {
        SessionManager->ReportSpamInput();

        UE_LOG(
            LogTargetSpawnerComponent,
            Warning,
            TEXT("[TargetSpawner] Trigger pulled but no active targets. Reported spam."));

        return;
    }

    UE_LOG(
        LogTargetSpawnerComponent,
        Warning,
        TEXT("[TargetSpawner] DEBUG trigger pulled. Resolving ALL active targets. Count=%d"),
        ActiveTargets.Num());

    // STAGE 2 RE-ENTRANCY: ResolveAsTriggered() now reaches the outcome lock,
    // which deregisters through DeregisterPhase4Target() and mutates
    // ActiveTargets while this loop would be iterating it. Snapshot first.
    TArray<TObjectPtr<ATranquilMindTargetActor>> Snapshot = MoveTemp(ActiveTargets);
    ActiveTargets.Reset();

    for (int32 Index = Snapshot.Num() - 1; Index >= 0; --Index)
    {
        ATranquilMindTargetActor* Target = Snapshot[Index].Get();

        if (!IsValid(Target) || Target->IsResolved())
        {
            continue;
        }

        // Spec §5 steps 1-4. ResolveAsTriggered() itself refuses any input that
        // is not the first valid one in Phase 4, so the outcome and the reaction
        // time are committed exactly once even if this loop were ever to see the
        // same actor twice.
        Target->ResolveAsTriggered(TriggerTimestamp_SEC);
    }

    // STAGE 2 — the line that used to sit here re-anchored the cadence to the
    // response instant:
    //
    //     LastSpawnTimestamp_SEC = TriggerTimestamp_SEC;
    //
    // That made the successor spawn at RT + 3000 ms after the predecessor's
    // spawn, i.e. it made CADENCE A FUNCTION OF REACTION SPEED. Spec §3 defines
    // Cadence as PresentationSpawnTime(n+1) - PresentationSpawnTime(n), and §5
    // step 8 requires it to be independent of reaction speed. The cadence is now
    // anchored solely at PresentationSpawnTime in SpawnNextTarget().
    //
    // Its original intent — "let the eye see that it really disappeared" — is
    // now served properly: after an early accepted response the presentation
    // still runs its full scheduled visual lifetime (§5 steps 6-7).
}

void UTargetSpawnerComponent::RegisterSessionManager(ATranquilMindSessionManager* InSessionManager)
{
    if (IsValid(SessionManager.Get()))
    {
        SessionManager->OnStaircaseParamsUpdated.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::ApplyStaircaseParams);

        SessionManager->OnHardGateResumed.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleHardGateResumed);

        SessionManager->OnInterruptStateChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleInterruptStateChanged);

        SessionManager->OnPhaseChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandlePhaseChanged);
    }

    SessionManager = InSessionManager;

    if (!IsValid(SessionManager.Get()))
    {
        return;
    }

    SessionManager->OnStaircaseParamsUpdated.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::ApplyStaircaseParams);

    SessionManager->OnHardGateResumed.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::HandleHardGateResumed);

    SessionManager->OnInterruptStateChanged.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::HandleInterruptStateChanged);

    SessionManager->OnPhaseChanged.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::HandlePhaseChanged);

    if (bDemoMode)
    {
        SessionManager->bBypassHardGateSuspend = true;
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[DemoMode] HardGate suspend watchdog bypassed"));
    }

    const FSessionStats SessionStats = SessionManager->GetSessionStats();
    ApplyStaircaseParams(SessionStats.Current_ISI_MS, SessionStats.Current_NoiseAlpha);

    if (SessionManager->GetActiveInterrupt() == ETMInterruptType::HardGate_GazeLost)
    {
        DestroyAllActiveTargetsAsVoid();
    }

    UE_LOG(
        LogTargetSpawnerComponent,
        Log,
        TEXT("[TargetSpawner] Registered SessionManager"));
}

void UTargetSpawnerComponent::SpawnNextTarget()
{
    // RESEARCH INTEGRITY GUARD (defence in depth).
    // Independent of the RestartTrial() guard on purpose: this is the single
    // site that constructs a Demo target, and it is BlueprintCallable, so it
    // must refuse on its own rather than trusting every caller to have checked.
    if (OperatingMode == ETMOperatingMode::Research)
    {
        LogResearchSuppressionOnce(TEXT("SpawnNextTarget"));
        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr || !CanSpawnTargetsNow())
    {
        return;
    }

    CleanupResolvedTargets();

    // ============================================================
    //  STAGE 2 SPAWN GATE — Spec §4.9
    //
    //  This replaces the previous gate:
    //
    //      if (ActiveTargets.Num() > 0) { return; }
    //
    //  Stage 1 §3.3 proved that gate is the root of C7. ActiveTargets is a
    //  SCORING-ELIGIBILITY set: the predecessor is deregistered at resolution
    //  and then keeps rendering for the whole 350 ms exit. A gate reading
    //  registration rather than visibility therefore opens ~350 ms early, and
    //  the trace measured exactly that — visiblepresentations reached 2 while
    //  activetargets stayed 1, with the predecessor fading from EdgeOpacity
    //  0.577 to 0.000 while fully co-visible with its successor.
    //
    //  LiveDemoPresentation is a PRESENTATION set and is held for the full
    //  visual lifetime, so a successor cannot become visible while the
    //  predecessor official presentation is still alive.
    // ============================================================
    if (HasLiveDemoPresentation())
    {
        // Deliberately does NOT touch LastSpawnTimestamp_SEC. A latched
        // first-spawn request (LastSpawnTimestamp_SEC < 0, set by a phase
        // change, a hard-gate resume or RestartTrial) stays armed and the tick
        // loop retries every frame, so the spawn happens as soon as the
        // predecessor's presentation actually ends. The gate delays a spawn; it
        // can never cancel one.
        return;
    }

    const float SpawnTimestamp_SEC = World->GetTimeSeconds();

    if (TargetActorClass.Get() == nullptr)
    {
        LastSpawnTimestamp_SEC = SpawnTimestamp_SEC;

        // STAGE 2 (Spec §4.9.1, "failed spawn"): no presentation was created, so
        // there is nothing to own — and the pending request must be cleared or
        // it leaks true forever. The frame is already captured by this point, so
        // no further OnDemoFrameCaptured broadcast will ever arrive to consume
        // it. Clearing here keeps the ordinary ISI cadence as the retry path and
        // guarantees a failed spawn cannot permanently block future spawning.
        bDemoSpawnPendingFrame = false;
        bLoggedPendingSpawnWait = false;

        UE_LOG(
            LogTargetSpawnerComponent,
            Warning,
            TEXT("[TargetSpawner] TargetActorClass is null. Spawn skipped."));

        return;
    }

    // Reentrancy guard for the pending-spawn handshake: if EnsureCaptured()
    // below latches the frame synchronously, its broadcast fires while this
    // call is still on the stack. The handler sees this flag and yields — the
    // in-flight call completes the request itself. TGuardValue restores the
    // flag on every return path.
    TGuardValue<bool> SpawnReentrancyGuard(bSpawnCallInProgress, true);

    // ---- Pending-spawn exactly-once handshake -------------------------------
    // Resolve frame availability BEFORE ChooseNextStimulusType(): a deferred
    // attempt must not consume a stimulus draw (alternation step or RNG), or
    // every coalesced retry would perturb the Go/NoGo sequence.
    UTMDemoSessionFrameSubsystem* Frame =
        World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
    const bool bFrameReady = Frame != nullptr && Frame->EnsureCaptured();

    if (Frame != nullptr && !bFrameReady)
    {
        // XR is present but its pose is still initializing (the only state in
        // which EnsureCaptured answers false). Record ONE coalesced request
        // and construct nothing — no target, no stimulus draw, no timestamp,
        // so the tick loop's first-spawn retry stays armed as a backstop.
        if (!bDemoSpawnPendingFrame)
        {
            bDemoSpawnPendingFrame = true;

            if (!bLoggedPendingSpawnWait)
            {
                bLoggedPendingSpawnWait = true;
                UE_LOG(LogTargetSpawnerComponent, Warning,
                    TEXT("[TargetSpawner] Demo frame awaiting first XR pose — "
                         "spawn request pended (coalescing further requests)."));
            }
        }

        return;
    }

    const ETMStimulusType NextStimulusType = ChooseNextStimulusType();

    // Spawn 200 cm along the captured Demo session forward. The frame is
    // captured lazily at the first spawn — the moment the Demo session
    // genuinely starts producing content.
    FVector SpawnLocation(
        TranquilMindSpawner::DebugSpawnX_CM,
        TranquilMindSpawner::DebugSpawnY_CM,
        TranquilMindSpawner::DebugSpawnZ_CM);

    const float SpawnDist_CM = DemoMode_SpawnDistance_CM;
    bool bSpawnLocationResolved = false;
    FVector CameraAtSpawn = FVector::ZeroVector;
    bool bHaveCameraAtSpawn = false;

    if (bFrameReady)
    {
        // Active bubble: distance from the CURRENT camera, heading from the
        // IMMUTABLE session orientation. Leaning or walking cannot invalidate
        // the comfort distance; head rotation cannot swing the target.
        CameraAtSpawn = Frame->GetCurrentCameraLocationOrOrigin();
        bHaveCameraAtSpawn = true;
        SpawnLocation = Frame->DemoActiveSpawnPointFrom(CameraAtSpawn, SpawnDist_CM, 0.0f, 0.0f);
        bSpawnLocationResolved = true;
    }
    else
    {
        // No subsystem at all (should not occur in Demo): legacy path below.
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[TargetSpawner] Demo session frame unavailable — using legacy "
                 "live-camera spawn path."));
    }

    if (bSpawnLocationResolved)
    {
        // Frame path resolved; skip the legacy live-camera resolution below.
    }
    else if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (APawn* ControlledPawn = PC->GetPawn())
        {
            UCameraComponent* Cam =
                ControlledPawn->FindComponentByClass<UCameraComponent>();
            if (IsValid(Cam))
            {
                const FVector CamLoc = Cam->GetComponentLocation();
                const FVector CamFwd = Cam->GetForwardVector();
                CameraAtSpawn = CamLoc;
                bHaveCameraAtSpawn = true;
                SpawnLocation = CamLoc + CamFwd * SpawnDist_CM;

                UE_LOG(LogTargetSpawnerComponent, Warning,
                    TEXT("[TargetSpawner] CamLoc=(%.1f,%.1f,%.1f) Fwd=(%.2f,%.2f,%.2f)"
                         " SpawnAt=(%.1f,%.1f,%.1f) Dist=%.0fcm"),
                    CamLoc.X, CamLoc.Y, CamLoc.Z,
                    CamFwd.X, CamFwd.Y, CamFwd.Z,
                    SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z,
                    SpawnDist_CM);
            }
            else
            {
                UE_LOG(LogTargetSpawnerComponent, Warning,
                    TEXT("[TargetSpawner] No UCameraComponent on pawn — using hardcoded spawn."));
            }
        }
        else
        {
            UE_LOG(LogTargetSpawnerComponent, Warning,
                TEXT("[TargetSpawner] No pawn possessed — using hardcoded spawn."));
        }
    }
    else
    {
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[TargetSpawner] No PlayerController — using hardcoded spawn."));
    }

    const FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = GetOwner();
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Kit §4 site 14 — the spawn decision. Everything above this point can
    // still bail out (Research guard, missing class, pended frame), so this is
    // the first instant the next target is genuinely committed to. prev_auid
    // comes from the tracer's own record of the last SPAWN, so no new
    // gameplay-visible state is introduced (deviation D4).
    FTMStage1Trace::LogNextSpawn(World, FTMStage1Trace::LastSpawnedAuid());

    ATranquilMindTargetActor* SpawnedTarget =
        World->SpawnActor<ATranquilMindTargetActor>(
            TargetActorClass.Get(),
            SpawnLocation,
            SpawnRotation,
            SpawnParameters);

    LastSpawnTimestamp_SEC = SpawnTimestamp_SEC;

    if (!IsValid(SpawnedTarget))
    {
        // STAGE 2 (Spec §4.9.1, "failed spawn"): ownership is claimed below and
        // only on success, so nothing was claimed here. Clear the pending
        // request for the same reason as the null-class path above — a failed
        // spawn must not permanently block future spawning.
        bDemoSpawnPendingFrame = false;
        bLoggedPendingSpawnWait = false;

        UE_LOG(
            LogTargetSpawnerComponent,
            Warning,
            TEXT("[TargetSpawner] SpawnActor failed."));

        return;
    }

    // ---- STAGE 2: claim the official presentation (Spec §4.9, Phase 1) -------
    // Claimed at presentation CREATION, before initialization, and held until
    // the actor actually dies at the end of Phase 9.
    LiveDemoPresentation = SpawnedTarget;
    SpawnedTarget->SetOwningSpawner(this);

    SpawnedTarget->InitializeTarget(
        NextStimulusType,
        SpawnTimestamp_SEC,
        SessionManager.Get());

    if (bDemoMode)
    {
        SpawnedTarget->ResponseWindow_MS = DemoMode_ResponseWindow_MS;

        // Pass 1A: presentation scale, applied AFTER InitializeTarget so it
        // supersedes the actor's internal TEMP scale. Purely visual (audited:
        // no gameplay path reads mesh bounds); Research never runs this block.
        SpawnedTarget->SetActorScale3D(FVector(DemoMode_TargetScale));

        // Pass 1A.1: approach speed, likewise pushed after InitializeTarget so
        // it supersedes the actor's TEMP 50 cm/s. Research is unaffected —
        // InitializeResearchVisual zeroes MovementSpeed and this block only
        // runs on the Demo path.
        SpawnedTarget->MovementSpeed_CMPerSec = DemoMode_ApproachSpeed_CMPerSec;

        // ---- Comfort-distance diagnostics (Pass 1A/1A.1 acceptance evidence).
        // All values read back from the ACTOR after every push, so this line is
        // proof of what the target actually received, not of what was intended.
        float UnscaledMeshRadius_CM = 50.0f;
        if (SpawnedTarget->MeshComponent != nullptr &&
            SpawnedTarget->MeshComponent->GetStaticMesh() != nullptr)
        {
            UnscaledMeshRadius_CM =
                SpawnedTarget->MeshComponent->GetStaticMesh()->GetBounds().SphereRadius;
        }

        const float EffectiveSpeed_CMPerSec =
            FMath::Abs(SpawnedTarget->MovementSpeed_CMPerSec);
        const float VisualRadius_CM =
            UnscaledMeshRadius_CM * SpawnedTarget->GetActorScale3D().X;
        const float TravelDuringWindow_CM =
            EffectiveSpeed_CMPerSec * (SpawnedTarget->ResponseWindow_MS / 1000.0f);
        const float PredictedEndCentre_CM = SpawnDist_CM - TravelDuringWindow_CM;
        const float PredictedNearestSurface_CM = PredictedEndCentre_CM - VisualRadius_CM;

        auto AngularDiameterDeg = [VisualRadius_CM](float CentreDist_CM) -> float
        {
            if (CentreDist_CM <= VisualRadius_CM)
            {
                return 180.0f;   // inside the sphere
            }
            return 2.0f * FMath::RadiansToDegrees(
                FMath::Asin(VisualRadius_CM / CentreDist_CM));
        };

        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[DemoPresentation] CamAtSpawn=(%.1f, %.1f, %.1f)%s | SpawnDist=%.1f cm | "
                 "Speed=%.1f cm/s | Travel=%.1f cm | MeshRadius=%.2f cm | Scale=%.2f | "
                 "VisualRadius=%.2f cm | PredEndCentre=%.1f cm | PredNearestSurface=%.1f cm | "
                 "AngDiam=%.2f deg -> %.2f deg"),
            CameraAtSpawn.X, CameraAtSpawn.Y, CameraAtSpawn.Z,
            bHaveCameraAtSpawn ? TEXT("") : TEXT(" [UNRESOLVED]"),
            SpawnDist_CM,
            EffectiveSpeed_CMPerSec, TravelDuringWindow_CM,
            UnscaledMeshRadius_CM, SpawnedTarget->GetActorScale3D().X,
            VisualRadius_CM, PredictedEndCentre_CM, PredictedNearestSurface_CM,
            AngularDiameterDeg(SpawnDist_CM), AngularDiameterDeg(PredictedEndCentre_CM));

        // ---- STAGE 2: arm the nine-phase presentation lifecycle -------------
        // Armed HERE, after every Demo override has been pushed, so the phase
        // machine reads the EFFECTIVE response window rather than the actor's
        // transient TEMP default. This is inside the bDemoMode block and behind
        // the operating-mode guard at the top of this function, so the Research
        // path can never reach it (R1-R7).
        //
        // Timing is the Spec §4.7 "Stage 2 Compatibility" column, normative for
        // Stage 2: Entrance 0 / Readable Hold 0 / Scored Response 2500 /
        // Persistence 0 / Exit 350, against a 3000 ms cadence. NOT A-prime.
        FTMDemoLifecycleTiming Timing;
        Timing.Entrance_MS       = 0.0f;
        Timing.ReadableHold_MS   = 0.0f;
        Timing.ScoredResponse_MS = SpawnedTarget->ResponseWindow_MS;
        Timing.Persistence_MS    = 0.0f;
        Timing.Exit_MS = IsValid(SpawnedTarget->VisualMotion)
            ? SpawnedTarget->VisualMotion->MotionParams.ExitDuration_SEC * 1000.0f
            : 350.0f;

        // Registers the target in ActiveTargets on the way into Phase 4. Under
        // the Compatibility timing Entrance and Readable Hold are zero-duration,
        // so this reaches Phase 4 synchronously and registration lands on the
        // spawn frame exactly as ActiveTargets.Add() used to.
        SpawnedTarget->ArmDemoPresentationLifecycle(SpawnTimestamp_SEC, Timing);
    }
    else
    {
        // Non-Demo-mode Demo path (bDemoMode disabled): no phase machine is
        // armed, so registration keeps its pre-Stage-2 form.
        RegisterPhase4Target(SpawnedTarget);
    }

    // This spawn services any pending request (clear-with-spawn: exactly once).
    bDemoSpawnPendingFrame = false;
    bLoggedPendingSpawnWait = false;

    UE_LOG(
        LogTargetSpawnerComponent,
        Warning,
        TEXT("[TargetSpawner] Spawned ONE target | Type=%d | Active=%d | Location=(%.1f, %.1f, %.1f)"),
        static_cast<int32>(NextStimulusType),
        ActiveTargets.Num(),
        SpawnLocation.X,
        SpawnLocation.Y,
        SpawnLocation.Z);

    if (bDebugHardwareMode)
    {
        const float DisplayISI_MS = bDemoMode ? DemoMode_ISI_MS : DebugHardware_ISI_MS;
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[HardwareDebug] Spawned %s | Next in %.1f s"),
            (NextStimulusType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
            DisplayISI_MS / 1000.0f);
    }
}

void UTargetSpawnerComponent::CleanupResolvedTargets()
{
    for (int32 Index = ActiveTargets.Num() - 1; Index >= 0; --Index)
    {
        ATranquilMindTargetActor* Target = ActiveTargets[Index].Get();

        if (!IsValid(Target) || Target->IsResolved())
        {
            ActiveTargets.RemoveAtSwap(Index, 1, false);
        }
    }
}

void UTargetSpawnerComponent::HandleInterruptStateChanged(
    ETMInterruptType InterruptType,
    bool bActive)
{
    if (!bActive)
    {
        return;
    }

    if (InterruptType == ETMInterruptType::HardGate_GazeLost ||
        InterruptType == ETMInterruptType::SysAbort)
    {
        DestroyAllActiveTargetsAsVoid();
        LastSpawnTimestamp_SEC = -1.0f;
    }
}

void UTargetSpawnerComponent::HandlePhaseChanged(ETMSessionPhase NewPhase)
{
    if (NewPhase == ETMSessionPhase::Phase_1B_Baseline ||
        NewPhase == ETMSessionPhase::Phase_II_CoreTraining)
    {
        LastSpawnTimestamp_SEC = -1.0f;
        return;
    }

    if (NewPhase == ETMSessionPhase::Phase_1A_Desensitization ||
        NewPhase == ETMSessionPhase::Phase_III_Cooldown ||
        NewPhase == ETMSessionPhase::Phase_Terminated)
    {
        DestroyAllActiveTargetsAsVoid();
        LastSpawnTimestamp_SEC = -1.0f;
    }
}

void UTargetSpawnerComponent::HandleHardGateResumed()
{
    RestartTrial();
}

void UTargetSpawnerComponent::HandleDemoFrameCaptured()
{
    // Belt-and-braces: only ever subscribed in Demo, but refuse regardless.
    if (OperatingMode == ETMOperatingMode::Research)
    {
        return;
    }

    if (!bDemoSpawnPendingFrame)
    {
        return;
    }

    // If the latch happened inside an in-flight SpawnNextTarget(), that call
    // completes the request itself with the now-valid frame — recursing here
    // would construct a second target.
    if (bSpawnCallInProgress)
    {
        return;
    }

    // Consume exactly once: clear BEFORE spawning, per the handshake contract.
    bDemoSpawnPendingFrame = false;

    UE_LOG(LogTargetSpawnerComponent, Warning,
        TEXT("[TargetSpawner] Demo frame captured — consuming pended spawn request."));

    SpawnNextTarget();
}

void UTargetSpawnerComponent::LogResearchSuppressionOnce(const TCHAR* Context)
{
    // A Research-mode suppression is expected and benign — every gaze hard-gate
    // resume reaches these entry points. Surface the first one at Warning so it
    // is visible that the guard is doing work, then drop to Verbose so a long
    // Research session does not fill the researcher's log with repeats.
    if (!bLoggedResearchSuppression)
    {
        bLoggedResearchSuppression = true;

        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[Mode] %s suppressed: OperatingMode is Research. "
                 "UTMResearchRunner owns the Research trial lifecycle. "
                 "Further suppressions this session are logged at Verbose."),
            Context);

        return;
    }

    UE_LOG(LogTargetSpawnerComponent, Verbose,
        TEXT("[Mode] %s suppressed: OperatingMode is Research."), Context);
}

bool UTargetSpawnerComponent::CanSpawnTargetsNow() const
{
    if (!IsValid(SessionManager.Get()))
    {
        return false;
    }

    const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

    if (CurrentPhase != ETMSessionPhase::Phase_1B_Baseline &&
        CurrentPhase != ETMSessionPhase::Phase_II_CoreTraining)
    {
        return false;
    }

    const ETMInterruptType ActiveInterrupt = SessionManager->GetActiveInterrupt();

    if (ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        return false;
    }

    if (!bDebugHardwareMode &&
        ActiveInterrupt == ETMInterruptType::HardGate_GazeLost)
    {
        return false;
    }

    return true;
}

ETMStimulusType UTargetSpawnerComponent::ChooseNextStimulusType()
{
    if (!IsValid(SessionManager.Get()))
    {
        return ETMStimulusType::Go;
    }

    if (bDebugHardwareMode)
    {
        return (DebugAlternateTypeCounter++ % 2 == 0)
            ? ETMStimulusType::Go
            : ETMStimulusType::NoGo;
    }

    const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

    if (CurrentPhase == ETMSessionPhase::Phase_1B_Baseline)
    {
        return ETMStimulusType::Go;
    }

    if (CurrentPhase == ETMSessionPhase::Phase_II_CoreTraining)
    {
        const float Sample = StimulusRandomStream.FRand();

        if (Sample < TranquilMindSpawner::GoProbability)
        {
            return ETMStimulusType::Go;
        }

        return ETMStimulusType::NoGo;
    }

    return ETMStimulusType::Go;
}

ATranquilMindTargetActor* UTargetSpawnerComponent::SelectTargetByTieBreak(
    const FVector& InSmoothedGazeDirection_World) const
{
    ATranquilMindTargetActor* BestTarget = nullptr;
    float BestDepth_CM = 0.0f;
    float BestGazeAngle_DEG = 0.0f;
    float BestSpawnTimestamp_SEC = 0.0f;

    for (int32 Index = 0; Index < ActiveTargets.Num(); ++Index)
    {
        ATranquilMindTargetActor* CandidateTarget = ActiveTargets[Index].Get();

        if (!IsValid(CandidateTarget) ||
            CandidateTarget->IsResolved() ||
            CandidateTarget->HasHadResponse())
        {
            continue;
        }

        const float CandidateDepth_CM = CandidateTarget->GetDepthCM();
        const float CandidateGazeAngle_DEG =
            ComputeGazeAngleToTargetDEG(CandidateTarget, InSmoothedGazeDirection_World);
        const float CandidateSpawnTimestamp_SEC = CandidateTarget->GetSpawnTimestampSEC();

        bool bCandidateWins = false;

        if (BestTarget == nullptr)
        {
            bCandidateWins = true;
        }
        else
        {
            const float DepthDelta_CM = FMath::Abs(CandidateDepth_CM - BestDepth_CM);

            if (DepthDelta_CM > TranquilMind::TIEBREAK_DEPTH_DELTA_CM)
            {
                bCandidateWins = CandidateDepth_CM < BestDepth_CM;
            }
            else
            {
                const float GazeAngleDelta_DEG =
                    FMath::Abs(CandidateGazeAngle_DEG - BestGazeAngle_DEG);

                if (GazeAngleDelta_DEG > TranquilMind::TIEBREAK_GAZE_DELTA_DEG)
                {
                    bCandidateWins = CandidateGazeAngle_DEG < BestGazeAngle_DEG;
                }
                else
                {
                    bCandidateWins = CandidateSpawnTimestamp_SEC < BestSpawnTimestamp_SEC;
                }
            }
        }

        if (bCandidateWins)
        {
            BestTarget = CandidateTarget;
            BestDepth_CM = CandidateDepth_CM;
            BestGazeAngle_DEG = CandidateGazeAngle_DEG;
            BestSpawnTimestamp_SEC = CandidateSpawnTimestamp_SEC;
        }
    }

    return BestTarget;
}

float UTargetSpawnerComponent::ComputeGazeAngleToTargetDEG(
    const ATranquilMindTargetActor* Target,
    const FVector& InSmoothedGazeDirection_World) const
{
    if (!IsValid(Target))
    {
        return 180.0f;
    }

    const AActor* OwnerActor = GetOwner();

    const FVector GazeOrigin =
        OwnerActor != nullptr ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

    const FVector GazeDirection = InSmoothedGazeDirection_World.GetSafeNormal();

    if (GazeDirection.IsNearlyZero())
    {
        return 180.0f;
    }

    const FVector TargetDirection =
        (Target->GetTargetCenterWorld() - GazeOrigin).GetSafeNormal();

    if (TargetDirection.IsNearlyZero())
    {
        return 180.0f;
    }

    const float Dot =
        FMath::Clamp(FVector::DotProduct(GazeDirection, TargetDirection), -1.0f, 1.0f);

    return FMath::RadiansToDegrees(FMath::Acos(Dot));
}
