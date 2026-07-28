// TranquilMind Demo Visual Pipeline — presentation-layer motion rig.

#include "TMVisualMotionComponent.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"
#include "TMStage1Trace.h"

DEFINE_LOG_CATEGORY_STATIC(LogTMVisualMotion, Log, All);

namespace TranquilMindVisualMotion
{
    // Frequency ratios relative to the primary float frequency. Chosen as
    // irrational-ish values so the summed sines never visibly repeat — the
    // motion reads as alive rather than as an oscillation loop.
    constexpr float SecondaryFloatRatio = 1.618f;  // ~golden ratio
    constexpr float LateralDriftRatio = 0.53f;     // slower than the float
    constexpr float RotationRatio = 0.71f;

    // Weighting of the two float sines (sums to 1 so amplitude stays bounded).
    constexpr float PrimaryFloatWeight = 0.65f;
    constexpr float SecondaryFloatWeight = 0.35f;

    // SplitMix64-style integer hash: cheap, deterministic, well distributed.
    // Used ONCE per BeginMotion to derive phase offsets. Not an RNG stream.
    inline uint64 HashSeed(uint64 X)
    {
        X += 0x9E3779B97F4A7C15ull;
        X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
        X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
        return X ^ (X >> 31);
    }
}

UTMVisualMotionComponent::UTMVisualMotionComponent()
{
    // Inert by default: motion is strictly opt-in via BeginMotion(). Research
    // Mode never opts in, so this component's mere presence on an actor is
    // guaranteed to be transform-neutral and tick-free.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    SetMobility(EComponentMobility::Movable);
}

void UTMVisualMotionComponent::BeginMotion(int32 InPhaseSeed)
{
    using namespace TranquilMindVisualMotion;

    // Two deterministic hash rounds: round A feeds float/drift phases, round B
    // feeds independent rotation phases. Hashed once here; no RNG stream is
    // consumed and no per-frame randomness exists.
    const uint64 HashA = HashSeed(static_cast<uint64>(static_cast<uint32>(InPhaseSeed)));
    const uint64 HashB = HashSeed(HashA);

    const auto PhaseFromChunk = [](uint64 Hash, int32 ChunkIndex) -> float
    {
        const uint64 Bits = (Hash >> (ChunkIndex * 16)) & 0xFFFFull;
        return (static_cast<float>(Bits) / 65535.0f) * 2.0f * UE_PI;
    };

    PhaseOffsets_RAD[0] = PhaseFromChunk(HashA, 0);  // float primary
    PhaseOffsets_RAD[1] = PhaseFromChunk(HashA, 1);  // float secondary
    PhaseOffsets_RAD[2] = PhaseFromChunk(HashA, 2);  // drift X
    PhaseOffsets_RAD[3] = PhaseFromChunk(HashA, 3);  // drift Y
    PhaseOffsets_RAD[4] = PhaseFromChunk(HashB, 0);  // rotation pitch
    PhaseOffsets_RAD[5] = PhaseFromChunk(HashB, 1);  // rotation roll

    LocalTime_SEC = 0.0f;
    bMotionActive = true;

    // Cache the t=0 unit values: the spawn envelope blends each channel away
    // from zero along its OWN sine trajectory (see ApplyMotionTransform).
    InitialUnits = ComputeUnitChannels(0.0f);

    // Apply the t=0 state immediately (ease-in start scale, zero offset) so
    // the first rendered frame never pops at full size before the envelope.
    ApplyMotionTransform();

    SetComponentTickEnabled(true);

    // Kit §4 site 3 — real state transition. BeginMotion is armed only by the
    // Environment Demo profile, so this never fires on the Research path.
    FTMStage1Trace::LogPhase(GetOwner(), TEXT("inert"), TEXT("motion"));
}

void UTMVisualMotionComponent::BeginVisualExit()
{
    if (bExiting)
    {
        return;   // idempotent while running
    }

    bExiting = true;
    ExitElapsed_SEC = 0.0f;

    FTMStage1Trace::LogPhase(GetOwner(), TEXT("motion"), TEXT("exit"));   // Kit §4 site 3

    // The exit must tick even if the owner armed it in an edge state.
    SetComponentTickEnabled(true);
}

