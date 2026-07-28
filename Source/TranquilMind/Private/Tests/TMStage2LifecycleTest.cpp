// TranquilMind Stage 2 — lifecycle, input gating and presentation ownership.
//
// Covers the Step F requirement list 1-18 against
// TranquilMind_Implementation_Spec.md §4.1 (nine phases), §4.2 (input rule),
// §4.3 (target-array rule), §4.9 (presentation ownership) and §5 (early-response
// semantics), at the §4.7 Stage 2 Compatibility timing.
//
// HOW THE ZERO-DURATION PHASES ARE TESTED
//
//   Under Compatibility timing Entrance, Readable Hold and Persistence are 0 ms,
//   so they cannot be sampled by advancing a clock. FTMDemoLifecycleTiming is
//   data, so these tests arm the machine with non-zero durations to prove the
//   states exist, are traversed in order, and discard input. Production timing
//   is untouched — the spawner always arms 0 / 0 / 2500 / 0 / 350.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

#include "TMDemoLifecycleComponent.h"
#include "TMDemoSessionFrame.h"
#include "TMVisualMotionComponent.h"
#include "TargetSpawnerComponent.h"
#include "../../TranquilMindSessionManager.h"
#include "../../TranquilMindTypes.h"
#include "../../Public/Private/Private/TranquilMindTargetActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMStage2TestUtil
{
    constexpr float Step_SEC = 1.0f / 72.0f;   // the Quest 2 baseline frame rate

    struct FScopedWorld
    {
        UWorld* World = nullptr;

        explicit FScopedWorld(ETMOperatingMode Mode)
        {
            if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                    TEXT("tranquilmind.OperatingMode")))
            {
                CVar->Set(Mode == ETMOperatingMode::Research ? 1 : 0, ECVF_SetByCode);
            }

            World = UWorld::CreateWorld(EWorldType::Game, false);
            FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
            Ctx.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->SetBegunPlay(true);
        }

        ~FScopedWorld()
        {
            if (World != nullptr)
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
            }

            if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                    TEXT("tranquilmind.OperatingMode")))
            {
                CVar->Set(-1, ECVF_SetByCode);
            }
        }

        int32 CountTargets() const
        {
            int32 Count = 0;
            for (TActorIterator<ATranquilMindTargetActor> It(World); It; ++It)
            {
                if (IsValid(*It))
                {
                    ++Count;
                }
            }
            return Count;
        }
    };

    void BuildRuntimeInPhaseII(
        FScopedWorld& W,
        ATranquilMindSessionManager*& OutSM,
        UTargetSpawnerComponent*& OutSpawner)
    {
        OutSM = W.World->SpawnActor<ATranquilMindSessionManager>();

        AActor* Host = W.World->SpawnActor<AActor>();
        OutSpawner = NewObject<UTargetSpawnerComponent>(Host);
        OutSpawner->RegisterComponent();
        OutSpawner->RegisterSessionManager(OutSM);

        for (int32 i = 0; i < 800; ++i)
        {
            W.World->DeltaTimeSeconds = 1.0f / 60.0f;
            OutSM->UpdateGazeVector(FVector(1.0f, 0.0f, 0.0f));
            OutSM->Tick(1.0f / 60.0f);
        }
    }

    /** Latch the Demo session frame so SpawnNextTarget() does not pend. */
    void CaptureFrame(FScopedWorld& W)
    {
        UTMDemoSessionFrameSubsystem* Frame =
            W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
        if (Frame != nullptr)
        {
            Frame->CaptureFromPose(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f));
        }
    }

    /**
     * Advance world time and drive the two component ticks a target relies on.
     * A scratch world does not register component tick functions, so they are
     * ticked explicitly — the same technique the existing Stage 1 tests use.
     */
    void Advance(FScopedWorld& W, ATranquilMindTargetActor* Target, float Seconds)
    {
        const int32 Steps = FMath::CeilToInt(Seconds / Step_SEC);
        for (int32 i = 0; i < Steps; ++i)
        {
            W.World->TimeSeconds += Step_SEC;
            W.World->DeltaTimeSeconds = Step_SEC;

            if (!IsValid(Target))
            {
                continue;
            }

            if (IsValid(Target->GetDemoLifecycle()))
            {
                Target->GetDemoLifecycle()->TickComponent(Step_SEC, LEVELTICK_All, nullptr);
            }

            if (IsValid(Target) && IsValid(Target->VisualMotion))
            {
                Target->VisualMotion->TickComponent(Step_SEC, LEVELTICK_All, nullptr);
            }
        }
    }

    /**
     * Spawn one Demo target through the real spawner path, arm the Environment
     * presentation rig (scratch worlds have no map name, so the profile gate
     * that arms it in production cannot fire), and optionally re-arm the
     * lifecycle with custom phase durations.
     */
    ATranquilMindTargetActor* SpawnArmed(
        FScopedWorld& W,
        UTargetSpawnerComponent* Spawner,
        bool bWithExitEnvelope = true)
    {
        Spawner->SpawnNextTarget();
        if (Spawner->ActiveTargets.Num() == 0)
        {
            return nullptr;
        }

        ATranquilMindTargetActor* Target = Spawner->ActiveTargets[0].Get();
        if (bWithExitEnvelope && IsValid(Target) && IsValid(Target->VisualMotion))
        {
            Target->VisualMotion->BeginMotion(7);
        }
        return Target;
    }
}

