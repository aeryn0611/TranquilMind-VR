// Fill out your copyright notice in the Description page of Project Settings.

#include "Private/Private/TranquilMindTargetActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "../../../TranquilMindSessionManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindTargetActor, Log, All);

namespace TranquilMindTarget
{
    constexpr float DebugResponseWindow_MS = 10000.0f;
    constexpr float DebugMovementSpeed_CMPerSec = 50.0f;
    constexpr float DebugDestroyDepth_CM = -10000.0f;
    constexpr float DebugTargetScale = 0.25f;
}

ATranquilMindTargetActor::ATranquilMindTargetActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    SetRootComponent(MeshComponent);

    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GoMatFinder(
        TEXT("/Game/MI_Target_Go.MI_Target_Go"));
    if (GoMatFinder.Succeeded())
    {
        MeshComponent->SetMaterial(0, GoMatFinder.Object);
    }

    StimulusType = ETMStimulusType::Go;
    SpawnTimestamp_SEC = 0.0f;
    bResolved = false;
    bHadResponse = false;

    ResponseWindow_MS = TranquilMindTarget::DebugResponseWindow_MS;
    MovementSpeed_CMPerSec = TranquilMindTarget::DebugMovementSpeed_CMPerSec;
    DestroyDepth_CM = TranquilMindTarget::DebugDestroyDepth_CM;

    TravelDirection_World = FVector(-1.0f, 0.0f, 0.0f);

    SetActorScale3D(FVector(
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale));
}

void ATranquilMindTargetActor::BeginPlay()
{
    Super::BeginPlay();

    ResponseWindow_MS = TranquilMindTarget::DebugResponseWindow_MS;
    MovementSpeed_CMPerSec = TranquilMindTarget::DebugMovementSpeed_CMPerSec;
    DestroyDepth_CM = TranquilMindTarget::DebugDestroyDepth_CM;

    SetActorScale3D(FVector(
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale));

    EnforceZAxisTravelDirection();
    LockCurrentYZ();
}

void ATranquilMindTargetActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bResolved)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    // TEMP DEBUG MODE:
    // 暂时关闭自动 ResponseWindow 到期判定。
    // 现在目标不会因为时间窗过期自动消失，只会被 Space / Trigger 命中后销毁。
    //
    // 正式版恢复：
    // const float Now_SEC = World->GetTimeSeconds();
    // const float Elapsed_MS = (Now_SEC - SpawnTimestamp_SEC) * 1000.0f;
    // if (Elapsed_MS >= ResponseWindow_MS)
    // {
    //     ResolveExpiredResponseWindow();
    //     return;
    // }

    const FVector CurrentLocation = GetActorLocation();

    const float Step_CM = FMath::Abs(MovementSpeed_CMPerSec) * DeltaTime;
    const float NewX_CM = CurrentLocation.X - Step_CM;

    const FVector NewLocation(NewX_CM, LockedY_CM, LockedZ_CM);
    SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

    // TEMP DEBUG MODE:
    // 暂时关闭到达 DestroyDepth 后自动 Void。
    //
    // 正式版恢复：
    // if (NewX_CM <= DestroyDepth_CM + KINDA_SMALL_NUMBER)
    // {
    //     ResolveAsVoid();
    // }
}

