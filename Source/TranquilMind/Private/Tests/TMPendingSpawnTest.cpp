// Exactly-once guarantee for the pending Demo spawn handshake.
//
// When SpawnNextTarget() runs while the Demo session frame is still waiting on
// a first usable XR pose, the request must be recorded once, coalesced across
// repeats, consumed exactly once on capture (valid pose OR timeout fallback),
// and must never leak into Research or perturb the stimulus sequence.
//
// The pose environment is injected via PoseSampleOverrideForTests so the
// Initializing wait and the timeout are fully deterministic — no headset.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

#include "TMDemoSessionFrame.h"
#include "TargetSpawnerComponent.h"
#include "../../TranquilMindSessionManager.h"
#include "../../TranquilMindTypes.h"
#include "../../Public/Private/Private/TranquilMindTargetActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMPendingSpawnTestUtil
{
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
                ++Count;
            }
            return Count;
        }
    };

    /** Shared mutable state the injected sampler reads each call. */
    struct FPoseDriver
    {
        ETMXRPoseStatus Status = ETMXRPoseStatus::Initializing;
        float Now = 0.0f;
        FVector CamLoc = FVector::ZeroVector;
        FVector CamFwd = FVector::ZeroVector;
    };

    void BindDriver(UTMDemoSessionFrameSubsystem* Frame, TSharedRef<FPoseDriver> Driver)
    {
        Frame->PoseSampleOverrideForTests = [Driver]()
        {
            FTMDemoPoseSample S;
            S.Status = Driver->Status;
            S.NowSeconds = Driver->Now;
            S.CameraLocation = Driver->CamLoc;
            S.CameraForward = Driver->CamFwd;
            return S;
        };
    }

    /** SessionManager + spawner, driven into Phase II with on-centre gaze. */
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
}

// ============================================================
//  (1)-(6) + (9) Exactly-once through valid-pose capture
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMPendingSpawnValidPoseTest,
    "TranquilMind.Demo.PendingSpawn.ExactlyOnceValidPose",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMPendingSpawnValidPoseTest::RunTest(const FString&)
{
    using namespace TMPendingSpawnTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);
    TestEqual(TEXT("reached Phase II"),
        SM->GetCurrentPhase(), ETMSessionPhase::Phase_II_CoreTraining);

    UTMDemoSessionFrameSubsystem* Frame =
        W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
    TestNotNull(TEXT("frame subsystem exists"), Frame);

    TSharedRef<FPoseDriver> Driver = MakeShared<FPoseDriver>();
    BindDriver(Frame, Driver);

    const FSessionStats LedgerBefore = SM->GetSessionStats();

    // (1) Initializing XR + spawn request -> zero targets immediately.
    Driver->Status = ETMXRPoseStatus::Initializing;
    Driver->Now = 0.0f;
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("no target while XR initializing"), W.CountTargets(), 0);
    TestEqual(TEXT("ActiveTargets empty while pending"), Spawner->ActiveTargets.Num(), 0);
    TestFalse(TEXT("frame not latched by the attempt"), Frame->IsDemoSessionFrameValid());

    // (4) Repeated spawn calls while pending coalesce — still zero targets.
    for (int32 i = 0; i < 5; ++i)
    {
        Driver->Now += 0.1f;
        Spawner->SpawnNextTarget();
    }
    TestEqual(TEXT("repeats while pending create nothing"), W.CountTargets(), 0);

    // (9) No trial outcome of any kind while the request is waiting.
    const FSessionStats LedgerDuring = SM->GetSessionStats();
    TestEqual(TEXT("no trials recorded while waiting"),
        LedgerDuring.Total_Trial_Count_Total, LedgerBefore.Total_Trial_Count_Total);
    TestEqual(TEXT("no omissions while waiting"),
        LedgerDuring.Total_N_Omission, LedgerBefore.Total_N_Omission);
    TestEqual(TEXT("no correct rejections while waiting"),
        LedgerDuring.Total_N_CorrectRejection, LedgerBefore.Total_N_CorrectRejection);

    // (2) A valid pose arriving inside the wait creates EXACTLY ONE target.
    Driver->Status = ETMXRPoseStatus::Tracked;
    Driver->CamLoc = FVector::ZeroVector;
    Driver->CamFwd = FVector(0.0f, 1.0f, 0.0f);
    Driver->Now += 0.1f;

    W.World->TimeSeconds = 20.0;   // give the spawn a sane timestamp
    Frame->Tick(1.0f / 60.0f);     // the bounded-wait tick that latches

    TestTrue(TEXT("frame latched"), Frame->IsDemoSessionFrameValid());
    TestEqual(TEXT("captured from the HMD"),
        Frame->GetDemoSessionFrame().Source, ETMDemoFrameSource::HMDCamera);
    TestEqual(TEXT("EXACTLY one target after capture"), W.CountTargets(), 1);
    TestEqual(TEXT("ActiveTargets has exactly one"), Spawner->ActiveTargets.Num(), 1);

    // The deferred attempts consumed no stimulus draws: the first spawned
    // target is still the alternation sequence's first entry (GO).
    ATranquilMindTargetActor* First = Spawner->ActiveTargets[0].Get();
    TestNotNull(TEXT("first target valid"), First);
    TestEqual(TEXT("stimulus sequence unperturbed by deferred attempts"),
        First->GetStimulusType(), ETMStimulusType::Go);
    TestTrue(TEXT("spawned along the captured heading at the Demo distance"),
        First->GetActorLocation().Equals(
            FVector(0.0f, Spawner->DemoMode_SpawnDistance_CM, 0.0f), 0.1f));

    // (5) Additional subsystem ticks / EnsureCaptured after Ready: no duplicates.
    for (int32 i = 0; i < 5; ++i)
    {
        Frame->Tick(1.0f / 60.0f);
        Frame->EnsureCaptured();
    }
    TestEqual(TEXT("no duplicates from extra ticks"), W.CountTargets(), 1);

    // (6) A normal later spawn still creates one additional target.
    First->ResolveAsVoid();
    Spawner->CleanupResolvedTargets();
    W.World->TimeSeconds = 25.0;
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("ordinary later spawn works"), Spawner->ActiveTargets.Num(), 1);
    TestEqual(TEXT("second target is the sequence's second entry (NOGO)"),
        Spawner->ActiveTargets[0]->GetStimulusType(), ETMStimulusType::NoGo);

    return true;
}