// ============================================================
//  (1)(3)(4)(5)(6) Early response: scored once, visual lifetime unchanged
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2EarlyResponseTest,
    "TranquilMind.Demo.Stage2.EarlyResponseDoesNotShortenLifetime",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2EarlyResponseTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);
    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);
    CaptureFrame(W);

    W.World->TimeSeconds = 100.0;
    ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
    TestNotNull(TEXT("target spawned"), Target);

    UTMDemoLifecycleComponent* Life = Target->GetDemoLifecycle();
    TestTrue(TEXT("lifecycle armed by the Demo spawn path"), Life->IsArmed());

    // Compatibility timing: Entrance and Readable Hold are zero-duration, so the
    // presentation is already in Phase 4 on the spawn frame.
    TestEqual(TEXT("(7) Phase 4 entered synchronously at spawn"),
        Life->GetPhase(), ETMDemoPresentationPhase::ApproachResponse);
    TestTrue(TEXT("(1) input open in Phase 4"), Life->IsInputOpen());

    const float ScheduledEnd = Life->GetScheduledPhase4End_SEC();
    TestEqual(TEXT("ScheduledPhase4End is spawn + 2500 ms"),
        ScheduledEnd, 100.0f + 2.5f, 0.001f);

    // Respond EARLY, 500 ms in.
    Advance(W, Target, 0.5f);
    const FBlockStats Before = SM->GetCurrentBlockStats();
    Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));
    const FBlockStats AfterFirst = SM->GetCurrentBlockStats();

    // (3) Committed exactly once.
    TestEqual(TEXT("(3) exactly one trial recorded by the first input"),
        AfterFirst.Trial_Count_Total, Before.Trial_Count_Total + 1);
    TestTrue(TEXT("(3) outcome committed"), Life->HasOutcomeCommitted());

    // (1)+(C) Input closes immediately; (4) later input cannot alter the outcome.
    TestFalse(TEXT("(1) input closed at the outcome lock"), Life->IsInputOpen());
    TestEqual(TEXT("(7) actor removed from ActiveTargets immediately"),
        Spawner->ActiveTargets.Num(), 0);
    TestEqual(TEXT("phase is Outcome Lock"),
        Life->GetPhase(), ETMDemoPresentationPhase::OutcomeLock);

    Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));
    Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));
    const FBlockStats AfterLater = SM->GetCurrentBlockStats();
    TestEqual(TEXT("(4) later responses record no further trial"),
        AfterLater.Trial_Count_Total, AfterFirst.Trial_Count_Total);
    TestEqual(TEXT("(4) later responses add no Hit"),
        AfterLater.N_Hit, AfterFirst.N_Hit);
    TestEqual(TEXT("(4) later responses add no Commission"),
        AfterLater.N_Commission, AfterFirst.N_Commission);

    // (5) The presentation must NOT have shortened. It is still in Outcome Lock
    // 1500 ms after the response, i.e. before ScheduledPhase4End.
    Advance(W, Target, 1.5f);
    TestTrue(TEXT("(5) presentation still alive 2000 ms after spawn"), IsValid(Target));
    TestEqual(TEXT("(5) still in Outcome Lock — exit has NOT begun"),
        Life->GetPhase(), ETMDemoPresentationPhase::OutcomeLock);
    TestFalse(TEXT("(5) exit envelope not running before the schedule"),
        Target->VisualMotion->IsVisualExitActive());

    // Cross ScheduledPhase4End one frame at a time and record the world time on
    // the frame the exit actually begins.
    float ExitBeganAt = -1.0f;
    for (int32 i = 0; i < 120 && IsValid(Target); ++i)
    {
        Advance(W, Target, Step_SEC);
        if (IsValid(Target) && Life->GetPhase() == ETMDemoPresentationPhase::Exit)
        {
            ExitBeganAt = W.World->TimeSeconds;
            break;
        }
    }

    TestTrue(TEXT("(5) still alive at the scheduled exit"), IsValid(Target));
    TestEqual(TEXT("(5) exit begins at the SCHEDULED time, not the response time"),
        Life->GetPhase(), ETMDemoPresentationPhase::Exit);
    TestTrue(TEXT("(5) exit envelope now running"),
        Target->VisualMotion->IsVisualExitActive());

    TestTrue(
        FString::Printf(
            TEXT("(5) exit began at ScheduledPhase4End (%.4f s), not at RT + epsilon "
                 "(observed %.4f s)"),
            ScheduledEnd, ExitBeganAt),
        ExitBeganAt > 0.0f && FMath::Abs(ExitBeganAt - ScheduledEnd) <= Step_SEC * 1.5f);

    // Full scheduled visual lifetime = 2500 + 350 = 2850 ms, independent of RT.
    TestEqual(TEXT("(5) scheduled visual lifetime is 2850 ms"),
        Life->GetScheduledVisualLifetime_MS(), 2850.0f, 1.0f);

    Advance(W, Target, 0.40f);
    TestFalse(TEXT("(5) destroyed after the scheduled exit completes"), IsValid(Target));
    TestFalse(TEXT("ownership released at Phase 9"), Spawner->HasLiveDemoPresentation());

    return true;
}

