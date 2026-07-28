// Deterministic coverage for the Demo player-relative session frame.
//
// The frame is the single coordinate authority every later Demo presentation
// system (approach, staging lanes, ambient field, wisps) resolves through, so
// its XR readiness handling, basis, origin/orientation split, immutability and
// Research inertness are all locked here.

#include "Misc/AutomationTest.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

#include "TMDemoSessionFrame.h"
#include "TargetSpawnerComponent.h"
#include "../../TranquilMindSessionManager.h"
#include "../../TranquilMindTypes.h"
#include "../../Public/Private/Private/TranquilMindTargetActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMDemoFrameTestUtil
{
    constexpr float Tol = 1.0e-4f;

    void SetMode(ETMOperatingMode Mode)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                TEXT("tranquilmind.OperatingMode")))
        {
            CVar->Set(Mode == ETMOperatingMode::Research ? 1 : 0, ECVF_SetByCode);
        }
    }

    /** Scratch game world with the operating mode latched before begin-play. */
    struct FScopedWorld
    {
        UWorld* World = nullptr;

        explicit FScopedWorld(ETMOperatingMode Mode)
        {
            SetMode(Mode);

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

        UTMDemoSessionFrameSubsystem* Frame() const
        {
            return World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
        }
    };

    /** Build a pose sample without touching a real headset. */
    FTMDemoPoseSample MakeSample(
        ETMXRPoseStatus Status, float Now,
        const FVector& CamLoc = FVector::ZeroVector,
        const FVector& CamFwd = FVector::ZeroVector,
        const FVector& PawnFwd = FVector::ZeroVector)
    {
        FTMDemoPoseSample S;
        S.Status = Status;
        S.NowSeconds = Now;
        S.CameraLocation = CamLoc;
        S.CameraForward = CamFwd;
        S.PawnForward = PawnFwd;
        S.bHasPawn = !PawnFwd.IsNearlyZero();
        return S;
    }
}

// ============================================================
//  (1)(2)(3) XR readiness: no premature latch, later capture, bounded timeout
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoFrameXRReadinessTest,
    "TranquilMind.Demo.SessionFrame.XRReadiness",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoFrameXRReadinessTest::RunTest(const FString&)
{
    using namespace TMDemoFrameTestUtil;

    // (1) XR allowed but pose not yet valid must NOT latch anything.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();

        for (float T = 0.0f; T < 3.0f; T += 0.5f)
        {
            const bool bCaptured = F->AdvanceCaptureWithSample(
                MakeSample(ETMXRPoseStatus::Initializing, T,
                    FVector::ZeroVector, FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)));

            TestFalse(TEXT("Initializing does not capture"), bCaptured);
            TestFalse(TEXT("frame stays invalid while initializing"),
                F->IsDemoSessionFrameValid());
        }

        // (2) A later valid pose inside the budget is captured from the HMD.
        const bool bCaptured = F->AdvanceCaptureWithSample(
            MakeSample(ETMXRPoseStatus::Tracked, 3.0f,
                FVector(5.0f, 6.0f, 150.0f), FVector(0.0f, 1.0f, 0.0f)));

        TestTrue(TEXT("late valid pose captures"), bCaptured);
        TestTrue(TEXT("frame valid"), F->IsDemoSessionFrameValid());
        TestEqual(TEXT("source is the HMD, not a fallback"),
            F->GetDemoSessionFrame().Source, ETMDemoFrameSource::HMDCamera);
        TestFalse(TEXT("not marked as timed out"),
            F->GetDemoSessionFrame().bCaptureTimedOut);
        TestTrue(TEXT("captured the real heading"),
            F->GetDemoSessionForward().Equals(FVector(0.0f, 1.0f, 0.0f), Tol));
    }

    // (3) Timeout is explicit and deterministic.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();

        const FVector PawnFwd(0.0f, 1.0f, 0.0f);

        TestFalse(TEXT("wait begins"), F->AdvanceCaptureWithSample(
            MakeSample(ETMXRPoseStatus::Initializing, 100.0f,
                FVector::ZeroVector, FVector::ZeroVector, PawnFwd)));

        // Just inside the budget: still waiting.
        TestFalse(TEXT("still waiting just inside budget"), F->AdvanceCaptureWithSample(
            MakeSample(ETMXRPoseStatus::Initializing,
                100.0f + UTMDemoSessionFrameSubsystem::PoseWaitBudget_SEC - 0.1f,
                FVector::ZeroVector, FVector::ZeroVector, PawnFwd)));
        TestFalse(TEXT("still invalid"), F->IsDemoSessionFrameValid());

        // Past the budget: latch a deterministic fallback, flagged as timed out.
        TestTrue(TEXT("times out past budget"), F->AdvanceCaptureWithSample(
            MakeSample(ETMXRPoseStatus::Initializing,
                100.0f + UTMDemoSessionFrameSubsystem::PoseWaitBudget_SEC + 0.1f,
                FVector::ZeroVector, FVector::ZeroVector, PawnFwd)));

        TestTrue(TEXT("frame valid after timeout"), F->IsDemoSessionFrameValid());
        TestTrue(TEXT("timeout is explicitly recorded"),
            F->GetDemoSessionFrame().bCaptureTimedOut);
        TestEqual(TEXT("fallback source is the pawn"),
            F->GetDemoSessionFrame().Source, ETMDemoFrameSource::PawnFallback);
        TestTrue(TEXT("fallback heading is the pawn heading"),
            F->GetDemoSessionForward().Equals(PawnFwd, Tol));
    }

    // (C) Genuinely non-XR falls back immediately with no wait and no timeout.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();

        TestTrue(TEXT("NoXR captures immediately"), F->AdvanceCaptureWithSample(
            MakeSample(ETMXRPoseStatus::NoXR, 0.0f,
                FVector::ZeroVector, FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f))));
        TestFalse(TEXT("NoXR is not a timeout"),
            F->GetDemoSessionFrame().bCaptureTimedOut);
        TestEqual(TEXT("NoXR uses the pawn heading"),
            F->GetDemoSessionFrame().Source, ETMDemoFrameSource::PawnFallback);
    }

    // No pawn at all -> deterministic world-axis fallback.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();

        TestTrue(TEXT("headless captures"), F->AdvanceCaptureWithSample(
            MakeSample(ETMXRPoseStatus::NoXR, 0.0f)));
        TestEqual(TEXT("world-axis fallback"),
            F->GetDemoSessionFrame().Source, ETMDemoFrameSource::WorldAxisFallback);
        TestTrue(TEXT("forward is +X"),
            F->GetDemoSessionForward().Equals(FVector(1.0f, 0.0f, 0.0f), Tol));
    }

    return true;
}