// ============================================================
//  (3) Exactly-once through the timeout fallback
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMPendingSpawnTimeoutTest,
    "TranquilMind.Demo.PendingSpawn.ExactlyOnceTimeout",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMPendingSpawnTimeoutTest::RunTest(const FString&)
{
    using namespace TMPendingSpawnTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);

    UTMDemoSessionFrameSubsystem* Frame =
        W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
    TSharedRef<FPoseDriver> Driver = MakeShared<FPoseDriver>();
    BindDriver(Frame, Driver);

    // Pend the request, then never deliver a pose.
    Driver->Status = ETMXRPoseStatus::Initializing;
    Driver->Now = 50.0f;
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("nothing spawned while waiting"), W.CountTargets(), 0);

    // Stay inside the budget: still nothing.
    Driver->Now = 50.0f + UTMDemoSessionFrameSubsystem::PoseWaitBudget_SEC - 0.2f;
    Frame->Tick(1.0f / 60.0f);
    TestEqual(TEXT("still nothing inside the budget"), W.CountTargets(), 0);

    // (3) Cross the budget: the timeout fallback consumes the SAME request,
    //     exactly once.
    Driver->Now = 50.0f + UTMDemoSessionFrameSubsystem::PoseWaitBudget_SEC + 0.2f;
    W.World->TimeSeconds = 20.0;
    Frame->Tick(1.0f / 60.0f);

    TestTrue(TEXT("frame latched by timeout"), Frame->IsDemoSessionFrameValid());
    TestTrue(TEXT("timeout recorded"), Frame->GetDemoSessionFrame().bCaptureTimedOut);
    TestEqual(TEXT("EXACTLY one target from the timeout fallback"), W.CountTargets(), 1);

    // Further ticks add nothing.
    Frame->Tick(1.0f / 60.0f);
    TestEqual(TEXT("no duplicate after timeout consume"), W.CountTargets(), 1);

    return true;
}