// ============================================================
//  (6) Natural expiry and early response converge on the same exit time
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2ConvergenceTest,
    "TranquilMind.Demo.Stage2.ExpiryAndEarlyResponseConverge",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2ConvergenceTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    // --- Run A: natural expiry, no input at all ---------------------------
    float ExpiryExitOffset = -1.0f;
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        ATranquilMindSessionManager* SM = nullptr;
        UTargetSpawnerComponent* Spawner = nullptr;
        BuildRuntimeInPhaseII(W, SM, Spawner);
        CaptureFrame(W);

        W.World->TimeSeconds = 200.0;
        ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
        TestNotNull(TEXT("A: target spawned"), Target);

        UTMDemoLifecycleComponent* Life = Target->GetDemoLifecycle();
        const float SpawnTime = Life->GetPresentationSpawnTime_SEC();

        for (int32 i = 0; i < 400 && IsValid(Target); ++i)
        {
            Advance(W, Target, Step_SEC);
            if (IsValid(Target) && Life->GetPhase() == ETMDemoPresentationPhase::Exit)
            {
                ExpiryExitOffset = W.World->TimeSeconds - SpawnTime;
                break;
            }
        }

        TestTrue(TEXT("A: natural expiry reached the exit"), ExpiryExitOffset > 0.0f);
        const FBlockStats S = SM->GetCurrentBlockStats();
        TestEqual(TEXT("A: natural expiry scored exactly one trial"),
            S.Trial_Count_Total, 1);
    }

    // --- Run B: early accepted response at 300 ms -------------------------
    float EarlyExitOffset = -1.0f;
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        ATranquilMindSessionManager* SM = nullptr;
        UTargetSpawnerComponent* Spawner = nullptr;
        BuildRuntimeInPhaseII(W, SM, Spawner);
        CaptureFrame(W);

        W.World->TimeSeconds = 200.0;
        ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
        TestNotNull(TEXT("B: target spawned"), Target);

        UTMDemoLifecycleComponent* Life = Target->GetDemoLifecycle();
        const float SpawnTime = Life->GetPresentationSpawnTime_SEC();

        Advance(W, Target, 0.30f);
        Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));

        for (int32 i = 0; i < 400 && IsValid(Target); ++i)
        {
            Advance(W, Target, Step_SEC);
            if (IsValid(Target) && Life->GetPhase() == ETMDemoPresentationPhase::Exit)
            {
                EarlyExitOffset = W.World->TimeSeconds - SpawnTime;
                break;
            }
        }

        TestTrue(TEXT("B: early response reached the exit"), EarlyExitOffset > 0.0f);
    }

    // (6) Same scheduled exit path — within one 72 Hz frame of quantisation.
    TestTrue(
        FString::Printf(
            TEXT("(6) expiry (%.4f s) and early response (%.4f s) converge on the "
                 "same scheduled exit"),
            ExpiryExitOffset, EarlyExitOffset),
        FMath::Abs(ExpiryExitOffset - EarlyExitOffset) <= Step_SEC * 1.5f);

    TestTrue(TEXT("(6) both exit at ScheduledPhase4End = 2500 ms"),
        FMath::Abs(ExpiryExitOffset - 2.5f) <= Step_SEC * 1.5f);

    return true;
}

