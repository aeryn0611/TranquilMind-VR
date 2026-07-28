// Post-resolve visual exit (Pass 1A.2): gameplay resolves at the original
// instant; only the presentation lingers ~0.35 s, then the actor destroys once.
//
// The exit is gated on the presentation rig being ACTIVE (BeginMotion), which
// in production is armed exclusively by the Environment demo profile — so the
// tests arm motion explicitly, and the Research/legacy path is asserted to
// keep the original immediate destroy.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "TMDemoSessionFrame.h"
#include "TMVisualMotionComponent.h"
#include "TargetSpawnerComponent.h"
#include "../../TranquilMindSessionManager.h"
#include "../../TranquilMindTypes.h"
#include "../../Public/Private/Private/TranquilMindTargetActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMVisualExitTestUtil
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

        int32 CountLiveTargets() const
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

    /** Drive the presentation rig's tick for N seconds of fixed steps. */
    void TickMotion(ATranquilMindTargetActor* Target, float Seconds)
    {
        const float Step = 1.0f / 60.0f;
        const int32 Steps = FMath::CeilToInt(Seconds / Step);
        for (int32 i = 0; i < Steps && IsValid(Target); ++i)
        {
            Target->VisualMotion->TickComponent(Step, LEVELTICK_All, nullptr);
        }
    }
}

// ============================================================
//  Demo Environment presentation: full exit lifecycle
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMVisualExitLifecycleTest,
    "TranquilMind.Demo.VisualExit.Lifecycle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMVisualExitLifecycleTest::RunTest(const FString&)
{
    using namespace TMVisualExitTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);

    UTMDemoSessionFrameSubsystem* Frame =
        W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
    Frame->CaptureFromPose(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f));

    W.World->TimeSeconds = 20.0;
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("one target spawned"), Spawner->ActiveTargets.Num(), 1);

    ATranquilMindTargetActor* Target = Spawner->ActiveTargets[0].Get();
    TestNotNull(TEXT("target valid"), Target);

    // Arm the Environment presentation (in production BeginMotion is called by
    // the profile-gated InitializeTarget branch; scratch worlds have no map
    // name, so arm it explicitly — the exit gate is IsMotionActive()).
    Target->VisualMotion->BeginMotion(7);
    TestTrue(TEXT("presentation armed"), Target->VisualMotion->IsMotionActive());

    // Outcomes are recorded into the CURRENT BLOCK ledger immediately; session
    // totals aggregate only at block rollover, so the block ledger is the
    // correct instant-recording evidence.
    const FBlockStats Before = SM->GetCurrentBlockStats();

    // (1) Resolve: the outcome must be recorded IMMEDIATELY, not after the fade.
    Target->ResolveAsVoid();
    const FBlockStats AfterResolve = SM->GetCurrentBlockStats();
    TestEqual(TEXT("(1) Void recorded at resolution instant"),
        AfterResolve.N_Void, Before.N_Void + 1);
    TestTrue(TEXT("resolved flag set"), Target->IsResolved());

    // (2) STAGE 2: deregistration now happens AT THE OUTCOME LOCK, inside the
    // resolve path itself (Spec §4.3 / §5 step 4) — no sweep is needed and none
    // is called here. CleanupResolvedTargets() survives only as a safety net.
    TestEqual(TEXT("(2) ActiveTargets cleared at the outcome lock, not by a sweep"),
        Spawner->ActiveTargets.Num(), 0);

    // (4) Actor stays visually alive during the exit window...
    TestTrue(TEXT("exit running"), Target->VisualMotion->IsVisualExitActive());
    TickMotion(Target, 0.20f);
    TestTrue(TEXT("(4) actor alive mid-exit"), IsValid(Target));
    TestEqual(TEXT("still one live actor mid-exit"), W.CountLiveTargets(), 1);

    // (3) ...but a trigger or void during the fade cannot record again.
    Target->ResolveAsTriggered(21.0);
    Target->ResolveAsVoid();
    const FBlockStats DuringFade = SM->GetCurrentBlockStats();
    TestEqual(TEXT("(3) no second outcome from trigger during fade"),
        DuringFade.Trial_Count_Total, AfterResolve.Trial_Count_Total);
    TestEqual(TEXT("(3) no second Void during fade"),
        DuringFade.N_Void, AfterResolve.N_Void);
    TestEqual(TEXT("(3) no Hit from trigger during fade"),
        DuringFade.N_Hit, AfterResolve.N_Hit);

    // (6) STAGE 2 — THIS ASSERTION IS DELIBERATELY INVERTED.
    //
    // Before Stage 2 this test asserted that a successor target COULD be
    // created while the predecessor was still visually fading, because the
    // spawn gate read ActiveTargets (a scoring set the predecessor leaves at
    // resolution) instead of presentation visibility. Stage 1 §3.3 confirmed
    // that is exactly defect C7, measured as visiblepresentations = 2 while
    // activetargets = 1.
    //
    // Spec §4.9 now requires the opposite: no successor official presentation
    // may become visible while the predecessor official presentation is still
    // visibly alive. The spawn must be REFUSED here.
    TestTrue(TEXT("(6) predecessor still owns the official presentation"),
        Spawner->HasLiveDemoPresentation());

    Spawner->SpawnNextTarget();

    TestEqual(TEXT("(6) successor spawn REFUSED during predecessor exit"),
        Spawner->ActiveTargets.Num(), 0);
    TestEqual(TEXT("(6) no second presentation was created"),
        W.CountLiveTargets(), 1);
    TestEqual(TEXT("(6) the fading predecessor still owns the presentation"),
        Spawner->GetLiveDemoPresentation(), Target);

    // (5) Exit completes -> destroyed exactly once, and ownership self-clears.
    TickMotion(Target, 0.30f);
    TestFalse(TEXT("(5) faded actor destroyed after exit"), IsValid(Target));
    TestEqual(TEXT("(5) no live actors remain"), W.CountLiveTargets(), 0);
    TestFalse(TEXT("(5) presentation ownership released at Phase 9"),
        Spawner->HasLiveDemoPresentation());

    // (6b) Once the predecessor is gone the gate opens and the successor
    // spawns. The gate delays a spawn; it never cancels one.
    Spawner->SpawnNextTarget();
    TestEqual(TEXT("(6b) successor spawns once the predecessor has ended"),
        Spawner->ActiveTargets.Num(), 1);
    TestFalse(TEXT("(6b) new target is unresolved"),
        Spawner->ActiveTargets[0]->IsResolved());

    // (7) The refused attempt consumed no stimulus draw: first target was GO,
    // the replacement is still the alternation sequence's second entry, NOGO.
    TestEqual(TEXT("(7) sequence unperturbed by the refused spawn (second is NOGO)"),
        Spawner->ActiveTargets[0]->GetStimulusType(), ETMStimulusType::NoGo);

    // (8) Cadence: the replacement target resolves and cycles normally.
    ATranquilMindTargetActor* Second = Spawner->ActiveTargets[0].Get();
    Second->ResolveAsVoid();
    TestEqual(TEXT("(8) deregistered at the outcome lock, without a sweep"),
        Spawner->ActiveTargets.Num(), 0);

    return true;
}