// ============================================================
//  (7) Teardown with a live pending request is safe
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMPendingSpawnTeardownTest,
    "TranquilMind.Demo.PendingSpawn.TeardownSafe",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMPendingSpawnTeardownTest::RunTest(const FString&)
{
    using namespace TMPendingSpawnTestUtil;

    {
        FScopedWorld W(ETMOperatingMode::Demo);

        ATranquilMindSessionManager* SM = nullptr;
        UTargetSpawnerComponent* Spawner = nullptr;
        BuildRuntimeInPhaseII(W, SM, Spawner);

        UTMDemoSessionFrameSubsystem* Frame =
            W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
        TSharedRef<FPoseDriver> Driver = MakeShared<FPoseDriver>();
        BindDriver(Frame, Driver);

        Driver->Status = ETMXRPoseStatus::Initializing;
        Spawner->SpawnNextTarget();
        TestEqual(TEXT("request pending at teardown"), W.CountTargets(), 0);

        // Component EndPlay drops the request and the subscription; a capture
        // after that must not reach the dying spawner.
        Spawner->DestroyComponent();
        Driver->Status = ETMXRPoseStatus::Tracked;
        Driver->CamFwd = FVector(1.0f, 0.0f, 0.0f);
        Frame->Tick(1.0f / 60.0f);
        TestEqual(TEXT("capture after teardown spawns nothing"), W.CountTargets(), 0);

        // World teardown with the frame latched: scope exit must be clean.
    }

    return true;
}

// ============================================================
//  (8) Research never records or consumes a pending Demo spawn
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMPendingSpawnResearchInertTest,
    "TranquilMind.Research.PendingSpawn.NeverEnters",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMPendingSpawnResearchInertTest::RunTest(const FString&)
{
    using namespace TMPendingSpawnTestUtil;

    FScopedWorld W(ETMOperatingMode::Research);

    ATranquilMindSessionManager* SM = W.World->SpawnActor<ATranquilMindSessionManager>();
    AActor* Host = W.World->SpawnActor<AActor>();
    UTargetSpawnerComponent* Spawner = NewObject<UTargetSpawnerComponent>(Host);
    Spawner->RegisterComponent();
    Spawner->RegisterSessionManager(SM);

    // No subsystem exists, so no subscription was made and no capture can occur.
    TestNull(TEXT("no frame subsystem in Research"),
        W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>());

    // Direct calls hit the Research guard before any pending logic.
    Spawner->SpawnNextTarget();
    Spawner->RestartTrial();
    TestEqual(TEXT("no targets in Research"), W.CountTargets(), 0);
    TestEqual(TEXT("ActiveTargets empty in Research"), Spawner->ActiveTargets.Num(), 0);

    return true;
}

// ============================================================
//  (10) Demo duration and post-capture cadence remain valid
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMPendingSpawnCadenceTest,
    "TranquilMind.Demo.PendingSpawn.PostCaptureCadence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMPendingSpawnCadenceTest::RunTest(const FString&)
{
    using namespace TMPendingSpawnTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);

    UTMDemoSessionFrameSubsystem* Frame =
        W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
    TSharedRef<FPoseDriver> Driver = MakeShared<FPoseDriver>();
    BindDriver(Frame, Driver);

    // Wait, then capture and consume.
    Driver->Status = ETMXRPoseStatus::Initializing;
    Driver->Now = 0.0f;
    W.World->TimeSeconds = 20.0;
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("pending, nothing spawned"), W.CountTargets(), 0);

    Driver->Status = ETMXRPoseStatus::Tracked;
    Driver->CamFwd = FVector(1.0f, 0.0f, 0.0f);
    Frame->Tick(1.0f / 60.0f);
    TestEqual(TEXT("one target after capture"), Spawner->ActiveTargets.Num(), 1);

    // Resolve it, advance PAST the Demo ISI (3000 ms), and drive the ordinary
    // component tick: the normal cadence must produce the next target.
    Spawner->ActiveTargets[0]->ResolveAsVoid();
    Spawner->CleanupResolvedTargets();

    W.World->TimeSeconds = 20.0 + 3.1;   // > DemoMode_ISI_MS
    W.World->DeltaTimeSeconds = 1.0f / 60.0f;
    Spawner->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);

    TestEqual(TEXT("normal ISI cadence resumed after capture"),
        Spawner->ActiveTargets.Num(), 1);
    TestFalse(TEXT("demo has not ended"), Spawner->IsDemoEnded());

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
