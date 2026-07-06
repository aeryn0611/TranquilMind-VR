// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindVoidVisualLayer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindVoidVisual, Log, All);

namespace TranquilMindScene
{
    // Scene root sits at the user's floor: camera height minus standing eye height.
    constexpr float EyeHeight_CM = 170.0f;

    // Pastel palette (unlit "color" parameter on M_TheVoid).
    const FLinearColor SkyColor      (0.60f, 0.44f, 0.68f);   // lavender-pink
    const FLinearColor WaterColor    (0.38f, 0.27f, 0.55f);   // deeper violet
    const FLinearColor ShimmerColor  (0.72f, 0.52f, 0.72f);   // pink highlight
    const FLinearColor TrunkColor    (0.10f, 0.06f, 0.22f);   // silhouette violet
    const FLinearColor CanopyColor   (0.22f, 0.14f, 0.40f);   // soft violet
    const FLinearColor CanopyAccent  (0.16f, 0.36f, 0.46f);   // cyan tint
    const FLinearColor OrbCyan       (0.35f, 0.72f, 0.82f);
    const FLinearColor OrbPink       (0.82f, 0.58f, 0.78f);
    const FLinearColor OrbViolet     (0.58f, 0.48f, 0.84f);

    constexpr float SkyDomeRadius_CM = 2500.0f;
    constexpr int32 OrbCount         = 16;
    constexpr int32 ShimmerCount     = 6;
}

ATranquilMindVoidVisualLayer::ATranquilMindVoidVisualLayer()
{
    // One-shot tick: waits for a valid camera, builds/aligns the scene, disables itself.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    SetRootComponent(Root);

    // Same engine mesh + project material pattern already Quest-verified by
    // ATranquilMindTargetActor (static mesh component + M_TheVoid family).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereFinder.Succeeded()) { SphereMesh = SphereFinder.Object; }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded()) { CubeMesh = CubeFinder.Object; }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderFinder.Succeeded()) { CylinderMesh = CylinderFinder.Object; }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
        TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneFinder.Succeeded()) { PlaneMesh = PlaneFinder.Object; }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> VoidMatFinder(
        TEXT("/Game/M_TheVoid.M_TheVoid"));
    if (VoidMatFinder.Succeeded()) { VoidBaseMaterial = VoidMatFinder.Object; }
}

void ATranquilMindVoidVisualLayer::BeginPlay()
{
    Super::BeginPlay();

    if (SphereMesh == nullptr || CubeMesh == nullptr ||
        CylinderMesh == nullptr || PlaneMesh == nullptr ||
        VoidBaseMaterial == nullptr)
    {
        UE_LOG(LogTranquilMindVoidVisual, Warning,
            TEXT("[VoidVisual] Missing mesh or material — scene layer skipped"));
        SetActorTickEnabled(false);
    }
}

void ATranquilMindVoidVisualLayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bSceneBuilt)
    {
        SetActorTickEnabled(false);
        return;
    }

    APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (CamManager == nullptr)
    {
        return;
    }

    // Anchor the scene to the user's actual position and eye height so the
    // water horizon reads correctly regardless of tracking origin.
    const FVector CamLoc = CamManager->GetCameraLocation();
    const FRotator CamRot = CamManager->GetCameraRotation();
    const FRotator YawOnly(0.0f, CamRot.Yaw, 0.0f);

    SetActorLocationAndRotation(
        FVector(CamLoc.X, CamLoc.Y, CamLoc.Z - TranquilMindScene::EyeHeight_CM),
        YawOnly);

    BuildSkyAndWater();
    BuildTree();
    BuildOrbs();

    bSceneBuilt = true;
    SetActorTickEnabled(false);

    UE_LOG(LogTranquilMindVoidVisual, Warning,
        TEXT("[VoidVisual] Therapeutic scene built | comps=%d"),
        SceneComps.Num());
}

UMaterialInstanceDynamic* ATranquilMindVoidVisualLayer::MakeColorMID(const FLinearColor& Color)
{
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(VoidBaseMaterial, this);
    MID->SetVectorParameterValue(TEXT("color"), Color);
    return MID;
}

UStaticMeshComponent* ATranquilMindVoidVisualLayer::AddMeshComp(
    UStaticMesh* Mesh,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    UMaterialInstanceDynamic* Material)
{
    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
    Comp->SetupAttachment(GetRootComponent());
    Comp->SetStaticMesh(Mesh);
    Comp->SetMaterial(0, Material);
    Comp->SetMobility(EComponentMobility::Movable);
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comp->SetGenerateOverlapEvents(false);
    Comp->SetCanEverAffectNavigation(false);
    Comp->SetCastShadow(false);
    Comp->SetRelativeTransform(FTransform(Rotation, Location, Scale));
    Comp->RegisterComponent();

    SceneComps.Add(Comp);
    return Comp;
}

