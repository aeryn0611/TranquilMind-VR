// TranquilMind Stage 2 — Demo presentation lifecycle.
//
// Owns the nine-phase presentation state machine defined by
// TranquilMind_Implementation_Spec.md §4.1, at the Stage 2 Compatibility timing
// of §4.7 (Entrance 0 / Readable Hold 0 / Scored Response 2500 / Persistence 0 /
// Exit 350, Cadence 3000).
//
// WHY THIS IS A COMPONENT AND NOT THE ACTOR TICK
//
//  ATranquilMindTargetActor::FinishResolution() calls SetActorTickEnabled(false)
//  at the outcome lock. Stage 1 §3.2 measured the consequence: the actor
//  transform is bit-identical across every exit frame — the approach freeze is
//  a direct product of that call. Hosting the phase clock on the actor tick
//  would mean keeping that tick alive and re-gating travel inside it, putting a
//  proven-correct behaviour at risk for no benefit (Spec §2 C3 forbids changing
//  apparent motion in Stage 2). A component tick is unaffected by
//  SetActorTickEnabled and runs for the whole presentation, which is exactly
//  what UTMVisualMotionComponent already relies on.
//
//  UTMVisualMotionComponent itself is NOT the right host: its header states an
//  explicit contract that it is a generic, reusable motion rig which never reads
//  gameplay state. A Demo-trial phase machine is gameplay state.
//
// RESEARCH SAFETY (R1-R7)
//
//  Inert by default: tick disabled, phase None, no callbacks. Motion is armed
//  only by BeginPresentation(), which is called from exactly one site —
//  UTargetSpawnerComponent::SpawnNextTarget()'s bDemoMode block, which is itself
//  behind the Research operating-mode guard. InitializeResearchVisual() calls
//  ForceInert() as an explicit second layer. No RNG of any kind is used here,
//  so R5 (seeded determinism) and R7 (Research random streams) cannot be
//  perturbed by this file.
//
// SCOPE (Stage 2)
//
//  Phase 8 (Hidden) EXISTS as a state and is traversed, but is entered and left
//  within a single frame: no SetVisibility(false), no SetHiddenInGame(true), no
//  deferred destroy. Giving Hidden a real frame is Spec §4.6 / Stage 3, and is
//  deliberately NOT done here — the rendered frame sequence must stay identical
//  to the Stage 1 baseline.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TMDemoLifecycleComponent.generated.h"

class ATranquilMindTargetActor;

/**
 * The nine logical phases of Spec §4.1. Entrance, Readable Hold and Persistence
 * are zero-duration under the Stage 2 Compatibility configuration but exist as
 * explicit states with explicit transitions.
 */
UENUM(BlueprintType)
enum class ETMDemoPresentationPhase : uint8
{
    /** Not armed. Research targets and any non-Demo path stay here forever. */
    None             = 0  UMETA(DisplayName = "None"),

    Spawn            = 1  UMETA(DisplayName = "1 Spawn"),
    Entrance         = 2  UMETA(DisplayName = "2 Entrance"),
    ReadableHold     = 3  UMETA(DisplayName = "3 Readable Hold"),
    ApproachResponse = 4  UMETA(DisplayName = "4 Approach + Response"),
    OutcomeLock      = 5  UMETA(DisplayName = "5 Outcome Lock"),
    Persistence      = 6  UMETA(DisplayName = "6 Persistence"),
    Exit             = 7  UMETA(DisplayName = "7 Exit"),
    Hidden           = 8  UMETA(DisplayName = "8 Hidden"),
    Destroyed        = 9  UMETA(DisplayName = "9 Destroy"),
};

/**
 * Phase durations. Defaults are the Spec §4.7 "Stage 2 Compatibility" column,
 * which is normative for Stage 2 and must not be replaced with A-prime.
 *
 * These are data so the zero-duration phases can be given a real duration in
 * automation tests — that is how Entrance / Readable Hold / Persistence are
 * proven to exist and to discard input, without changing anything a user sees.
 */
USTRUCT(BlueprintType)
struct FTMDemoLifecycleTiming
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Lifecycle",
        meta = (ClampMin = "0.0", Units = "Milliseconds"))
    float Entrance_MS = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Lifecycle",
        meta = (ClampMin = "0.0", Units = "Milliseconds"))
    float ReadableHold_MS = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Lifecycle",
        meta = (ClampMin = "1.0", Units = "Milliseconds"))
    float ScoredResponse_MS = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Lifecycle",
        meta = (ClampMin = "0.0", Units = "Milliseconds"))
    float Persistence_MS = 0.0f;

    /** Informational for Stage 2: the exit envelope's real duration is owned by
     *  UTMVisualMotionComponent (C1, retained). Used only when no envelope is
     *  armed, where the exit is instantaneous exactly as it is today. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Lifecycle",
        meta = (ClampMin = "0.0", Units = "Milliseconds"))
    float Exit_MS = 350.0f;
};

/**
 * Demo-only presentation lifecycle. One per target actor, inert until armed.
 *
 * OWNERSHIP BOUNDARY — this component decides WHEN a phase begins. It never:
 *   - records an outcome            (ATranquilMindTargetActor)
 *   - writes a material parameter   (ATranquilMindTargetActor)
 *   - writes a transform or scale   (UTMVisualMotionComponent)
 *   - mutates ActiveTargets         (UTargetSpawnerComponent)
 */