// ============================================================
//  (2) Input discarded in Phases 2, 3, 6 and 7
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2InputGatingTest,
    "TranquilMind.Demo.Stage2.InputDiscardedOutsidePhase4",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2InputGatingTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);
    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);
    CaptureFrame(W);

    W.World->TimeSeconds = 300.0;
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("one target spawned"), Spawner->ActiveTargets.Num(), 1);
    ATranquilMindTargetActor* Target = Spawner->ActiveTargets[0].Get();
    Target->VisualMotion->BeginMotion(11);

    // Re-arm the phase machine with NON-ZERO Entrance / Readable Hold /
    // Persistence so those states can actually be observed. Production always
    // arms them at 0 ms; this proves they exist and gate input.
    UTMDemoLifecycleComponent* Life = Target->GetDemoLifecycle();
    Life->ForceInert();
    Spawner->DeregisterPhase4Target(Target);

    FTMDemoLifecycleTiming T;
    T.Entrance_MS       = 400.0f;
    T.ReadableHold_MS   = 200.0f;
    T.ScoredResponse_MS = 2500.0f;
    T.Persistence_MS    = 300.0f;
    T.Exit_MS           = 350.0f;
    Target->ArmDemoPresentationLifecycle(W.World->TimeSeconds, T);

    const FBlockStats Baseline = SM->GetCurrentBlockStats();

    // Compares against the ledger AT ENTRY to the phase under test, so a trial
    // legitimately scored earlier in the presentation (the Phase-4 expiry) is
    // not mistaken for one produced by the discarded input.
    auto AssertDiscarded = [&](const TCHAR* PhaseLabel, ETMDemoPresentationPhase Expected)
    {
        TestEqual(FString::Printf(TEXT("reached %s"), PhaseLabel),
            Life->GetPhase(), Expected);
        TestFalse(FString::Printf(TEXT("(2) input CLOSED in %s"), PhaseLabel),
            Life->IsInputOpen());
        TestEqual(FString::Printf(TEXT("(2) %s: not registered in ActiveTargets"), PhaseLabel),
            Spawner->ActiveTargets.Num(), 0);

        const FBlockStats AtEntry = SM->GetCurrentBlockStats();

        Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));
        Target->ResolveAsTriggered(W.World->TimeSeconds);   // direct path too

        const FBlockStats After = SM->GetCurrentBlockStats();
        TestEqual(FString::Printf(TEXT("(2) %s: no trial recorded by the input"), PhaseLabel),
            After.Trial_Count_Total, AtEntry.Trial_Count_Total);
        TestEqual(FString::Printf(TEXT("(2) %s: NOT converted into a commission"), PhaseLabel),
            After.N_Commission, AtEntry.N_Commission);
        TestEqual(FString::Printf(TEXT("(2) %s: no Hit"), PhaseLabel),
            After.N_Hit, AtEntry.N_Hit);
    };

    // PHASE 2 — Entrance.
    AssertDiscarded(TEXT("Phase 2 Entrance"), ETMDemoPresentationPhase::Entrance);

    // PHASE 3 — Readable Hold.
    Advance(W, Target, 0.45f);
    AssertDiscarded(TEXT("Phase 3 Readable Hold"), ETMDemoPresentationPhase::ReadableHold);

    // PHASE 4 — input must now be ACCEPTED, and the discarded input above must
    // NOT have been buffered, queued or replayed into it.
    Advance(W, Target, 0.25f);
    TestEqual(TEXT("(1) reached Phase 4"),
        Life->GetPhase(), ETMDemoPresentationPhase::ApproachResponse);
    TestTrue(TEXT("(1) input open in Phase 4"), Life->IsInputOpen());
    TestEqual(TEXT("(7) registered in ActiveTargets only now"),
        Spawner->ActiveTargets.Num(), 1);

    const FBlockStats AtPhase4 = SM->GetCurrentBlockStats();
    TestEqual(TEXT("(2) NOTHING was buffered or replayed into Phase 4"),
        AtPhase4.Trial_Count_Total, Baseline.Trial_Count_Total);

    // Let Phase 4 expire naturally so Persistence and Exit can be observed.
    Advance(W, Target, 2.55f);

    // PHASE 6 — Persistence.
    AssertDiscarded(TEXT("Phase 6 Persistence"), ETMDemoPresentationPhase::Persistence);

    // PHASE 7 — Exit.
    Advance(W, Target, 0.35f);
    AssertDiscarded(TEXT("Phase 7 Exit"), ETMDemoPresentationPhase::Exit);

    // Exactly one trial across the whole presentation: the natural expiry.
    const FBlockStats Final = SM->GetCurrentBlockStats();
    TestEqual(TEXT("(3) exactly one outcome across the whole presentation"),
        Final.Trial_Count_Total, Baseline.Trial_Count_Total + 1);

    return true;
}