// ============================================================
//  Basis: heading, pitch removal, degeneracy
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoFrameBasisTest,
    "TranquilMind.Demo.SessionFrame.Basis",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoFrameBasisTest::RunTest(const FString&)
{
    using namespace TMDemoFrameTestUtil;

    const FVector Origin(10.0f, 20.0f, 150.0f);

    {   // Forward +X -> +X
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        TestNotNull(TEXT("subsystem exists in Demo"), F);
        TestTrue(TEXT("+X captures"), F->CaptureFromPose(Origin, FVector(1.0f, 0.0f, 0.0f)));
        TestTrue(TEXT("forward is +X"),
            F->GetDemoSessionForward().Equals(FVector(1.0f, 0.0f, 0.0f), Tol));
    }

    {   // Forward +Y -> +Y
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        TestTrue(TEXT("+Y captures"), F->CaptureFromPose(Origin, FVector(0.0f, 1.0f, 0.0f)));
        TestTrue(TEXT("forward is +Y"),
            F->GetDemoSessionForward().Equals(FVector(0.0f, 1.0f, 0.0f), Tol));
    }

    {   // Pitched up 45 deg -> vertical removed, heading kept
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        TestTrue(TEXT("pitched captures"),
            F->CaptureFromPose(Origin, FVector(1.0f, 0.0f, 1.0f).GetSafeNormal()));
        const FVector Fwd = F->GetDemoSessionForward();
        TestTrue(TEXT("pitch removed"), FMath::IsNearlyZero(Fwd.Z, Tol));
        TestTrue(TEXT("heading preserved"), Fwd.Equals(FVector(1.0f, 0.0f, 0.0f), Tol));
        TestTrue(TEXT("unit length"), FMath::IsNearlyEqual(Fwd.Size(), 1.0f, Tol));
    }

    {   // Pitched down, diagonal heading kept
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        TestTrue(TEXT("pitched-down captures"),
            F->CaptureFromPose(Origin, FVector(1.0f, 1.0f, -3.0f)));
        TestTrue(TEXT("diagonal heading preserved"),
            F->GetDemoSessionForward().Equals(FVector(1.0f, 1.0f, 0.0f).GetSafeNormal(), Tol));
    }

    {   // Degenerate near-vertical: refuse rather than latch garbage
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        TestFalse(TEXT("straight-up heading refused"),
            F->CaptureFromPose(Origin, FVector(0.0f, 0.0f, 1.0f)));
        TestFalse(TEXT("still invalid"), F->IsDemoSessionFrameValid());

        // A TRACKED but vertical pose must not wait — yaw is unrecoverable, so
        // it takes the deterministic pawn heading straight away.
        TestTrue(TEXT("tracked-but-vertical falls back at once"),
            F->AdvanceCaptureWithSample(MakeSample(ETMXRPoseStatus::Tracked, 0.0f,
                Origin, FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 0.0f))));
        TestEqual(TEXT("pawn fallback used"),
            F->GetDemoSessionFrame().Source, ETMDemoFrameSource::PawnFallback);
        TestFalse(TEXT("not a timeout"), F->GetDemoSessionFrame().bCaptureTimedOut);
        const FVector Fwd = F->GetDemoSessionForward();
        TestTrue(TEXT("fallback horizontal"), FMath::IsNearlyZero(Fwd.Z, Tol));
        TestTrue(TEXT("fallback normalized"), FMath::IsNearlyEqual(Fwd.Size(), 1.0f, Tol));
    }

    return true;
}

