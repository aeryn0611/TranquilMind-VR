// Research-integrity regression lock for the Demo spawn path.
//
// UTargetSpawnerComponent::HandleHardGateResumed is bound to
// SessionManager::OnHardGateResumed in BOTH operating modes. Before the guards
// in RestartTrial()/SpawnNextTarget(), a gaze hard-gate resume during a
// Research block reached SpawnNextTarget() and produced a self-scoring Demo
// target at the Research stimulus location whose outcome never appeared in the
// JSONL. These tests drive the REAL gaze hard-gate cycle in a live world.
//
// The Demo test is the positive control: it uses the identical harness and
// asserts a target IS produced, which is what makes the Research zero-result
// meaningful rather than an artefact of a harness that never fired.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreMiscDefines.h"

#include "TargetSpawnerComponent.h"
#include "TMResearchRunner.h"
#include "../../TranquilMindSessionManager.h"
#include "../../TranquilMindTypes.h"
#include "../../Public/Private/Private/TranquilMindTargetActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMSpawnContainmentTestUtil
{
    constexpr float TickStep_SEC = 1.0f / 60.0f;

    /** Off world +X by 90 degrees — far outside the 10 degree CPT zone. */
    const FVector GazeOffCenter(0.0f, 1.0f, 0.0f);

    /** World +X — the hard gate's hard-coded centre direction. */
    const FVector GazeOnCenter(1.0f, 0.0f, 0.0f);

    /** Scratch world with actors initialized and BeginPlay dispatched. */
    struct FScopedTestWorld
    {
        UWorld* World = nullptr;

        explicit FScopedTestWorld(ETMOperatingMode Mode)
        {
            // The spawner latches OperatingMode in BeginPlay, so the CVar must
            // be set before the world begins play.
            if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                    TEXT("tranquilmind.OperatingMode")))
            {
                CVar->Set(Mode == ETMOperatingMode::Research ? 1 : 0, ECVF_SetByCode);
            }

            World = UWorld::CreateWorld(EWorldType::Game, false);
            FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
            Ctx.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());

            // A scratch world has no GameMode, so UWorld::BeginPlay() never
            // reaches AGameStateBase::HandleBeginPlay() and bBegunPlay stays
            // false — which means actors spawned below would never receive
            // BeginPlay and would silently keep their header defaults
            // (OperatingMode = Demo). Set the flag explicitly so SpawnActor
            // dispatches BeginPlay and the mode is genuinely latched.
            World->SetBegunPlay(true);
        }

        ~FScopedTestWorld()
        {
            if (World != nullptr)
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
            }

            if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                    TEXT("tranquilmind.OperatingMode")))
            {
                CVar->Set(-1, ECVF_SetByCode);   // restore "use settings"
            }
        }

        /**
         * Advance simulated time at a fixed step, feeding a constant gaze
         * direction the way the VR pawn does.
         *
         * Two details this harness has to get right, both of which silently
         * neuter the test if missed:
         *  - A scratch world does not register actor tick functions, so
         *    World->Tick() never reaches SessionManager::Tick. The manager is
         *    therefore ticked directly.
         *  - SmoothenGazeVector derives its smoothing alpha from
         *    GetWorld()->GetDeltaSeconds(), NOT from its DeltaTime argument.
         *    Left at 0 the alpha is 0, the smoothed gaze can never leave
         *    centre, and the hard gate could never trip. The world delta is
         *    therefore set explicitly to the same fixed step.
         */
        void AdvanceWithGaze(float Seconds, ATranquilMindSessionManager* SM, const FVector& Gaze)
        {
            const int32 Steps = FMath::CeilToInt(Seconds / TickStep_SEC);
            for (int32 i = 0; i < Steps; ++i)
            {
                World->DeltaTimeSeconds = TickStep_SEC;
                SM->UpdateGazeVector(Gaze);
                SM->Tick(TickStep_SEC);
            }
        }

        int32 CountTargetActors() const
        {
            int32 Count = 0;
            for (TActorIterator<ATranquilMindTargetActor> It(World); It; ++It)
            {
                ++Count;
            }
            return Count;
        }
    };

    /** Spawn a SessionManager plus a bare actor carrying a TargetSpawner. */
    void BuildRuntime(
        UWorld* World,
        ATranquilMindSessionManager*& OutSM,
        UTargetSpawnerComponent*& OutSpawner)
    {
        OutSM = World->SpawnActor<ATranquilMindSessionManager>();

        AActor* Host = World->SpawnActor<AActor>();
        OutSpawner = NewObject<UTargetSpawnerComponent>(Host);
        OutSpawner->RegisterComponent();
        OutSpawner->RegisterSessionManager(OutSM);
    }
}