// ============================================================
//  (8)(9)(10) Ownership cardinality and successor gating
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2OwnershipCardinalityTest,
    "TranquilMind.Demo.Stage2.OwnershipCardinality",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2OwnershipCardinalityTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);
    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);
    CaptureFrame(W);

    TestFalse(TEXT("(9) no presentation owned before the first spawn"),
        Spawner->HasLiveDemoPresentation());

    W.World->TimeSeconds = 400.0;
    ATranquilMindTargetActor* First = SpawnArmed(W, Spawner);
    TestNotNull(TEXT("first target spawned"), First);
    TestTrue(TEXT("(9) presentation owned from creation"),
        Spawner->HasLiveDemoPresentation());
    TestEqual(TEXT("(9) owned by the first target"),
        Spawner->GetLiveDemoPresentation(), First);

    int32 MaxActive = Spawner->ActiveTargets.Num();
    int32 MaxLive   = Spawner->HasLiveDemoPresentation() ? 1 : 0;

    // (10) Hammer the spawn path across the entire predecessor lifetime. Not one
    // successor may be created while the predecessor owns the presentation.
    for (int32 i = 0; i < 240 && IsValid(First); ++i)
    {
        Spawner->SpawnNextTarget();
        Spawner->RestartTrial();

        MaxActive = FMath::Max(MaxActive, Spawner->ActiveTargets.Num());
        MaxLive   = FMath::Max(MaxLive, Spawner->HasLiveDemoPresentation() ? 1 : 0);

        TestEqual(TEXT("(10) exactly one target actor exists at all times"),
            W.CountTargets(), 1);

        Advance(W, First, Step_SEC);
    }

    TestEqual(TEXT("(8) max ActiveTargets = 1"), MaxActive, 1);
    TestEqual(TEXT("(9) max official LiveDemoPresentation = 1"), MaxLive, 1);

    TestFalse(TEXT("predecessor destroyed"), IsValid(First));
    TestFalse(TEXT("(9) ownership released, exactly once, at Phase 9"),
        Spawner->HasLiveDemoPresentation());

    // (10) The gate delays; it never cancels. The successor spawns now.
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("(10) successor spawns once ownership ends"),
        Spawner->ActiveTargets.Num(), 1);
    TestEqual(TEXT("(10) and there is still exactly one presentation"),
        W.CountTargets(), 1);

    return true;
}

