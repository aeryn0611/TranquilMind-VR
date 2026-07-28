// TranquilMind Stage 2 — Demo presentation lifecycle. See the header for the
// architecture rationale and the Research-safety argument.

#include "TMDemoLifecycleComponent.h"

#include "Engine/World.h"
#include "TMStage1Trace.h"
#include "../Public/Private/Private/TranquilMindTargetActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogTMDemoLifecycle, Log, All);

namespace TMDemoLifecycle
{
    const TCHAR* PhaseName(ETMDemoPresentationPhase P)
    {
        switch (P)
        {
        case ETMDemoPresentationPhase::Spawn:            return TEXT("p1_spawn");
        case ETMDemoPresentationPhase::Entrance:         return TEXT("p2_entrance");
        case ETMDemoPresentationPhase::ReadableHold:     return TEXT("p3_hold");
        case ETMDemoPresentationPhase::ApproachResponse: return TEXT("p4_response");
        case ETMDemoPresentationPhase::OutcomeLock:      return TEXT("p5_outcomelock");
        case ETMDemoPresentationPhase::Persistence:      return TEXT("p6_persistence");
        case ETMDemoPresentationPhase::Exit:             return TEXT("p7_exit");
        case ETMDemoPresentationPhase::Hidden:           return TEXT("p8_hidden");
        case ETMDemoPresentationPhase::Destroyed:        return TEXT("p9_destroy");
        case ETMDemoPresentationPhase::None:
        default:                                         return TEXT("p0_none");
        }
    }
}

UTMDemoLifecycleComponent::UTMDemoLifecycleComponent()
{
    // Inert by default. The phase machine runs only after BeginPresentation(),
    // which the Research path never calls.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ATranquilMindTargetActor* UTMDemoLifecycleComponent::GetTargetOwner() const
{
    return Cast<ATranquilMindTargetActor>(GetOwner());
}

float UTMDemoLifecycleComponent::GetDemoTrialOnsetTime_SEC() const
{
    // Spec §3: start of Phase 4, AFTER Entrance and Readable Hold.
    return PresentationSpawnTime_SEC
        + (Timing.Entrance_MS + Timing.ReadableHold_MS) * 0.001f;
}

float UTMDemoLifecycleComponent::GetScheduledPhase4End_SEC() const
{
    // Spec §3: fixed, independent of any response.
    return GetDemoTrialOnsetTime_SEC() + Timing.ScoredResponse_MS * 0.001f;
}

float UTMDemoLifecycleComponent::GetScheduledVisualLifetime_MS() const
{
    return Timing.Entrance_MS + Timing.ReadableHold_MS + Timing.ScoredResponse_MS
        + Timing.Persistence_MS + Timing.Exit_MS;
}

void UTMDemoLifecycleComponent::BeginPresentation(
    float InPresentationSpawnTime_SEC,
    const FTMDemoLifecycleTiming& InTiming)
{
    if (bArmed)
    {
        return;   // one presentation per actor; never re-armed
    }

    Timing                   = InTiming;
    PresentationSpawnTime_SEC = InPresentationSpawnTime_SEC;
    bArmed                   = true;
    bOutcomeCommitted        = false;
    bCancelled               = false;
    bExitEnvelopeRunning     = false;

    SetComponentTickEnabled(true);

    EnterPhase(ETMDemoPresentationPhase::Spawn);

    // Traverse every zero-duration phase synchronously. Under the Stage 2
    // Compatibility timing (Entrance 0, Readable Hold 0) this lands in Phase 4
    // before the call returns, so ActiveTargets registration happens on the
    // spawn frame — identical to the current behaviour.
    const UWorld* World = GetWorld();
    AdvancePhases(World != nullptr ? World->GetTimeSeconds() : PresentationSpawnTime_SEC);
}

void UTMDemoLifecycleComponent::ForceInert()
{
    bArmed               = false;
    bOutcomeCommitted    = false;
    bCancelled           = false;
    bExitEnvelopeRunning = false;
    Phase                = ETMDemoPresentationPhase::None;

    SetComponentTickEnabled(false);
}

void UTMDemoLifecycleComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bArmed)
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    AdvancePhases(World->GetTimeSeconds());
}

void UTMDemoLifecycleComponent::NotifyOutcomeCommitted(bool bCancellation)
{
    if (!bArmed || bOutcomeCommitted)
    {
        return;   // exactly once (Spec §5 step 2)
    }

    bOutcomeCommitted = true;
    bCancelled        = bCancellation;

    // Phase 5 is entered at the RESOLUTION instant, which for an early accepted
    // response precedes ScheduledPhase4End. Input closes here and the actor is
    // deregistered from ActiveTargets here (Spec §5 steps 3-4).
    EnterPhase(ETMDemoPresentationPhase::OutcomeLock);

    if (bAdvancing)
    {
        // Called from inside AdvancePhases (natural expiry). The outer loop
        // continues and will take the scheduled 5 -> 6 -> 7 transitions itself.
        return;
    }

    const UWorld* World = GetWorld();
    AdvancePhases(World != nullptr ? World->GetTimeSeconds() : GetScheduledPhase4End_SEC());
}

void UTMDemoLifecycleComponent::NotifyExitEnvelopeComplete()
{
    if (!bArmed || !bExitEnvelopeRunning)
    {
        return;   // exactly once
    }

    bExitEnvelopeRunning = false;

    const UWorld* World = GetWorld();
    AdvancePhases(World != nullptr ? World->GetTimeSeconds() : 0.0f);
}