// ============================================================
//  Orthonormality and handedness
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoFrameOrthonormalTest,
    "TranquilMind.Demo.SessionFrame.OrthonormalAndHanded",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoFrameOrthonormalTest::RunTest(const FString&)
{
    using namespace TMDemoFrameTestUtil;

    {   // Facing +X, right is +Y; a positive right lane lands at +Y.
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        F->CaptureFromPose(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f));
        TestTrue(TEXT("facing +X, right is +Y"),
            F->GetDemoSessionRight().Equals(FVector(0.0f, 1.0f, 0.0f), Tol));
        TestTrue(TEXT("positive right lane is at +Y"),
            F->DemoEnvironmentPointToWorld(0.0f, 100.0f, 0.0f)
                .Equals(FVector(0.0f, 100.0f, 0.0f), Tol));
    }

    {   // Facing +Y, right is -X.
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        F->CaptureFromPose(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f));
        TestTrue(TEXT("facing +Y, right is -X"),
            F->GetDemoSessionRight().Equals(FVector(-1.0f, 0.0f, 0.0f), Tol));
    }

    for (int32 Deg = 0; Deg < 360; Deg += 37)
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();

        const float Rad = FMath::DegreesToRadians(static_cast<float>(Deg));
        F->CaptureFromPose(FVector::ZeroVector,
            FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.5f));

        const FVector Fwd = F->GetDemoSessionForward();
        const FVector Rgt = F->GetDemoSessionRight();
        const FVector Up = F->GetDemoSessionUp();

        TestTrue(*FString::Printf(TEXT("forward unit @%d"), Deg),
            FMath::IsNearlyEqual(Fwd.Size(), 1.0f, Tol));
        TestTrue(*FString::Printf(TEXT("right unit @%d"), Deg),
            FMath::IsNearlyEqual(Rgt.Size(), 1.0f, Tol));
        TestTrue(*FString::Printf(TEXT("up unit @%d"), Deg),
            FMath::IsNearlyEqual(Up.Size(), 1.0f, Tol));
        TestTrue(*FString::Printf(TEXT("fwd perp right @%d"), Deg),
            FMath::IsNearlyZero(FVector::DotProduct(Fwd, Rgt), Tol));
        TestTrue(*FString::Printf(TEXT("fwd perp up @%d"), Deg),
            FMath::IsNearlyZero(FVector::DotProduct(Fwd, Up), Tol));
        TestTrue(*FString::Printf(TEXT("right perp up @%d"), Deg),
            FMath::IsNearlyZero(FVector::DotProduct(Rgt, Up), Tol));
    }

    return true;
}

