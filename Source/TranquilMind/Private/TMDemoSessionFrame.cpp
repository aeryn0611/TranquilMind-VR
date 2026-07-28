// TranquilMind Demo Visual Pipeline — player-relative session coordinate frame.

#include "TMDemoSessionFrame.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"

#include "TMResearchSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogTMDemoSessionFrame, Log, All);

namespace TMDemoSessionFrameConst
{
    /** A heading within ~0.06 degrees of vertical projects to nothing. */
    constexpr float MinHorizontalLength = 1.0e-3f;
}

// ============================================================
//  LIFECYCLE
// ============================================================

bool UTMDemoSessionFrameSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    const UWorld* World = Cast<UWorld>(Outer);
    if (World == nullptr || !World->IsGameWorld())
    {
        return false;
    }

    // RESEARCH SAFETY (layer 1 of 2): do not exist outside Demo.
    //
    // This is evaluated at world-subsystem initialization, and the mode source
    // is valid that early by construction: GetEffectiveOperatingMode() reads
    // only (a) the tranquilmind.OperatingMode console variable, which is
    // registered at static-init when the module loads and is pre-applied by
    // DeviceProfileManager for -dpcvars, and (b) the UTMResearchSettings CDO, a
    // config=Game UDeveloperSettings loaded at class construction. Neither
    // depends on a UWorld, a GameMode, the SessionManager, or the map name — so
    // there is no ordering hazard here.
    //
    // Layer 2 is an unconditional refusal inside AdvanceCaptureWithSample(), so
    // even if this object were somehow created in a Research run it can never
    // capture, tick, or produce a side effect.
    return UTMResearchSettings::GetEffectiveOperatingMode() == ETMOperatingMode::Demo;
}

void UTMDemoSessionFrameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Emitted only when the subsystem is genuinely created, so the absence of
    // this line in a Research log is positive evidence of non-creation.
    UE_LOG(LogTMDemoSessionFrame, Warning,
        TEXT("[DemoFrame] Subsystem created (OperatingMode=Demo)."));
}

TStatId UTMDemoSessionFrameSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UTMDemoSessionFrameSubsystem, STATGROUP_Tickables);
}

void UTMDemoSessionFrameSubsystem::Tick(float DeltaTime)
{
    // Runs only while bAwaitingPose is set — i.e. between the first
    // "XR is initializing" classification and the latch. Self-terminating.
    EnsureCaptured();
}

// ============================================================
//  XR CLASSIFICATION
// ============================================================

ETMXRPoseStatus UTMDemoSessionFrameSubsystem::ClassifyXRPose(UWorld* World)
{
    if (World == nullptr || GEngine == nullptr || !GEngine->XRSystem.IsValid())
    {
        return ETMXRPoseStatus::NoXR;   // (C) no XR plugin at all
    }

    IXRTrackingSystem* XR = GEngine->XRSystem.Get();

    // IsHeadTrackingAllowedForWorld additionally rejects the non-VR PIE
    // instances during VR Preview, which IsHeadTrackingAllowed does not.
    if (!XR->IsHeadTrackingAllowedForWorld(*World))
    {
        return ETMXRPoseStatus::NoXR;   // (C) desktop PIE / headless / non-VR
    }

    // Head tracking being ALLOWED is not the same as the HMD currently
    // reporting a pose. These two calls are the actual evidence:
    //   IsTracking()    - the device is being tracked right now
    //   GetCurrentPose() - a pose was actually produced for it
    if (!XR->IsTracking(IXRTrackingSystem::HMDDeviceId))
    {
        return ETMXRPoseStatus::Initializing;   // (B) up, but not tracking yet
    }

    FQuat Orientation = FQuat::Identity;
    FVector Position = FVector::ZeroVector;
    if (!XR->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Orientation, Position))
    {
        return ETMXRPoseStatus::Initializing;   // (B) tracking, no pose yet
    }

    return ETMXRPoseStatus::Tracked;            // (A) genuinely usable
}

