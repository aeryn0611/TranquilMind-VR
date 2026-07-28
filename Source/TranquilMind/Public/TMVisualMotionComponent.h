// TranquilMind Demo Visual Pipeline — presentation-layer motion rig.
//
// UTMVisualMotionComponent is a reusable, generic visual-motion capability:
// any visual object attached BENEATH this component inherits calm, almost
// subliminal idle motion (float / drift / rotation) plus a visual-only spawn
// ease-in envelope. Bubble targets are only its first use case; future demo
// visuals (floating panels, guidance bubbles, tutorial objects) can reuse it
// unchanged.
//
// ARCHITECTURE CONTRACT (do not violate):
//  - This component owns ONLY its own relative transform. It never writes any
//    other component's transform and never reads gameplay state.
//  - Gameplay must never read this component's (or its children's) transform;
//    gameplay owns the stable parent/anchor transform. Visual offset here is
//    presentation only and composes on top of gameplay travel via the child
//    relative transform.
//  - Research Mode safety: the component is INERT BY DEFAULT (tick disabled,
//    identity relative transform, no RNG consumption). Motion runs only after
//    an explicit BeginMotion() call, which the Research path never makes.
//    ForceInert() restores the inert state as an additional safety layer.
//  - Determinism: per-instance phase variation comes from a caller-supplied
//    seed hashed once at BeginMotion(). No FRandomStream / FMath::Rand use,
//    no frame-by-frame randomness — nothing that could perturb experiment
//    reproducibility or draw from shared RNG streams.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TMVisualMotionComponent.generated.h"

/**
 * Tuning parameters for the presentation-layer idle motion.
 *
 * Aesthetic guardrails (calm, physically plausible, almost subliminal):
 * float 0.5–1.5 cm, drift 0.25–1.0 cm, period 6–12 s, rotation 0.25–1.0 deg,
 * spawn ease-in 0.3–0.6 s. Values outside these ranges tend to read as
 * game-like hovering, which this project explicitly avoids.
 */
USTRUCT(BlueprintType)
struct FTMVisualMotionParams
{
    GENERATED_BODY()