// ============================================================
//  (11)(12) Hard gate and restart
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2HardGateAndRestartTest,
    "TranquilMind.Demo.Stage2.HardGateAndRestartOwnership",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2HardGateAndRestartTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);
    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);
    CaptureFrame(W);

    W.World->TimeSeconds = 500.0;
    ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
    TestNotNull(TEXT("target spawned"), Target);
    TestTrue(TEXT("presentation owned"), Spawner->HasLiveDemoPresentation());

    Advance(W, Target, 0.5f);

    // (11) Hard-gate entry voids the live target. The Void is a CANCELLATION, so
    // the presentation ends promptly — but ownership is held until the actor
    // actually dies, which is what stops the resume from spawning into it.
    const FBlockStats Before = SM->GetCurrentBlockStats();
    Spawner->DestroyAllActiveTargetsAsVoid();
    const FBlockStats After = SM->GetCurrentBlockStats();

    TestEqual(TEXT("(11) exactly one Void recorded"), After.N_Void, Before.N_Void + 1);
    TestEqual(TEXT("(11) ActiveTargets cleared"), Spawner->ActiveTargets.Num(), 0);
    TestTrue(TEXT("(11) ownership still held while the bubble fades"),
        Spawner->HasLiveDemoPresentation());
    TestTrue(TEXT("(11) cancellation started the exit promptly"),
        Target->VisualMotion->IsVisualExitActive());

    // (12) A restart during the fade must not spawn into the predecessor.
    Spawner->RestartTrial();
    TestEqual(TEXT("(12) restart did not spawn into the fading predecessor"),
        W.CountTargets(), 1);
    TestEqual(TEXT("(12) and recorded no second Void"),
        SM->GetCurrentBlockStats().N_Void, After.N_Void);

    // The exit completes; ownership releases exactly once.
    Advance(W, Target, 0.45f);
    TestFalse(TEXT("(11) predecessor destroyed"), IsValid(Target));
    TestFalse(TEXT("(11) ownership released exactly once"),
        Spawner->HasLiveDemoPresentation());

    // (12) RestartTrial latched LastSpawnTimestamp_SEC < 0, so the ordinary tick
    // retry is still armed and spawns as soon as the gate opens.
    W.World->DeltaTimeSeconds = Step_SEC;
    Spawner->TickComponent(Step_SEC, LEVELTICK_All, nullptr);
    TestEqual(TEXT("(12) latched request spawns once the gate opens"),
        Spawner->ActiveTargets.Num(), 1);

    return true;
}