// ============================================================
//  (4)(5)(6)(7) Immutable orientation, moving active origin, fixed env origin
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoFrameOriginSplitTest,
    "TranquilMind.Demo.SessionFrame.OriginOrientationSplit",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoFrameOriginSplitTest::RunTest(const FString&)
{
    using namespace TMDemoFrameTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);
    UTMDemoSessionFrameSubsystem* F = W.Frame();

    const FVector CaptureOrigin(0.0f, 0.0f, 150.0f);
    TestTrue(TEXT("capture facing +X"),
        F->CaptureFromPose(CaptureOrigin, FVector(1.0f, 0.0f, 0.0f)));

    // (4) Orientation is immutable across any later pose change.
    TestFalse(TEXT("recapture refused"),
        F->CaptureFromPose(FVector(500.0f, 500.0f, 500.0f), FVector(0.0f, 1.0f, 0.0f)));
    TestTrue(TEXT("forward unchanged"),
        F->GetDemoSessionForward().Equals(FVector(1.0f, 0.0f, 0.0f), Tol));
    TestTrue(TEXT("right unchanged"),
        F->GetDemoSessionRight().Equals(FVector(0.0f, 1.0f, 0.0f), Tol));

    // (7) Environment origin stays at the capture-time position, so ambient
    //     content authored against it does not follow the participant.
    TestTrue(TEXT("environment origin is the capture origin"),
        F->GetDemoSessionOrigin().Equals(CaptureOrigin, Tol));
    TestTrue(TEXT("environment point anchored to capture origin"),
        F->DemoEnvironmentPointToWorld(800.0f, 0.0f, 0.0f)
            .Equals(FVector(800.0f, 0.0f, 150.0f), Tol));

    // (5) Active spawn origin DOES follow the camera.
    const FVector CamAtCapture = CaptureOrigin;
    TestTrue(TEXT("active spawn at capture position"),
        F->DemoActiveSpawnPointFrom(CamAtCapture, 200.0f, 0.0f, 0.0f)
            .Equals(FVector(200.0f, 0.0f, 150.0f), Tol));

    // Participant leans 40 cm right and 30 cm forward.
    const FVector CamAfterLean(30.0f, 40.0f, 150.0f);
    const FVector SpawnAfterLean = F->DemoActiveSpawnPointFrom(CamAfterLean, 200.0f, 0.0f, 0.0f);
    TestTrue(TEXT("active spawn tracks the moved camera"),
        SpawnAfterLean.Equals(FVector(230.0f, 40.0f, 150.0f), Tol));

    // The comfort distance is preserved exactly despite the movement — this is
    // the whole reason the active origin is not the frozen capture origin.
    TestTrue(TEXT("comfort distance preserved after leaning"),
        FMath::IsNearlyEqual((SpawnAfterLean - CamAfterLean).Size(), 200.0f, 0.01f));

    // (6) Active spawn HEADING must not follow head rotation. The frame was
    //     captured facing +X; the participant now faces +Y, but the offset
    //     direction from the camera is still +X.
    const FVector Offset = SpawnAfterLean - CamAfterLean;
    TestTrue(TEXT("spawn heading still the captured +X, not the new facing"),
        Offset.GetSafeNormal().Equals(FVector(1.0f, 0.0f, 0.0f), Tol));
    TestTrue(TEXT("no +Y component from the new head rotation"),
        FMath::IsNearlyZero(Offset.Y, 0.01f));

    // Lanes also stay in the immutable basis.
    const FVector RightLane = F->DemoActiveSpawnPointFrom(CamAfterLean, 200.0f, 50.0f, 0.0f);
    TestTrue(TEXT("right lane uses captured right (+Y)"),
        (RightLane - SpawnAfterLean).Equals(FVector(0.0f, 50.0f, 0.0f), Tol));

    // Reset permits exactly one recapture.
    F->ResetForNewSession();
    TestFalse(TEXT("invalid after reset"), F->IsDemoSessionFrameValid());
    TestTrue(TEXT("one new capture allowed"),
        F->CaptureFromPose(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f)));
    TestEqual(TEXT("ordinal advanced"), F->GetDemoSessionFrame().CaptureOrdinal, 2);
    TestFalse(TEXT("further capture refused"),
        F->CaptureFromPose(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)));

    return true;
}

// ============================================================
//  (8) Research inertness — both layers
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoFrameResearchInertTest,
    "TranquilMind.Research.SessionFrame.NotCreatedOrCaptured",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoFrameResearchInertTest::RunTest(const FString&)
{
    using namespace TMDemoFrameTestUtil;

    // Layer 1: the subsystem is not created at all in Research.
    {
        FScopedWorld W(ETMOperatingMode::Research);
        TestNull(TEXT("no Demo session frame subsystem in Research"), W.Frame());
    }

    // Control: it does exist in Demo, so the null above is meaningful.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        TestNotNull(TEXT("control: subsystem exists in Demo"), W.Frame());
    }

    // Layer 2: even holding a live instance, it refuses to capture while the
    // mode reads Research — so an existing object could not become consumable.
    {
        FScopedWorld W(ETMOperatingMode::Demo);
        UTMDemoSessionFrameSubsystem* F = W.Frame();
        TestNotNull(TEXT("instance obtained in Demo"), F);

        SetMode(ETMOperatingMode::Research);

        TestFalse(TEXT("refuses to capture in Research"),
            F->AdvanceCaptureWithSample(MakeSample(ETMXRPoseStatus::Tracked, 0.0f,
                FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f))));
        TestFalse(TEXT("still uncaptured"), F->IsDemoSessionFrameValid());
        TestFalse(TEXT("not left ticking"), F->IsTickable());

        SetMode(ETMOperatingMode::Demo);
    }

    return true;
}