UCLASS(ClassGroup = (TranquilMind), meta = (BlueprintSpawnableComponent))
class TRANQUILMIND_API UTMDemoLifecycleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTMDemoLifecycleComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

// ============================================================
//  ARMING
// ============================================================

public:
    /**
     * Arm the presentation. Phase 1 (Spawn) is entered immediately and every
     * zero-duration phase is traversed synchronously, so under the Stage 2
     * Compatibility timing the presentation is in Phase 4 before this call
     * returns — which is what keeps ActiveTargets registration on the spawn
     * frame, exactly as it is today.
     *
     * @param InPresentationSpawnTime_SEC  Spec §3 PresentationSpawnTime (t = 0).
     */
    void BeginPresentation(float InPresentationSpawnTime_SEC, const FTMDemoLifecycleTiming& InTiming);

    /** Hard-stop: tick off, phase None, no further callbacks. Research safety. */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Lifecycle")
    void ForceInert();

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Lifecycle")
    bool IsArmed() const { return bArmed; }

// ============================================================
//  STATE
// ============================================================

public:
    UFUNCTION(BlueprintPure, Category = "TranquilMind|Lifecycle")
    ETMDemoPresentationPhase GetPhase() const { return Phase; }

    /**
     * Spec §4.2 / §5 steps 1-5: input is accepted ONLY in Phase 4, and only
     * until the first valid input has been accepted. Everything else is
     * discarded at the point of receipt.
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|Lifecycle")
    bool IsInputOpen() const
    {
        return bArmed && Phase == ETMDemoPresentationPhase::ApproachResponse && !bOutcomeCommitted;
    }

    /** Spec §3 clocks. All are absolute world seconds. */
    float GetPresentationSpawnTime_SEC() const { return PresentationSpawnTime_SEC; }
    float GetDemoTrialOnsetTime_SEC() const;
    float GetScheduledPhase4End_SEC() const;

    /** Scheduled visual lifetime in ms — independent of any reaction time. */
    float GetScheduledVisualLifetime_MS() const;

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Lifecycle")
    bool HasOutcomeCommitted() const { return bOutcomeCommitted; }

    const FTMDemoLifecycleTiming& GetTiming() const { return Timing; }

// ============================================================
//  NOTIFICATIONS FROM THE OWNING ACTOR
// ============================================================

public:
    /**
     * The owner committed an outcome. Called exactly once per presentation from
     * ATranquilMindTargetActor::FinishResolution(), which every resolve path
     * funnels through.
     *
     * @param bCancellation  true for a Void — a session cancellation (hard gate,
     *   phase change, restart, demo end, teardown), which ends the presentation
     *   promptly exactly as it does today. false for a SCORED outcome (Hit /
     *   Commission / Omission / CorrectRejection), which locks the outcome
     *   immediately but leaves the presentation running to ScheduledPhase4End
     *   per Spec §5 steps 6-8.
     */
    void NotifyOutcomeCommitted(bool bCancellation);

    /** The 350 ms exit envelope finished (C1, retained). Drives 7 -> 8 -> 9. */
    void NotifyExitEnvelopeComplete();

private:
    void AdvancePhases(float NowSeconds);
    void EnterPhase(ETMDemoPresentationPhase NewPhase);
    ATranquilMindTargetActor* GetTargetOwner() const;

    UPROPERTY(VisibleInstanceOnly, Category = "TranquilMind|Lifecycle")
    ETMDemoPresentationPhase Phase = ETMDemoPresentationPhase::None;

    UPROPERTY(VisibleInstanceOnly, Category = "TranquilMind|Lifecycle")
    FTMDemoLifecycleTiming Timing;

    bool  bArmed = false;

    /** Set at the outcome lock; makes acceptance of a second input impossible. */
    bool  bOutcomeCommitted = false;

    /** A Void ends the presentation promptly instead of at the schedule. */
    bool  bCancelled = false;

    /** True while the exit envelope owns the presentation, so a second
     *  completion notification cannot re-enter phases 8 and 9. */
    bool  bExitEnvelopeRunning = false;

    /** Re-entrancy guard: EnterPhase() calls into the owner, which can call back. */
    bool  bAdvancing = false;

    float PresentationSpawnTime_SEC = 0.0f;
};