void ATranquilMindTargetActor::InitializeTarget(
    ETMStimulusType InType,
    float InSpawnTimestamp_SEC,
    ATranquilMindSessionManager* InSessionManager)
{
    StimulusType = InType;
    SpawnTimestamp_SEC = InSpawnTimestamp_SEC;
    SessionManager = InSessionManager;

    bResolved = false;
    bHadResponse = false;

    // 强制覆盖运行时参数，避免 BP_Target 旧默认值干扰。
    ResponseWindow_MS = TranquilMindTarget::DebugResponseWindow_MS;
    MovementSpeed_CMPerSec = TranquilMindTarget::DebugMovementSpeed_CMPerSec;
    DestroyDepth_CM = TranquilMindTarget::DebugDestroyDepth_CM;

    // TEMP hardware visibility test — restore to DebugTargetScale (0.25) once ball is confirmed visible
    SetActorScale3D(FVector(1.0f));

    EnforceZAxisTravelDirection();
    LockCurrentYZ();

    SetActorTickEnabled(true);
    SetLifeSpan(0.0f);

    // MI_Target_Go / M_TheVoid drives Emissive Color from a Material Parameter Collection;
    // SetVectorParameterValue("EmissiveColor") has no effect on it.
    // Use BasicShapeMaterial which has a direct "Color" VectorParameter → BaseColor.
    UMaterial* BasicMat = LoadObject<UMaterial>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (IsValid(BasicMat))
    {
        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BasicMat, this);
        if (IsValid(DynMat))
        {
            const FLinearColor TargetColor =
                (StimulusType == ETMStimulusType::Go)
                    ? FLinearColor(0.0f, 1.0f, 0.0f)   // GO: green
                    : FLinearColor(1.0f, 0.0f, 0.0f);  // NOGO: red
            DynMat->SetVectorParameterValue(TEXT("Color"), TargetColor);
            MeshComponent->SetMaterial(0, DynMat);
        }
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] Initialized | Type=%d | SpawnTime=%.3f | Speed=%.1f | ResponseWindow=%.1f | Scale=1.00 (TEMP)"),
        static_cast<int32>(StimulusType),
        SpawnTimestamp_SEC,
        MovementSpeed_CMPerSec,
        ResponseWindow_MS);
}

void ATranquilMindTargetActor::ResolveAsTriggered(float TriggerTimestamp_SEC)
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;
    bHadResponse = true;

    if (StimulusType == ETMStimulusType::Go)
    {
        const float RT_MS = FMath::Max(
            0.0f,
            (TriggerTimestamp_SEC - SpawnTimestamp_SEC) * 1000.0f);

        if (IsValid(SessionManager.Get()))
        {
            SessionManager->RecordTrialOutcome(ETMTrialOutcome::Hit, RT_MS);
        }

        UE_LOG(
            LogTranquilMindTargetActor,
            Warning,
            TEXT("[TargetActor] Triggered GO -> Hit | RT=%.1f ms"),
            RT_MS);
    }
    else
    {
        if (IsValid(SessionManager.Get()))
        {
            SessionManager->RecordTrialOutcome(ETMTrialOutcome::Commission, 0.0f);
        }

        UE_LOG(
            LogTranquilMindTargetActor,
            Warning,
            TEXT("[TargetActor] Triggered NOGO -> Commission"));
    }

    FinishResolution();
}

void ATranquilMindTargetActor::ResolveAsVoid()
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->RecordTrialOutcome(ETMTrialOutcome::Void, 0.0f);
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] Resolved as Void"));

    FinishResolution();
}

float ATranquilMindTargetActor::GetDepthCM() const
{
    // 当前调试版本中，“深度”映射到 UE X 轴。
    // X 越小，目标越接近玩家/相机。
    return GetActorLocation().X;
}

FVector ATranquilMindTargetActor::GetTargetCenterWorld() const
{
    if (MeshComponent != nullptr)
    {
        return MeshComponent->GetComponentLocation();
    }

    return GetActorLocation();
}

void ATranquilMindTargetActor::ResolveExpiredResponseWindow()
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;

    const ETMTrialOutcome Outcome =
        StimulusType == ETMStimulusType::Go
            ? ETMTrialOutcome::Omission
            : ETMTrialOutcome::CorrectRejection;

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->RecordTrialOutcome(Outcome, 0.0f);
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] Response window expired | Outcome=%d"),
        static_cast<int32>(Outcome));

    FinishResolution();
}

void ATranquilMindTargetActor::FinishResolution()
{
    SetActorTickEnabled(false);

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] FinishResolution -> Destroy immediately"));

    Destroy();
}

void ATranquilMindTargetActor::LockCurrentYZ()
{
    const FVector CurrentLocation = GetActorLocation();

    LockedY_CM = CurrentLocation.Y;
    LockedZ_CM = CurrentLocation.Z;
}

void ATranquilMindTargetActor::EnforceZAxisTravelDirection()
{
    // SRS 里的 Z-depth 在 UE 当前调试坐标中映射到 X 轴前后深度。
    TravelDirection_World = FVector(-1.0f, 0.0f, 0.0f);
}