// ============================================================
//  Demo regression: spawn / travel / depth still correct end to end
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoFrameSpawnRegressionTest,
    "TranquilMind.Demo.SessionFrame.SpawnRegression",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoFrameSpawnRegressionTest::RunTest(const FString&)
{
    using namespace TMDemoFrameTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = W.World->SpawnActor<ATranquilMindSessionManager>();
    AActor* Host = W.World->SpawnActor<AActor>();
    UTargetSpawnerComponent* Spawner = NewObject<UTargetSpawnerComponent>(Host);
    Spawner->RegisterComponent();
    Spawner->RegisterSessionManager(SM);

    // Pin a known heading: facing +Y from the origin.
    UTMDemoSessionFrameSubsystem* F = W.Frame();
    TestTrue(TEXT("frame captured"),
        F->CaptureFromPose(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f)));

    for (int32 i = 0; i < 800; ++i)
    {
        W.World->DeltaTimeSeconds = 1.0f / 60.0f;
        SM->UpdateGazeVector(FVector(1.0f, 0.0f, 0.0f));
        SM->Tick(1.0f / 60.0f);
    }
    TestEqual(TEXT("reached Phase II"),
        SM->GetCurrentPhase(), ETMSessionPhase::Phase_II_CoreTraining);

    Spawner->SpawnNextTarget();
    TestEqual(TEXT("one target spawned"), Spawner->ActiveTargets.Num(), 1);

    ATranquilMindTargetActor* Target = Spawner->ActiveTargets[0].Get();
    TestNotNull(TEXT("target valid"), Target);

    // No pawn in this world, so the active origin resolves to the frame origin;
    // DemoMode_SpawnDistance_CM (Pass 1A: 350 cm) along the captured +Y heading.
    const float ExpectedDist = Spawner->DemoMode_SpawnDistance_CM;
    TestTrue(TEXT("spawned at DemoMode_SpawnDistance_CM along frame forward"),
        Target->GetActorLocation().Equals(FVector(0.0f, ExpectedDist, 0.0f), 0.1f));
    TestTrue(TEXT("approach is -frame forward"),
        Target->TravelDirection_World.Equals(FVector(0.0f, -1.0f, 0.0f), Tol));
    TestTrue(TEXT("depth equals spawn distance at spawn"),
        FMath::IsNearlyEqual(Target->GetDepthCM(), ExpectedDist, 0.1f));

    // Pass 1A: presentation scale is the Demo parameter, not the TEMP 1.0.
    TestTrue(TEXT("presentation scale applied"),
        Target->GetActorScale3D().Equals(FVector(Spawner->DemoMode_TargetScale), Tol));

    // Pass 1A.1: the actor must have RECEIVED the Demo approach speed — this
    // is the receipt check, guarding against any later init-order regression
    // reintroducing the TEMP 50 cm/s.
    const float ExpectedSpeed = Spawner->DemoMode_ApproachSpeed_CMPerSec;
    TestTrue(TEXT("actor received the Demo approach speed"),
        FMath::IsNearlyEqual(Target->MovementSpeed_CMPerSec, ExpectedSpeed, Tol));

    const FVector Before = Target->GetActorLocation();
    Target->Tick(1.0f);   // DemoMode_ApproachSpeed_CMPerSec for 1 s
    const FVector After = Target->GetActorLocation();

    TestTrue(TEXT("moved at the Demo approach speed"),
        FMath::IsNearlyEqual((Before - After).Size(), ExpectedSpeed, 0.5f));
    TestTrue(TEXT("closed along frame forward"),
        FMath::IsNearlyEqual(After.Y, ExpectedDist - ExpectedSpeed, 0.5f));
    TestTrue(TEXT("no lateral drift"), FMath::IsNearlyZero(After.X, 0.5f));
    TestTrue(TEXT("no vertical drift"), FMath::IsNearlyZero(After.Z, 0.5f));
    TestTrue(TEXT("depth tracks travel"),
        FMath::IsNearlyEqual(Target->GetDepthCM(), ExpectedDist - ExpectedSpeed, 0.5f));

    Target->ResolveAsVoid();
    TestTrue(TEXT("target resolves"), Target->IsResolved());

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