// ============================================================
//  Legacy / Research path: no exit, immediate destroy
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMVisualExitInertPathsTest,
    "TranquilMind.Demo.VisualExit.InertWithoutPresentation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMVisualExitInertPathsTest::RunTest(const FString&)
{
    using namespace TMVisualExitTestUtil;

    FScopedWorld W(ETMOperatingMode::Demo);

    ATranquilMindSessionManager* SM = nullptr;
    UTargetSpawnerComponent* Spawner = nullptr;
    BuildRuntimeInPhaseII(W, SM, Spawner);

    UTMDemoSessionFrameSubsystem* Frame =
        W.World->GetSubsystem<UTMDemoSessionFrameSubsystem>();
    Frame->CaptureFromPose(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f));

    W.World->TimeSeconds = 20.0;
    Spawner->SpawnNextTarget();
    ATranquilMindTargetActor* Target = Spawner->ActiveTargets[0].Get();
    TestNotNull(TEXT("target valid"), Target);

    // (9) Presentation rig NOT armed (Void-map demo / Research-shaped state):
    // resolution must destroy immediately, exactly as before this pass.
    TestFalse(TEXT("motion not armed in scratch world"),
        Target->VisualMotion->IsMotionActive());

    Target->ResolveAsVoid();
    TestFalse(TEXT("(9) immediate destroy without presentation"), IsValid(Target));
    TestEqual(TEXT("no lingering actors"), W.CountLiveTargets(), 0);

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
