// TranquilMind Demo Visual Pipeline — player-relative session coordinate frame.
//
// A single Demo-only authority holding an IMMUTABLE orientation basis captured
// once per session from the participant's real head pose, plus the capture-time
// origin used as the fixed environment anchor.
//
// ORIGIN vs ORIENTATION — deliberately separated:
//   - Orientation (Forward/Right/Up) is immutable for the whole session. Head
//     rotation after capture must never rotate the environment or the lanes.
//   - The capture-time origin is the fixed ENVIRONMENT origin, for ambient
//     bubbles and cloud wisps that should stay put in the room.
//   - The ACTIVE bubble measures its comfort distance from the CURRENT camera
//     position instead, so seated leaning or physical translation cannot
//     invalidate it. It still uses the immutable orientation, so the target
//     never chases head rotation.
//
// WHY A WORLD SUBSYSTEM: one per world == one per Demo session, reachable by
// every future presentation system without coupling it to the spawner, and it
// is NOT an actor — so it does not enlarge the actor population that
// ATranquilMindVRPawn enumerates with GetAllActorsOfClass(AActor) on the
// input path.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TMDemoSessionFrame.generated.h"

/** Where a captured heading came from. Diagnostics. */
UENUM(BlueprintType)
enum class ETMDemoFrameSource : uint8
{
    None              UMETA(DisplayName = "None"),
    /** Normal path: a genuinely tracked HMD pose. */
    HMDCamera         UMETA(DisplayName = "HMD Camera"),
    /** Camera unusable/near-vertical, or non-XR: used the pawn's heading. */
    PawnFallback      UMETA(DisplayName = "Pawn Fallback"),
    /** Nothing usable at all: world +X. Headless only. */
    WorldAxisFallback UMETA(DisplayName = "World Axis Fallback")
};

/**
 * Whether the XR head pose can be trusted right now.
 *
 * IsHeadTrackingAllowedForWorld() alone is NOT proof of a usable pose — it says
 * the world is a VR world and head tracking is permitted, which is already true
 * while the runtime is still bringing the session up. Distinguishing
 * "initializing" from "genuinely non-XR" is what stops the first spawn from
 * permanently latching a fallback on a frame where Quest simply was not ready.
 */
UENUM(BlueprintType)
enum class ETMXRPoseStatus : uint8
{
    /** (C) No XR system, or head tracking not allowed for this world. Desktop
     *  PIE, headless, or a genuinely non-XR run. Fall back immediately — there
     *  is nothing to wait for. */
    NoXR         UMETA(DisplayName = "No XR"),

    /** (B) XR is present and allowed, but the HMD is not reporting a pose yet
     *  (or tracking was momentarily lost). Wait — bounded. */
    Initializing UMETA(DisplayName = "Initializing"),

    /** (A) The HMD is tracking and returned a pose. Capture. */
    Tracked      UMETA(DisplayName = "Tracked")
};

/** One resolved sample of the pose environment. Injectable for tests. */
USTRUCT(BlueprintType)
struct FTMDemoPoseSample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "TranquilMind|DemoFrame")
    ETMXRPoseStatus Status = ETMXRPoseStatus::NoXR;

    UPROPERTY(BlueprintReadWrite, Category = "TranquilMind|DemoFrame")
    FVector CameraLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "TranquilMind|DemoFrame")
    FVector CameraForward = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "TranquilMind|DemoFrame")
    FVector PawnForward = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "TranquilMind|DemoFrame")
    bool bHasPawn = false;

    /** Seconds used for the bounded-wait deadline. */
    UPROPERTY(BlueprintReadWrite, Category = "TranquilMind|DemoFrame")
    float NowSeconds = 0.0f;
};

/** Immutable orientation basis plus the fixed environment origin. */
USTRUCT(BlueprintType)
struct FTMDemoSessionFrame
{
    GENERATED_BODY()

    /** Capture-time camera position. The fixed ENVIRONMENT origin — ambient
     *  content anchors here. The active bubble does NOT. */
    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    FVector OriginWorld = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    FVector ForwardWorld = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    FVector RightWorld = FVector::RightVector;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    FVector UpWorld = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    float CaptureTimestamp_SEC = -1.0f;

    /** 1 for the first capture, 2 after a reset, and so on. */
    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    int32 CaptureOrdinal = 0;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    ETMDemoFrameSource Source = ETMDemoFrameSource::None;

    /** True when a fallback was latched because the bounded XR wait expired,
     *  as opposed to being classified non-XR up front. */
    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|DemoFrame")
    bool bCaptureTimedOut = false;
};

/**
 * Demo-only coordinate authority. Ticks ONLY while waiting for a first usable
 * XR pose, then stops permanently.
 */