void UTMVisualMotionComponent::ForceInert()
{
    bMotionActive = false;
    LocalTime_SEC = 0.0f;
    bExiting = false;
    ExitElapsed_SEC = 0.0f;

    SetComponentTickEnabled(false);

    // Identity relative transform: presentation layer contributes nothing.
    SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
    SetRelativeScale3D(FVector::OneVector);
}

void UTMVisualMotionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bMotionActive && !bExiting)
    {
        return;
    }

    LocalTime_SEC += DeltaTime;

    if (bExiting)
    {
        ExitElapsed_SEC += DeltaTime;
    }

    ApplyMotionTransform();

    if (bExiting)
    {
        const float Duration = FMath::Max(MotionParams.ExitDuration_SEC, KINDA_SMALL_NUMBER);
        const float Alpha = FMath::Clamp(ExitElapsed_SEC / Duration, 0.0f, 1.0f);
        const float Smooth = FMath::SmoothStep(0.0f, 1.0f, Alpha);

        // Kit §4 sites 6 and 10, logged before the broadcast so the trace reads
        // in causal order: EXIT_70 / DELEGATE_PROGRESS, then the MAT_WRITEs the
        // handler performs. LogExit70 self-limits to one emission per cycle.
        if (Alpha >= 0.70f)
        {
            FTMStage1Trace::LogExit70(GetOwner(), ExitElapsed_SEC * 1000.0f);
        }
        FTMStage1Trace::LogProgress(GetOwner(), Smooth);

        OnVisualExitProgress.Broadcast(Smooth);

        // Kit §4 site 15 — SNAP for this frame, taken AFTER the handler's
        // material writes and after ApplyMotionTransform, but BEFORE any
        // teardown, so it records the state that was actually presented.
        TMStage1_Snapshot();

        if (Alpha >= 1.0f)
        {
            // Order matters: clear the flag first so a completion handler that
            // destroys the owner cannot re-enter a live exit.
            bExiting = false;

            // Kit §4 site 11 — logged BEFORE the broadcast: the bound handler
            // calls Destroy() on the owner, so this is the last moment the
            // actor is safely loggable.
            FTMStage1Trace::LogComplete(GetOwner());

            OnVisualExitComplete.Broadcast();
        }
    }
    else
    {
        TMStage1_Snapshot();
    }
}

void UTMVisualMotionComponent::TMStage1_Snapshot()
{
    // STAGE 1 INSTRUMENTATION (log-only). This lives on the motion component
    // rather than the target actor because ATranquilMindTargetActor calls
    // SetActorTickEnabled(false) in FinishResolution() BEFORE the exit begins —
    // the actor does not tick during the exact window Stage 1 must observe.
    // The component tick is unaffected by SetActorTickEnabled and runs for the
    // whole exit, which is why it is the correct host for the per-frame record.
    //
    // Research safety: reaching here requires bMotionActive or bExiting, and
    // Research forces the component inert with tick disabled, so the Research
    // path never executes this.
    AActor* Owner = GetOwner();
    if (Owner == nullptr)
    {
        return;
    }

    FTMStage1Trace::TickSnapshot(
        Owner,
        Owner->FindComponentByClass<UMeshComponent>(),
        this);
}