    /** Peak vertical (world-Z-aligned local) float amplitude. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion",
        meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "cm"))
    float VerticalFloatAmplitude_CM = 1.0f;

    /** Peak lateral drift amplitude (local X/Y). Slower than the float. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion",
        meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "cm"))
    float LateralDriftAmplitude_CM = 0.5f;

    /** Primary float period. Secondary frequencies derive from this via
     *  incommensurate ratios so the motion never visibly loops. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion",
        meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "s"))
    float FloatPeriod_SEC = 9.0f;

    /** Peak rotation amplitude per axis. Keep sub-degree for bubbles. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion",
        meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "deg"))
    float RotationAmplitude_DEG = 0.5f;

    /** Duration of the visual-only spawn ease-in envelope. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion|SpawnEaseIn",
        meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float SpawnEaseInDuration_SEC = 0.35f;

    /**
     * Relative-scale floor at spawn. MUST stay well above zero: the gameplay
     * response window starts at spawn time regardless of this animation, and a
     * scale-from-zero target would be sub-perceptible for a meaningful fraction
     * of its response window (false Omission risk). 0.6 keeps the target
     * immediately recognizable while still reading as a soft arrival.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion|SpawnEaseIn",
        meta = (ClampMin = "0.25", ClampMax = "1.0"))
    float SpawnEaseInStartScale = 0.6f;

    /** Duration of the post-resolve visual exit ("gently releasing"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion|VisualExit",
        meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float ExitDuration_SEC = 0.35f;

    /** Relative-scale factor at exit end. Deliberately close to 1 — a gentle
     *  settle, never a collapse to zero. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion|VisualExit",
        meta = (ClampMin = "0.50", ClampMax = "1.0"))
    float ExitEndScaleFactor = 0.94f;
};

/** Smoothed 0..1 exit progress, broadcast once per tick during the exit. */
DECLARE_MULTICAST_DELEGATE_OneParam(FTMVisualExitProgress, float);
/** Fired exactly once when the exit envelope completes. */
DECLARE_MULTICAST_DELEGATE(FTMVisualExitComplete);

/**
 * Generic presentation-layer motion rig (Demo Visual Pipeline).
 *
 * Attach visuals beneath this component; call BeginMotion() to start the idle
 * motion + spawn ease-in, ForceInert() to hard-stop and zero everything.
 * Inert by default — safe to exist on actors used by Research Mode.
 */
UCLASS(ClassGroup = (TranquilMind), meta = (BlueprintSpawnableComponent))
class TRANQUILMIND_API UTMVisualMotionComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UTMVisualMotionComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

// ============================================================
//  TUNING
// ============================================================

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|VisualMotion")
    FTMVisualMotionParams MotionParams;

// ============================================================
//  PUBLIC API
// ============================================================

public:
    /**
     * Start idle motion and the spawn ease-in envelope.
     * @param InPhaseSeed  Deterministic per-instance seed (e.g. spawn-position
     *                     hash mixed with a spawn counter). Hashed once here;
     *                     no RNG stream is consumed.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|VisualMotion")
    void BeginMotion(int32 InPhaseSeed);

    /**
     * Hard-stop: disable tick and restore the identity relative transform
     * (location 0, rotation 0, scale 1). Called by Research initialization as
     * an explicit safety layer on top of the inert-by-default construction.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|VisualMotion")
    void ForceInert();

    UFUNCTION(BlueprintPure, Category = "TranquilMind|VisualMotion")
    bool IsMotionActive() const { return bMotionActive; }

// ============================================================
//  VISUAL EXIT (Pass 1A.2) — transform-only "gently releasing" envelope.
//  The component animates ONLY its relative scale (contract preserved);
//  material fading is the OWNER's job via OnVisualExitProgress. Research
//  never reaches this: exits are begun by owners only when motion is active,
//  and ForceInert() clears any exit state outright.
// ============================================================

public:
    /**
     * Begin the post-resolve exit envelope: relative scale eases from its
     * current value toward ExitEndScaleFactor over ExitDuration_SEC while the
     * idle motion keeps drifting in place. Broadcasts OnVisualExitProgress
     * (smoothed 0..1) each tick and OnVisualExitComplete exactly once at the
     * end. Idempotent while an exit is already running.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|VisualMotion")
    void BeginVisualExit();

    UFUNCTION(BlueprintPure, Category = "TranquilMind|VisualMotion")
    bool IsVisualExitActive() const { return bExiting; }

    FTMVisualExitProgress OnVisualExitProgress;
    FTMVisualExitComplete OnVisualExitComplete;

// ============================================================
//  INTERNAL
// ============================================================

private:
    /** Unit (amplitude-less) sine values for every motion channel at one instant. */
    struct FChannelUnits
    {
        float FloatZ = 0.0f;
        float DriftX = 0.0f;
        float DriftY = 0.0f;
        float Pitch = 0.0f;
        float Roll = 0.0f;
    };

    /** Evaluate the unit channel values at time T (seconds since BeginMotion). */
    FChannelUnits ComputeUnitChannels(float T) const;

    /** Evaluate and apply the relative transform for the current LocalTime. */
    void ApplyMotionTransform();

    /**
     * Stage 1 instrumentation only (log-only, no state change). Hosted here
     * because the owning actor's tick is disabled before the exit begins, so
     * the component tick is the only per-frame hook alive across the window
     * Spec §11 requires observing.
     */
    void TMStage1_Snapshot();

    bool bMotionActive = false;

    /** Seconds since BeginMotion(). Presentation-local clock only. */
    float LocalTime_SEC = 0.0f;

    /**
     * Per-instance phase offsets [radians], derived once from the seed via two
     * deterministic hash rounds: [0..1] float, [2..3] drift X/Y, [4..5]
     * rotation pitch/roll. Every channel owns an independent phase.
     */
    float PhaseOffsets_RAD[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    /**
     * Unit channel values at t=0, cached by BeginMotion(). The spawn envelope
     * blends each channel from zero along its OWN trajectory:
     *   offset(t) = A * (U(t) - U(0) * (1 - envelope(t)))
     * so consecutive bubbles never replay a shared opening ramp.
     */
    FChannelUnits InitialUnits;

    // ---- Visual exit state ----
    bool bExiting = false;
    float ExitElapsed_SEC = 0.0f;
};