// ============================================================
//  (13)(15) Teardown, and an actor that dies without its normal path
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2TeardownOwnershipTest,
    "TranquilMind.Demo.Stage2.TeardownReleasesOwnership",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2TeardownOwnershipTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    // (15) An unexpected Destroy() must release ownership by itself. A weak
    // reference makes this automatic — this is the anti-deadlock assertion of
    // Spec §4.9.1: a gate that can deadlock the Demo is worse than the overlap
    // it prevents.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        ATranquilMindSessionManager* SM = nullptr;
        UTargetSpawnerComponent* Spawner = nullptr;
        BuildRuntimeInPhaseII(W, SM, Spawner);
        CaptureFrame(W);

        W.World->TimeSeconds = 600.0;
        ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
        TestNotNull(TEXT("target spawned"), Target);
        TestTrue(TEXT("ownership held"), Spawner->HasLiveDemoPresentation());

        // Kill it outright, bypassing every lifecycle path.
        Target->Destroy();

        TestFalse(TEXT("(15) stale ownership self-cleared by the weak reference"),
            Spawner->HasLiveDemoPresentation());

        Spawner->CleanupResolvedTargets();
        Spawner->SpawnNextTarget();
        TestEqual(TEXT("(15) spawning is not permanently blocked"),
            Spawner->ActiveTargets.Num(), 1);
    }

    // (13) Session end and world teardown leave no stale ownership.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        ATranquilMindSessionManager* SM = nullptr;
        UTargetSpawnerComponent* Spawner = nullptr;
        BuildRuntimeInPhaseII(W, SM, Spawner);
        CaptureFrame(W);

        W.World->TimeSeconds = 700.0;
        ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
        TestNotNull(TEXT("target spawned"), Target);
        TestTrue(TEXT("ownership held"), Spawner->HasLiveDemoPresentation());

        // Component EndPlay is the session-end / teardown path.
        Spawner->DestroyComponent();

        TestFalse(TEXT("(13) no stale ownership after teardown"),
            Spawner->HasLiveDemoPresentation());
        TestNull(TEXT("(13) ownership reference cleared"),
            Spawner->GetLiveDemoPresentation());

        // World teardown at scope exit must also be clean.
    }

    return true;
}

// ============================================================
//  (14) A failed spawn must not permanently block future spawning
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2FailedSpawnTest,
    "TranquilMind.Demo.Stage2.FailedSpawnDoesNotBlock",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2FailedSpawnTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);
    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);
    CaptureFrame(W);

    // Force the failure path: a null target class.
    Spawner->TargetActorClass = nullptr;

    W.World->TimeSeconds = 800.0;
    Spawner->SpawnNextTarget();

    TestEqual(TEXT("(14) nothing was created"), W.CountTargets(), 0);
    TestFalse(TEXT("(14) no ownership claimed by a failed spawn"),
        Spawner->HasLiveDemoPresentation());
    TestEqual(TEXT("(14) nothing registered for scoring"),
        Spawner->ActiveTargets.Num(), 0);

    // Recover: the gate must be open, not latched shut.
    Spawner->TargetActorClass = ATranquilMindTargetActor::StaticClass();
    W.World->TimeSeconds = 805.0;
    Spawner->SpawnNextTarget();

    TestEqual(TEXT("(14) spawning works again after a failed spawn"),
        Spawner->ActiveTargets.Num(), 1);
    TestTrue(TEXT("(14) ownership claimed by the successful spawn"),
        Spawner->HasLiveDemoPresentation());

    return true;
}

