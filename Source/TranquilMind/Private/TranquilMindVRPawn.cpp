// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindVRPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "TargetSpawnerComponent.h"
#include "../TranquilMindSessionManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindVRPawn, Log, All);

ATranquilMindVRPawn::ATranquilMindVRPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    AutoPossessPlayer = EAutoReceiveInput::Player0;

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootScene);

    VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
    VROrigin->SetupAttachment(RootScene);

    VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
    VRCamera->SetupAttachment(VROrigin);
    VRCamera->bLockToHmd = true;

    LeftMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMotionController"));
    LeftMotionController->SetupAttachment(VROrigin);
    LeftMotionController->SetTrackingMotionSource(FName(TEXT("Left")));

    RightMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightMotionController"));
    RightMotionController->SetupAttachment(VROrigin);
    RightMotionController->SetTrackingMotionSource(FName(TEXT("Right")));

    SessionManager = nullptr;
    TargetSpawner = nullptr;

    bUseHeadForwardAsGazeFallback = true;
    bLogAutoBinding = true;
}

void ATranquilMindVRPawn::BeginPlay()
{
    Super::BeginPlay();

    AutoBindRuntimeReferences();

    UE_LOG(
        LogTranquilMindVRPawn,
        Log,
        TEXT("[VRPawn] BeginPlay | SessionManager=%s | TargetSpawner=%s"),
        IsValid(SessionManager.Get()) ? *SessionManager->GetName() : TEXT("None"),
        IsValid(TargetSpawner.Get()) ? *TargetSpawner->GetName() : TEXT("None"));
}

void ATranquilMindVRPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // One-time diagnostic: log VRCamera world location and forward vector
    static bool bCamLocLogged = false;
    if (!bCamLocLogged && IsValid(VRCamera.Get()))
    {
        const FVector Loc = VRCamera->GetComponentLocation();
        const FVector Fwd = VRCamera->GetForwardVector();
        UE_LOG(LogTranquilMindVRPawn, Warning,
            TEXT("[VRPawn] CamWorld=(%.1f,%.1f,%.1f) Fwd=(%.2f,%.2f,%.2f)"),
            Loc.X, Loc.Y, Loc.Z, Fwd.X, Fwd.Y, Fwd.Z);
        bCamLocLogged = true;
    }

    if (!bUseHeadForwardAsGazeFallback)
    {
        return;
    }

    if (!IsValid(SessionManager.Get()))
    {
        return;
    }

    const FVector GazeDirectionWorld = GetCurrentGazeDirectionWorld();

    if (!GazeDirectionWorld.IsNearlyZero())
    {
        SessionManager->UpdateGazeVector(GazeDirectionWorld);
    }
}

void ATranquilMindVRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Bind Quest right trigger click directly via legacy key binding.
    // SpaceBar → IA_TriggerPressed is handled by Blueprint + IMC_VRDefault (unchanged).
    PlayerInputComponent->BindKey(
        EKeys::OculusTouch_Right_Trigger_Click,
        IE_Pressed,
        this,
        &ATranquilMindVRPawn::HandleTriggerPressed);

    UE_LOG(LogTranquilMindVRPawn, Warning,
        TEXT("[TranquilMindHardwareTest] SetupPlayerInputComponent: bound OculusTouch_Right_Trigger_Click -> HandleTriggerPressed"));
}

void ATranquilMindVRPawn::HandleTriggerPressed()
{
    if (!IsValid(TargetSpawner.Get()))
    {
        AutoBindRuntimeReferences();
    }

    if (!IsValid(TargetSpawner.Get()))
    {
        UE_LOG(
            LogTranquilMindVRPawn,
            Warning,
            TEXT("[VRPawn] HandleTriggerPressed failed: TargetSpawner is null."));

        return;
    }

    const FVector GazeDirectionWorld = GetCurrentGazeDirectionWorld();

    TargetSpawner->HandleTriggerPulled(GazeDirectionWorld);

    UE_LOG(
        LogTranquilMindVRPawn,
        Verbose,
        TEXT("[VRPawn] Trigger pressed | Gaze=(%.3f, %.3f, %.3f)"),
        GazeDirectionWorld.X,
        GazeDirectionWorld.Y,
        GazeDirectionWorld.Z);
}

void ATranquilMindVRPawn::NotifyHMDRemoved()
{
    if (!IsValid(SessionManager.Get()))
    {
        AutoBindRuntimeReferences();
    }

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->NotifyHMDRemoved();

        UE_LOG(
            LogTranquilMindVRPawn,
            Warning,
            TEXT("[VRPawn] Forwarded NotifyHMDRemoved to SessionManager."));
    }
}

void ATranquilMindVRPawn::NotifyHMDResumed()
{
    if (!IsValid(SessionManager.Get()))
    {
        AutoBindRuntimeReferences();
    }

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->NotifyHMDResumed();

        UE_LOG(
            LogTranquilMindVRPawn,
            Log,
            TEXT("[VRPawn] Forwarded NotifyHMDResumed to SessionManager."));
    }
}

void ATranquilMindVRPawn::AutoBindRuntimeReferences()
{
    // ============================================================
    // Find SessionManager
    // ============================================================

    if (!IsValid(SessionManager.Get()))
    {
        AActor* FoundSessionActor = UGameplayStatics::GetActorOfClass(
            this,
            ATranquilMindSessionManager::StaticClass());

        SessionManager = Cast<ATranquilMindSessionManager>(FoundSessionActor);
    }

    // ============================================================
    // Find TargetSpawnerComponent
    // ============================================================

    if (!IsValid(TargetSpawner.Get()))
    {
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(
            this,
            AActor::StaticClass(),
            AllActors);

        for (AActor* Actor : AllActors)
        {
            if (!IsValid(Actor))
            {
                continue;
            }

            UTargetSpawnerComponent* FoundSpawner =
                Actor->FindComponentByClass<UTargetSpawnerComponent>();

            if (IsValid(FoundSpawner))
            {
                TargetSpawner = FoundSpawner;
                break;
            }
        }
    }

    // ============================================================
    // Register SessionManager with Spawner
    // ============================================================

    if (IsValid(TargetSpawner.Get()) && IsValid(SessionManager.Get()))
    {
        TargetSpawner->RegisterSessionManager(SessionManager.Get());
    }

    if (bLogAutoBinding)
    {
        UE_LOG(
            LogTranquilMindVRPawn,
            Log,
            TEXT("[VRPawn] AutoBindRuntimeReferences | SessionManager=%s | TargetSpawner=%s"),
            IsValid(SessionManager.Get()) ? *SessionManager->GetName() : TEXT("None"),
            IsValid(TargetSpawner.Get()) ? *TargetSpawner->GetName() : TEXT("None"));
    }
}

FVector ATranquilMindVRPawn::GetCurrentGazeDirectionWorld() const
{
    if (IsValid(VRCamera.Get()))
    {
        return VRCamera->GetForwardVector().GetSafeNormal();
    }

    return GetActorForwardVector().GetSafeNormal();
}