void UTMDemoLifecycleComponent::EnterPhase(ETMDemoPresentationPhase NewPhase)
{
    if (Phase == NewPhase)
    {
        return;
    }

    const ETMDemoPresentationPhase From = Phase;
    Phase = NewPhase;

    // Schema-2 trace: every line already carries (sid, spawnseq), so the phase
    // timeline is validatable per presentation with no relabelling risk.
    FTMStage1Trace::LogPhase(
        GetOwner(),
        TMDemoLifecycle::PhaseName(From),
        TMDemoLifecycle::PhaseName(NewPhase));

    ATranquilMindTargetActor* Target = GetTargetOwner();
    if (Target == nullptr)
    {
        return;
    }

    switch (NewPhase)
    {
    case ETMDemoPresentationPhase::ApproachResponse:
        // Spec §4.3: registered in ActiveTargets at Phase 4 entry, and only here.
        Target->HandleLifecycleEnterPhase4();
        break;

    case ETMDemoPresentationPhase::OutcomeLock:
        // Spec §4.3 / §5 step 4: deregistered at the outcome lock, immediately.
        Target->HandleLifecycleOutcomeLock();
        break;

    default:
        break;
    }
}

void UTMDemoLifecycleComponent::AdvancePhases(float NowSeconds)
{
    if (bAdvancing || !bArmed)
    {
        return;
    }

    TGuardValue<bool> AdvanceGuard(bAdvancing, true);

    const float T_EntranceEnd = PresentationSpawnTime_SEC + Timing.Entrance_MS * 0.001f;
    const float T_HoldEnd     = GetDemoTrialOnsetTime_SEC();
    const float T_Phase4End   = GetScheduledPhase4End_SEC();
    const float T_PersistEnd  = T_Phase4End + Timing.Persistence_MS * 0.001f;

    // Bounded: every iteration either advances the phase or breaks.
    for (int32 Guard = 0; Guard < 16; ++Guard)
    {
        switch (Phase)
        {
        case ETMDemoPresentationPhase::Spawn:
            EnterPhase(ETMDemoPresentationPhase::Entrance);
            continue;

        case ETMDemoPresentationPhase::Entrance:
            if (NowSeconds >= T_EntranceEnd)
            {
                EnterPhase(ETMDemoPresentationPhase::ReadableHold);
                continue;
            }
            return;

        case ETMDemoPresentationPhase::ReadableHold:
            if (NowSeconds >= T_HoldEnd)
            {
                EnterPhase(ETMDemoPresentationPhase::ApproachResponse);
                continue;
            }
            return;

        case ETMDemoPresentationPhase::ApproachResponse:
            if (NowSeconds >= T_Phase4End)
            {
                // Natural expiry. The actor records the outcome and calls back
                // into NotifyOutcomeCommitted(false), which sets Phase 5; the
                // guard above stops that call from recursing here.
                if (ATranquilMindTargetActor* Target = GetTargetOwner())
                {
                    Target->HandleLifecycleResponseWindowExpired();
                }

                if (Phase == ETMDemoPresentationPhase::ApproachResponse)
                {
                    // Owner declined to resolve (already resolved, or gone).
                    // Do not strand the presentation in Phase 4.
                    bOutcomeCommitted = true;
                    EnterPhase(ETMDemoPresentationPhase::OutcomeLock);
                }
                continue;
            }
            return;

        case ETMDemoPresentationPhase::OutcomeLock:
            // Spec §5 steps 6-7: Persistence and Exit begin from SCHEDULED
            // presentation time, never from the moment a response arrived.
            // A Void is a session cancellation, not a scored response, and ends
            // the presentation promptly exactly as it does today.
            if (bCancelled || NowSeconds >= T_Phase4End)
            {
                EnterPhase(ETMDemoPresentationPhase::Persistence);
                continue;
            }
            return;

        case ETMDemoPresentationPhase::Persistence:
            if (bCancelled || NowSeconds >= T_PersistEnd)
            {
                EnterPhase(ETMDemoPresentationPhase::Exit);

                // C1 RETAINED: the 350 ms envelope is unchanged, both in form
                // and in duration. Only the decision of WHEN it starts has
                // moved here from FinishResolution().
                bExitEnvelopeRunning = false;
                if (ATranquilMindTargetActor* Target = GetTargetOwner())
                {
                    bExitEnvelopeRunning = Target->HandleLifecycleEnterExit();
                }
                continue;
            }
            return;

        case ETMDemoPresentationPhase::Exit:
            if (bExitEnvelopeRunning)
            {
                return;   // NotifyExitEnvelopeComplete() resumes from here
            }
            EnterPhase(ETMDemoPresentationPhase::Hidden);
            continue;

        case ETMDemoPresentationPhase::Hidden:
            // STAGE 2: Hidden exists and is traversed, but has ZERO duration and
            // performs no visibility write. Spec §4.6's hard alpha clamp,
            // visibility-off and >=1 full Hidden frame belong to Stage 3, and
            // introducing them here would change the rendered frame sequence
            // away from the Stage 1 baseline.
            EnterPhase(ETMDemoPresentationPhase::Destroyed);
            continue;

        case ETMDemoPresentationPhase::Destroyed:
        {
            // Phase 9 is terminal. Disarm BEFORE destroying so nothing re-enters
            // through EndPlay, and so LiveDemoPresentation release is driven by
            // the actor actually dying.
            bArmed = false;
            SetComponentTickEnabled(false);

            if (ATranquilMindTargetActor* Target = GetTargetOwner())
            {
                Target->HandleLifecycleDestroy();
            }
            return;
        }

        case ETMDemoPresentationPhase::None:
        default:
            return;
        }
    }

    UE_LOG(LogTMDemoLifecycle, Error,
        TEXT("[Lifecycle] Phase advance guard tripped at %s — presentation halted."),
        TMDemoLifecycle::PhaseName(Phase));
}