UTMVisualMotionComponent::FChannelUnits
UTMVisualMotionComponent::ComputeUnitChannels(float T) const
{
    using namespace TranquilMindVisualMotion;

    const float BaseOmega =
        2.0f * UE_PI / FMath::Max(MotionParams.FloatPeriod_SEC, KINDA_SMALL_NUMBER);
    const float DriftOmega = BaseOmega * LateralDriftRatio;
    const float RotOmega = BaseOmega * RotationRatio;

    FChannelUnits Units;

    // Idle float: two incommensurate sines on local Z.
    Units.FloatZ =
        PrimaryFloatWeight * FMath::Sin(BaseOmega * T + PhaseOffsets_RAD[0]) +
        SecondaryFloatWeight * FMath::Sin(BaseOmega * SecondaryFloatRatio * T + PhaseOffsets_RAD[1]);

    // Lateral drift: slower, decorrelated per axis.
    Units.DriftX = FMath::Sin(DriftOmega * T + PhaseOffsets_RAD[2]);
    Units.DriftY = FMath::Sin(DriftOmega * T + PhaseOffsets_RAD[3]);

    // Barely perceptible rotation: independent phases per axis.
    Units.Pitch = FMath::Sin(RotOmega * T + PhaseOffsets_RAD[4]);
    Units.Roll = FMath::Sin(RotOmega * T + PhaseOffsets_RAD[5]);

    return Units;
}

void UTMVisualMotionComponent::ApplyMotionTransform()
{
    const FTMVisualMotionParams& P = MotionParams;
    const float T = LocalTime_SEC;

    // ---- Spawn ease-in envelope (visual only; gameplay timing is untouched) ----
    // Scale eases from the start floor to 1 with a cubic ease-out.
    float EnvelopeAlpha = 1.0f;
    if (P.SpawnEaseInDuration_SEC > KINDA_SMALL_NUMBER)
    {
        EnvelopeAlpha = FMath::Clamp(T / P.SpawnEaseInDuration_SEC, 0.0f, 1.0f);
    }
    const float OffsetEnvelope = FMath::SmoothStep(0.0f, 1.0f, EnvelopeAlpha);
    const float ScaleEase = 1.0f - FMath::Cube(1.0f - EnvelopeAlpha);
    float Scale = FMath::Lerp(P.SpawnEaseInStartScale, 1.0f, ScaleEase);

    // Visual exit: a gentle settle toward ExitEndScaleFactor composed onto the
    // spawn-ease scale. Never a collapse — the fade (owner-side) carries the
    // "no longer there" reading; the scale only softens it.
    if (bExiting)
    {
        const float ExitDuration = FMath::Max(P.ExitDuration_SEC, KINDA_SMALL_NUMBER);
        const float ExitAlpha = FMath::Clamp(ExitElapsed_SEC / ExitDuration, 0.0f, 1.0f);
        Scale *= FMath::Lerp(1.0f, P.ExitEndScaleFactor,
            FMath::SmoothStep(0.0f, 1.0f, ExitAlpha));
    }

    // ---- Phase-preserving spawn blend ----
    // offset(t) = A * (U(t) - U(0) * (1 - envelope))
    // At t=0 the offset is exactly zero (no pop). As the envelope reaches 1
    // the correction vanishes and the channel follows its pure sine. Because
    // smoothstep has zero slope at t=0, the initial velocity is A * U'(0) —
    // fully phase-determined, so consecutive bubbles begin moving along their
    // OWN trajectories instead of replaying a shared opening ramp.
    const FChannelUnits Units = ComputeUnitChannels(T);
    const float Fade = 1.0f - OffsetEnvelope;

    const float FloatZ_CM =
        P.VerticalFloatAmplitude_CM * (Units.FloatZ - InitialUnits.FloatZ * Fade);
    const float DriftX_CM =
        P.LateralDriftAmplitude_CM * (Units.DriftX - InitialUnits.DriftX * Fade);
    const float DriftY_CM =
        P.LateralDriftAmplitude_CM * (Units.DriftY - InitialUnits.DriftY * Fade);
    const float Pitch_DEG =
        P.RotationAmplitude_DEG * (Units.Pitch - InitialUnits.Pitch * Fade);
    const float Roll_DEG =
        P.RotationAmplitude_DEG * (Units.Roll - InitialUnits.Roll * Fade);

    SetRelativeLocationAndRotation(
        FVector(DriftX_CM, DriftY_CM, FloatZ_CM),
        FRotator(Pitch_DEG, 0.0f, Roll_DEG));
    SetRelativeScale3D(FVector(Scale));
}