void ATranquilMindVoidVisualLayer::BuildSkyAndWater()
{
    using namespace TranquilMindScene;

    // Sky dome: engine sphere is 100 cm across; negative X scale flips the
    // winding so the surface renders from inside.
    const float DomeScale = (SkyDomeRadius_CM * 2.0f) / 100.0f;
    AddMeshComp(
        SphereMesh,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        FVector(-DomeScale, DomeScale, DomeScale),
        MakeColorMID(SkyColor));

    // Water plane at the scene floor; meets the dome in a clean horizon circle.
    AddMeshComp(
        PlaneMesh,
        FVector(0.0f, 0.0f, -2.0f),
        FRotator::ZeroRotator,
        FVector(60.0f, 60.0f, 1.0f),
        MakeColorMID(WaterColor));

    // A few thin shimmer strips as a calm wave impression — opaque, unlit.
    UMaterialInstanceDynamic* ShimmerMID = MakeColorMID(ShimmerColor);
    FRandomStream Rand(1123);
    for (int32 i = 0; i < ShimmerCount; ++i)
    {
        const float Angle_DEG = Rand.FRandRange(0.0f, 360.0f);
        const float Dist      = Rand.FRandRange(900.0f, 2100.0f);
        const float Angle_RAD = FMath::DegreesToRadians(Angle_DEG);

        AddMeshComp(
            CubeMesh,
            FVector(Dist * FMath::Cos(Angle_RAD), Dist * FMath::Sin(Angle_RAD), 1.0f),
            FRotator(0.0f, Rand.FRandRange(0.0f, 180.0f), 0.0f),
            FVector(Rand.FRandRange(3.0f, 7.0f), 0.05f, 0.015f),
            ShimmerMID);
    }
}

void ATranquilMindVoidVisualLayer::BuildTree()
{
    using namespace TranquilMindScene;

    // Symbolic tree 9 m ahead, 3.5 m left of the user's initial facing —
    // far outside the 2 m central target corridor.
    const FVector TreeBase(900.0f, -350.0f, 0.0f);

    UMaterialInstanceDynamic* TrunkMID  = MakeColorMID(TrunkColor);
    UMaterialInstanceDynamic* CanopyMID = MakeColorMID(CanopyColor);

    // Trunk: engine cylinder is 100 cm tall, pivot centered.
    AddMeshComp(
        CylinderMesh,
        TreeBase + FVector(0.0f, 0.0f, 180.0f),
        FRotator::ZeroRotator,
        FVector(0.22f, 0.22f, 3.6f),
        TrunkMID);

    // Two angled branches near the top.
    AddMeshComp(
        CylinderMesh,
        TreeBase + FVector(0.0f, 35.0f, 330.0f),
        FRotator(0.0f, 0.0f, 32.0f),
        FVector(0.10f, 0.10f, 1.3f),
        TrunkMID);

    AddMeshComp(
        CylinderMesh,
        TreeBase + FVector(0.0f, -30.0f, 300.0f),
        FRotator(0.0f, 0.0f, -26.0f),
        FVector(0.09f, 0.09f, 1.1f),
        TrunkMID);

    // Canopy cluster: three soft-violet spheres plus one cyan accent.
    AddMeshComp(SphereMesh, TreeBase + FVector(  0.0f,  20.0f, 430.0f),
        FRotator::ZeroRotator, FVector(1.5f), CanopyMID);
    AddMeshComp(SphereMesh, TreeBase + FVector( 15.0f,  95.0f, 400.0f),
        FRotator::ZeroRotator, FVector(1.1f), CanopyMID);
    AddMeshComp(SphereMesh, TreeBase + FVector(-10.0f, -70.0f, 385.0f),
        FRotator::ZeroRotator, FVector(1.0f), CanopyMID);
    AddMeshComp(SphereMesh, TreeBase + FVector( 30.0f,  30.0f, 470.0f),
        FRotator::ZeroRotator, FVector(0.7f), MakeColorMID(CanopyAccent));
}

void ATranquilMindVoidVisualLayer::BuildOrbs()
{
    using namespace TranquilMindScene;

    UMaterialInstanceDynamic* Mids[3] =
    {
        MakeColorMID(OrbCyan),
        MakeColorMID(OrbPink),
        MakeColorMID(OrbViolet)
    };

    FRandomStream Rand(5202);

    for (int32 i = 0; i < OrbCount; ++i)
    {
        // Keep the frontal +/-30 degree corridor clear for GO/NOGO targets.
        const float Angle_DEG = Rand.FRandRange(30.0f, 330.0f);
        const float Dist      = Rand.FRandRange(350.0f, 1000.0f);
        const float Height    = Rand.FRandRange(40.0f, 350.0f);
        const float Scale     = Rand.FRandRange(0.10f, 0.28f);
        const float Angle_RAD = FMath::DegreesToRadians(Angle_DEG);

        AddMeshComp(
            SphereMesh,
            FVector(Dist * FMath::Cos(Angle_RAD), Dist * FMath::Sin(Angle_RAD), Height),
            FRotator::ZeroRotator,
            FVector(Scale),
            Mids[i % 3]);
    }
}