FTMDemoPoseSample UTMDemoSessionFrameSubsystem::SampleCurrentPose() const
{
    FTMDemoPoseSample Sample;

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return Sample;
    }

    Sample.NowSeconds = World->GetTimeSeconds();
    Sample.Status = ClassifyXRPose(World);

    if (const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
    {
        if (const APawn* Pawn = PC->GetPawn())
        {
            Sample.bHasPawn = true;
            Sample.PawnForward = Pawn->GetActorForwardVector();
            Sample.CameraLocation = Pawn->GetActorLocation();

            if (const UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
            {
                Sample.CameraLocation = Cam->GetComponentLocation();
                Sample.CameraForward = Cam->GetForwardVector();
            }
        }
    }

    return Sample;
}

// ============================================================
//  CAPTURE STATE MACHINE
// ============================================================

bool UTMDemoSessionFrameSubsystem::AdvanceCaptureWithSample(const FTMDemoPoseSample& Sample)
{
    if (Frame.bIsValid)
    {
        bAwaitingPose = false;
        return true;
    }

    // RESEARCH SAFETY (layer 2 of 2): refuse unconditionally, so this object is
    // inert even if it were created in a Research run.
    if (UTMResearchSettings::GetEffectiveOperatingMode() != ETMOperatingMode::Demo)
    {
        bAwaitingPose = false;
        return false;
    }

    // ---- (A) genuinely tracked: capture from the HMD camera ----
    if (Sample.Status == ETMXRPoseStatus::Tracked)
    {
        FVector Horizontal;
        if (TryMakeHorizontal(Sample.CameraForward, Horizontal))
        {
            BuildAndLatch(Sample.CameraLocation, Horizontal,
                ETMDemoFrameSource::HMDCamera, /*bTimedOut*/ false);
            return true;
        }

        // Tracked but the participant is looking almost straight up or down.
        // Waiting will not help — a vertical heading carries no yaw — so take
        // the deterministic pawn heading immediately.
        LatchFallback(Sample, /*bTimedOut*/ false);
        return true;
    }

    // ---- (C) genuinely non-XR: nothing to wait for ----
    if (Sample.Status == ETMXRPoseStatus::NoXR)
    {
        LatchFallback(Sample, /*bTimedOut*/ false);
        return true;
    }

    // ---- (B) XR present but the pose is not usable yet: bounded wait ----
    if (WaitStartedAt_SEC < 0.0f)
    {
        WaitStartedAt_SEC = Sample.NowSeconds;

        UE_LOG(LogTMDemoSessionFrame, Warning,
            TEXT("[DemoFrame] XR present but pose not ready — waiting up to %.1f s "
                 "before falling back."), PoseWaitBudget_SEC);
    }

    const float Elapsed = Sample.NowSeconds - WaitStartedAt_SEC;
    if (Elapsed >= PoseWaitBudget_SEC)
    {
        UE_LOG(LogTMDemoSessionFrame, Warning,
            TEXT("[DemoFrame] XR pose wait TIMED OUT after %.2f s — latching a "
                 "deterministic fallback heading."), Elapsed);

        LatchFallback(Sample, /*bTimedOut*/ true);
        return true;
    }

    // Keep ticking (and keep answering "not yet") until tracked or timed out.
    bAwaitingPose = true;
    return false;
}

bool UTMDemoSessionFrameSubsystem::EnsureCaptured()
{
    if (Frame.bIsValid)
    {
        return true;
    }

    return AdvanceCaptureWithSample(
        PoseSampleOverrideForTests ? PoseSampleOverrideForTests() : SampleCurrentPose());
}

void UTMDemoSessionFrameSubsystem::LatchFallback(const FTMDemoPoseSample& Sample, bool bTimedOut)
{
    FVector Horizontal;

    // Prefer the camera heading if it happens to be usable even when the pose
    // was not classified Tracked (e.g. an authored desktop-PIE camera).
    if (TryMakeHorizontal(Sample.CameraForward, Horizontal))
    {
        BuildAndLatch(Sample.CameraLocation, Horizontal,
            ETMDemoFrameSource::PawnFallback, bTimedOut);
        return;
    }

    if (Sample.bHasPawn && TryMakeHorizontal(Sample.PawnForward, Horizontal))
    {
        BuildAndLatch(Sample.CameraLocation, Horizontal,
            ETMDemoFrameSource::PawnFallback, bTimedOut);
        return;
    }

    BuildAndLatch(Sample.CameraLocation, FVector::ForwardVector,
        ETMDemoFrameSource::WorldAxisFallback, bTimedOut);
}

bool UTMDemoSessionFrameSubsystem::TryMakeHorizontal(const FVector& RawDirection, FVector& OutHorizontal)
{
    // Project onto the world-horizontal plane so head pitch and roll can never
    // tilt the Demo environment, then normalize.
    const FVector Flattened(RawDirection.X, RawDirection.Y, 0.0f);

    if (Flattened.Size() < TMDemoSessionFrameConst::MinHorizontalLength)
    {
        return false;
    }

    OutHorizontal = Flattened.GetSafeNormal();
    return !OutHorizontal.IsNearlyZero();
}

void UTMDemoSessionFrameSubsystem::BuildAndLatch(
    const FVector& OriginWorld,
    const FVector& HorizontalForward,
    ETMDemoFrameSource InSource,
    bool bTimedOut)
{
    Frame.OriginWorld = OriginWorld;
    Frame.ForwardWorld = HorizontalForward;
    Frame.UpWorld = FVector::UpVector;

    // Handedness, verified rather than assumed:
    // UE is left-handed, Z-up, +X forward, +Y right. FVector::CrossProduct uses
    // the standard determinant form, so with Forward=(1,0,0) and Up=(0,0,1):
    //   Up x Forward = (0*0 - 1*0, 1*1 - 0*0, 0*0 - 0*1) = (0,1,0) = +Y = right.
    // A positive Right offset therefore lands on the participant's real right.
    // (Locked by the Handedness and Orthonormality automation tests.)
    Frame.RightWorld = FVector::CrossProduct(Frame.UpWorld, Frame.ForwardWorld).GetSafeNormal();

    Frame.Source = InSource;
    Frame.bCaptureTimedOut = bTimedOut;
    Frame.bIsValid = true;
    Frame.CaptureOrdinal += 1;

    const UWorld* World = GetWorld();
    Frame.CaptureTimestamp_SEC = World != nullptr ? World->GetTimeSeconds() : -1.0f;

    bAwaitingPose = false;
    WaitStartedAt_SEC = -1.0f;

    const TCHAR* SourceText =
        InSource == ETMDemoFrameSource::HMDCamera ? TEXT("HMDCamera") :
        InSource == ETMDemoFrameSource::PawnFallback ? TEXT("PawnFallback") :
        TEXT("WorldAxisFallback");

    UE_LOG(LogTMDemoSessionFrame, Warning,
        TEXT("[DemoFrame] Captured #%d | Source=%s | TimedOut=%s | "
             "EnvOrigin=(%.1f, %.1f, %.1f) | Fwd=(%.3f, %.3f, %.3f) | "
             "Right=(%.3f, %.3f, %.3f) | T=%.2f"),
        Frame.CaptureOrdinal, SourceText, bTimedOut ? TEXT("YES") : TEXT("no"),
        Frame.OriginWorld.X, Frame.OriginWorld.Y, Frame.OriginWorld.Z,
        Frame.ForwardWorld.X, Frame.ForwardWorld.Y, Frame.ForwardWorld.Z,
        Frame.RightWorld.X, Frame.RightWorld.Y, Frame.RightWorld.Z,
        Frame.CaptureTimestamp_SEC);

    // Notify consumers (e.g. a pending spawn request) exactly once per latch.
    // Broadcast LAST, with the frame fully valid, so handlers can read it.
    OnDemoFrameCaptured.Broadcast();
}

bool UTMDemoSessionFrameSubsystem::CaptureFromPose(
    const FVector& OriginWorld,
    const FVector& RawForwardWorld)
{
    if (Frame.bIsValid)
    {
        return false;   // immutable until ResetForNewSession()
    }

    FVector Horizontal;
    if (!TryMakeHorizontal(RawForwardWorld, Horizontal))
    {
        return false;
    }

    BuildAndLatch(OriginWorld, Horizontal, ETMDemoFrameSource::HMDCamera, /*bTimedOut*/ false);
    return true;
}

void UTMDemoSessionFrameSubsystem::ResetForNewSession()
{
    if (!Frame.bIsValid)
    {
        return;
    }

    UE_LOG(LogTMDemoSessionFrame, Warning,
        TEXT("[DemoFrame] Reset after capture #%d — one further capture permitted."),
        Frame.CaptureOrdinal);

    Frame.bIsValid = false;
    Frame.Source = ETMDemoFrameSource::None;
    Frame.bCaptureTimedOut = false;
    bAwaitingPose = false;
    WaitStartedAt_SEC = -1.0f;
    // CaptureOrdinal is intentionally preserved so the next capture reads #2.
}

// ============================================================
//  QUERY / CONVERSION
// ============================================================

FVector UTMDemoSessionFrameSubsystem::GetCurrentCameraLocationOrOrigin() const
{
    if (const UWorld* World = GetWorld())
    {
        if (const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
        {
            if (const APawn* Pawn = PC->GetPawn())
            {
                if (const UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
                {
                    return Cam->GetComponentLocation();
                }

                return Pawn->GetActorLocation();
            }
        }
    }

    return Frame.OriginWorld;
}

FVector UTMDemoSessionFrameSubsystem::DemoEnvironmentPointToWorld(
    float ForwardCm, float RightCm, float UpCm) const
{
    if (!Frame.bIsValid)
    {
        return FVector::ZeroVector;
    }

    return Frame.OriginWorld
        + Frame.ForwardWorld * ForwardCm
        + Frame.RightWorld * RightCm
        + Frame.UpWorld * UpCm;
}

FVector UTMDemoSessionFrameSubsystem::DemoActiveSpawnPointToWorld(
    float ForwardCm, float RightCm, float UpCm) const
{
    if (!Frame.bIsValid)
    {
        return FVector::ZeroVector;
    }

    // Origin tracks the participant; orientation does not. Leaning or walking
    // preserves the comfort distance, while head rotation cannot swing the
    // target around the room.
    return DemoActiveSpawnPointFrom(
        GetCurrentCameraLocationOrOrigin(), ForwardCm, RightCm, UpCm);
}

FVector UTMDemoSessionFrameSubsystem::DemoActiveSpawnPointFrom(
    const FVector& CameraWorld, float ForwardCm, float RightCm, float UpCm) const
{
    if (!Frame.bIsValid)
    {
        return FVector::ZeroVector;
    }

    return CameraWorld
        + Frame.ForwardWorld * ForwardCm
        + Frame.RightWorld * RightCm
        + Frame.UpWorld * UpCm;
}

FVector UTMDemoSessionFrameSubsystem::DemoLocalDirectionToWorld(
    float ForwardAmount, float RightAmount, float UpAmount) const
{
    if (!Frame.bIsValid)
    {
        return FVector::ZeroVector;
    }

    return (Frame.ForwardWorld * ForwardAmount
        + Frame.RightWorld * RightAmount
        + Frame.UpWorld * UpAmount).GetSafeNormal();
}