// ============================================================
//  (17) Demo lifecycle code draws from no Research random stream
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2NoResearchStreamTest,
    "TranquilMind.Demo.Stage2.DemoDoesNotTouchResearchStream",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2NoResearchStreamTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    // The Demo stimulus sequence is the observable proxy for stream state: if
    // any Stage 2 lifecycle path consumed a draw, the Go/NoGo order would move.
    // Two independent worlds must produce the identical sequence, including
    // across refused spawns, discarded input and early responses.
    auto CaptureSequence = [this](bool bWithLifecycleTraffic) -> TArray<uint8>
    {
        TArray<uint8> Seq;

        FScopedWorld W(ETMOperatingMode::Demo);
        ATranquilMindSessionManager* SM = nullptr;
        UTargetSpawnerComponent* Spawner = nullptr;
        BuildRuntimeInPhaseII(W, SM, Spawner);
        CaptureFrame(W);

        W.World->TimeSeconds = 900.0;

        for (int32 Trial = 0; Trial < 6; ++Trial)
        {
            ATranquilMindTargetActor* Target = SpawnArmed(W, Spawner);
            if (!IsValid(Target))
            {
                break;
            }
            Seq.Add(static_cast<uint8>(Target->GetStimulusType()));

            if (bWithLifecycleTraffic)
            {
                // Refused spawns, out-of-phase input and an early response —
                // every Stage 2 path that could conceivably touch a stream.
                Spawner->SpawnNextTarget();
                Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));
                Advance(W, Target, 0.2f);
                Spawner->HandleTriggerPulled(FVector(0.0f, 1.0f, 0.0f));
                Spawner->SpawnNextTarget();
            }

            // Run the presentation out so the gate opens for the next trial.
            for (int32 i = 0; i < 300 && IsValid(Target); ++i)
            {
                Advance(W, Target, Step_SEC);
            }
        }

        return Seq;
    };

    const TArray<uint8> Quiet = CaptureSequence(false);
    const TArray<uint8> Busy  = CaptureSequence(true);

    TestTrue(TEXT("(17) captured a usable sequence"), Quiet.Num() >= 4);
    TestEqual(TEXT("(17) same number of stimuli"), Busy.Num(), Quiet.Num());

    bool bIdentical = (Busy.Num() == Quiet.Num());
    for (int32 i = 0; bIdentical && i < Quiet.Num(); ++i)
    {
        bIdentical = (Busy[i] == Quiet[i]);
    }

    TestTrue(TEXT("(17) Stage 2 lifecycle traffic perturbs no stimulus draw"),
        bIdentical);

    return true;
}

// ============================================================
//  (18) Research negative control — the lifecycle never arms there
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMStage2ResearchInertTest,
    "TranquilMind.Research.Stage2.LifecycleNeverArms",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMStage2ResearchInertTest::RunTest(const FString&)
{
    using namespace TMStage2TestUtil;

    FScopedWorld W(ETMOperatingMode::Research);

    ATranquilMindSessionManager* SM = W.World->SpawnActor<ATranquilMindSessionManager>();
    AActor* Host = W.World->SpawnActor<AActor>();
    UTargetSpawnerComponent* Spawner = NewObject<UTargetSpawnerComponent>(Host);
    Spawner->RegisterComponent();
    Spawner->RegisterSessionManager(SM);

    // The Demo spawn path is guarded, so no Demo presentation and therefore no
    // presentation ownership can ever exist in Research.
    Spawner->SpawnNextTarget();
    Spawner->RestartTrial();
    Spawner->HandleTriggerPulled(FVector(1.0f, 0.0f, 0.0f));

    TestEqual(TEXT("(18) no Demo targets in Research"), W.CountTargets(), 0);
    TestEqual(TEXT("(18) ActiveTargets empty in Research"),
        Spawner->ActiveTargets.Num(), 0);
    TestFalse(TEXT("(18) no presentation ownership in Research"),
        Spawner->HasLiveDemoPresentation());

    // A Research stimulus actor carries the component but must never arm it.
    ATranquilMindTargetActor* Stim = W.World->SpawnActor<ATranquilMindTargetActor>();
    Stim->InitializeResearchVisual(ETMStimulusType::Go, 0.3f);

    TestNotNull(TEXT("(18) research stimulus carries the lifecycle component"),
        Stim->GetDemoLifecycle());
    TestFalse(TEXT("(18) lifecycle is INERT on a Research stimulus"),
        Stim->GetDemoLifecycle()->IsArmed());
    TestEqual(TEXT("(18) lifecycle phase is None on a Research stimulus"),
        Stim->GetDemoLifecycle()->GetPhase(), ETMDemoPresentationPhase::None);

    // Even a direct arming attempt must be refused on a Research stimulus.
    FTMDemoLifecycleTiming T;
    Stim->ArmDemoPresentationLifecycle(0.0f, T);
    TestFalse(TEXT("(18) direct arming refused on a Research stimulus"),
        Stim->GetDemoLifecycle()->IsArmed());

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