// ============================================================
//  RESEARCH — a hard-gate resume must create nothing
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMResearchHardGateSpawnContainmentTest,
    "TranquilMind.Research.SpawnContainment.HardGateResume",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMResearchHardGateSpawnContainmentTest::RunTest(const FString&)
{
    using namespace TMSpawnContainmentTestUtil;

    FScopedTestWorld Scoped(ETMOperatingMode::Research);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntime(Scoped.World, SM, Spawner);

    // Sanity: BeginPlay must actually have run, or nothing below proves
    // anything. Checked via a value that is NOT merely the header default —
    // SessionID is empty until BeginPlay initializes the session record.
    TestTrue(TEXT("session manager BeginPlay ran (session record initialized)"),
        SM->HasActorBegunPlay());
    TestTrue(TEXT("spawner BeginPlay ran (component registered and begun)"),
        Spawner->HasBegunPlay());

    // Reach Phase II: gaze/hard-gate ticking begins there (12 s master clock).
    Scoped.AdvanceWithGaze(13.0f, SM, GazeOnCenter);
    TestEqual(TEXT("reached Phase II core training"),
        SM->GetCurrentPhase(), ETMSessionPhase::Phase_II_CoreTraining);

    const FSessionStats StatsBefore = SM->GetSessionStats();
    const int32 TargetsBeforeGate = Scoped.CountTargetActors();

    // Trip the hard gate: >3 s of off-centre gaze (plus smoothing headroom).
    Scoped.AdvanceWithGaze(6.0f, SM, GazeOffCenter);
    TestEqual(TEXT("hard gate tripped by off-centre gaze"),
        SM->GetActiveInterrupt(), ETMInterruptType::HardGate_GazeLost);

    // Resume: >3 s dwell back on centre. This broadcasts OnHardGateResumed,
    // which is the delegate that used to reach SpawnNextTarget().
    Scoped.AdvanceWithGaze(6.0f, SM, GazeOnCenter);
    TestEqual(TEXT("hard gate resumed"),
        SM->GetActiveInterrupt(), ETMInterruptType::None);

    // (2) No Demo target actor may exist.
    TestEqual(TEXT("no target actors created by the resume"),
        Scoped.CountTargetActors(), TargetsBeforeGate);

    // (3) ActiveTargets must remain empty.
    TestEqual(TEXT("ActiveTargets remains empty"), Spawner->ActiveTargets.Num(), 0);

    // (4)+(5) The outcome ledger must not move: no phantom trial.
    const FSessionStats StatsAfter = SM->GetSessionStats();
    TestEqual(TEXT("ledger total trials unchanged"),
        StatsAfter.Total_Trial_Count_Total, StatsBefore.Total_Trial_Count_Total);
    TestEqual(TEXT("ledger omissions unchanged"),
        StatsAfter.Total_N_Omission, StatsBefore.Total_N_Omission);
    TestEqual(TEXT("ledger correct rejections unchanged"),
        StatsAfter.Total_N_CorrectRejection, StatsBefore.Total_N_CorrectRejection);

    // Direct calls must also refuse, independent of the delegate path.
    Spawner->RestartTrial();
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("direct RestartTrial/SpawnNextTarget create nothing in Research"),
        Scoped.CountTargetActors(), TargetsBeforeGate);
    TestEqual(TEXT("ActiveTargets still empty after direct calls"),
        Spawner->ActiveTargets.Num(), 0);

    return true;
}

// ============================================================
//  DEMO — positive control: the same harness must still spawn
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMDemoHardGateStillSpawnsTest,
    "TranquilMind.Demo.SpawnContainment.HardGateResumeStillSpawns",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMDemoHardGateStillSpawnsTest::RunTest(const FString&)
{
    using namespace TMSpawnContainmentTestUtil;

    FScopedTestWorld Scoped(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntime(Scoped.World, SM, Spawner);

    Scoped.AdvanceWithGaze(13.0f, SM, GazeOnCenter);
    TestEqual(TEXT("reached Phase II core training"),
        SM->GetCurrentPhase(), ETMSessionPhase::Phase_II_CoreTraining);

    // (7) Demo must be unaffected: an explicit RestartTrial still produces a
    // target. This also proves the harness genuinely drives the spawn path,
    // so the Research test's zero-result is a real containment result.
    Spawner->DestroyAllActiveTargetsAsVoid();
    Spawner->RestartTrial();

    TestTrue(TEXT("Demo RestartTrial still spawns a target"),
        Spawner->ActiveTargets.Num() > 0);
    TestTrue(TEXT("Demo target actor exists in the world"),
        Scoped.CountTargetActors() > 0);

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