UCLASS()
class TRANQUILMIND_API UTMDemoSessionFrameSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Fired exactly once per successful capture (valid pose, pawn fallback,
     * or timeout fallback — every latch funnels through BuildAndLatch). The
     * spawner uses this to consume a pending spawn request. The subsystem
     * itself never constructs gameplay objects.
     */
    FSimpleMulticastDelegate OnDemoFrameCaptured;

    /**
     * TEST SEAM: when bound, EnsureCaptured() uses this instead of the live
     * XR/pawn sample, so the Initializing wait, the timeout, and the pending
     * spawn handshake can be driven deterministically without a headset.
     * Never set in production code.
     */
    TFunction<FTMDemoPoseSample()> PoseSampleOverrideForTests;

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // -- Tickable: alive only during the bounded pose wait ------------------
    virtual bool IsTickable() const override { return bAwaitingPose; }
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

// ============================================================
//  TUNING
// ============================================================

public:
    /** Bounded wait for a first usable XR pose before latching a fallback. */
    static constexpr float PoseWaitBudget_SEC = 5.0f;

// ============================================================
//  CAPTURE
// ============================================================

public:
    /**
     * Capture if not already valid, sampling the live XR/pawn environment.
     * Cheap and safe to call repeatedly — a no-op once latched.
     *
     * @return true when a valid frame exists afterwards. False means "still
     *         waiting for a usable XR pose" — the caller should use its own
     *         fallback for this frame and try again later.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|DemoFrame")
    bool EnsureCaptured();

    /**
     * The capture state machine, with the environment sample supplied. This is
     * the same code path EnsureCaptured() runs; it is exposed so the bounded
     * wait, the timeout, and the XR classifications can be driven
     * deterministically without a live headset.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|DemoFrame")
    bool AdvanceCaptureWithSample(const FTMDemoPoseSample& Sample);

    /** Resolve the live XR + pawn environment into a sample. */
    FTMDemoPoseSample SampleCurrentPose() const;

    /** Classify the live XR pose. Public so callers can log/branch on it. */
    static ETMXRPoseStatus ClassifyXRPose(UWorld* World);

    /**
     * Capture from an explicit pose, honouring the immutability latch. Used by
     * an intentional recenter and by tests.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|DemoFrame")
    bool CaptureFromPose(const FVector& OriginWorld, const FVector& RawForwardWorld);

    /** Invalidate so exactly one further capture may occur. */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|DemoFrame")
    void ResetForNewSession();

// ============================================================
//  QUERY
// ============================================================

public:
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    bool IsDemoSessionFrameValid() const { return Frame.bIsValid; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    const FTMDemoSessionFrame& GetDemoSessionFrame() const { return Frame; }

    /** The immutable capture-time ENVIRONMENT origin. */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector GetDemoSessionOrigin() const { return Frame.OriginWorld; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector GetDemoSessionForward() const { return Frame.ForwardWorld; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector GetDemoSessionRight() const { return Frame.RightWorld; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector GetDemoSessionUp() const { return Frame.UpWorld; }

    /** Current camera position, or the environment origin if unavailable. */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector GetCurrentCameraLocationOrOrigin() const;

// ============================================================
//  CONVERSION HELPERS
//  Callers must use these rather than rebuilding the basis themselves.
// ============================================================

public:
    /**
     * ENVIRONMENT placement (ambient bubbles, cloud wisps): frame-local offsets
     * from the IMMUTABLE capture-time origin. Stays put when the player moves.
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector DemoEnvironmentPointToWorld(float ForwardCm, float RightCm, float UpCm) const;

    /**
     * ACTIVE bubble placement: frame-local offsets from the CURRENT camera
     * position, using the immutable orientation. Comfort distance therefore
     * survives leaning and physical translation, while the target still never
     * chases head rotation.
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector DemoActiveSpawnPointToWorld(float ForwardCm, float RightCm, float UpCm) const;

    /**
     * Same as DemoActiveSpawnPointToWorld but with the camera position supplied
     * explicitly. The origin/orientation split is the whole point of this
     * function: CameraWorld moves the spawn origin, the immutable session basis
     * supplies every direction.
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector DemoActiveSpawnPointFrom(const FVector& CameraWorld,
                                     float ForwardCm, float RightCm, float UpCm) const;

    /** Frame-local direction -> normalized world direction. Origin-independent. */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|DemoFrame")
    FVector DemoLocalDirectionToWorld(float ForwardAmount, float RightAmount, float UpAmount) const;

// ============================================================
//  INTERNAL
// ============================================================

private:
    void BuildAndLatch(const FVector& OriginWorld, const FVector& HorizontalForward,
                       ETMDemoFrameSource InSource, bool bTimedOut);

    /** Latch the best non-HMD heading available from the sample. */
    void LatchFallback(const FTMDemoPoseSample& Sample, bool bTimedOut);

    static bool TryMakeHorizontal(const FVector& RawDirection, FVector& OutHorizontal);

    FTMDemoSessionFrame Frame;

    /** True only between the first "Initializing" classification and the latch;
     *  gates IsTickable() so the subsystem ticks for a few frames at most. */
    bool bAwaitingPose = false;

    /** Seconds at which the bounded wait began; negative until it starts. */
    float WaitStartedAt_SEC = -1.0f;
};
