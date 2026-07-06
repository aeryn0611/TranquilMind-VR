// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindVoidVisualLayer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindVoidVisual, Log, All);

namespace TranquilMindVoidVisual
{
    // Floor grid: dim violet, well below the target corridor sightline.
    constexpr float GridHalfSpan_CM   = 1000.0f;
    constexpr float GridSpacing_CM    = 250.0f;
    constexpr float GridLineWidth_CM  = 2.5f;
    constexpr float GridLineHeight_CM = 0.8f;
    constexpr float GridZ_CM          = 0.0f;

    // Accent ring: calm cyan markers far outside the play space.
    constexpr int32 RingCount      = 28;
    constexpr float RingRadius_CM  = 1500.0f;
    constexpr float RingBaseZ_CM   = 60.0f;
}

ATranquilMindVoidVisualLayer::ATranquilMindVoidVisualLayer()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    SetRootComponent(Root);

    // Same engine mesh + project material pattern already Quest-verified by
    // ATranquilMindTargetActor (static mesh component + M_TheVoid family).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        CubeMesh = CubeFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> VoidMatFinder(
        TEXT("/Game/M_TheVoid.M_TheVoid"));
    if (VoidMatFinder.Succeeded())
    {
        VoidBaseMaterial = VoidMatFinder.Object;
    }
}

void ATranquilMindVoidVisualLayer::BeginPlay()
{
    Super::BeginPlay();

    if (CubeMesh == nullptr || VoidBaseMaterial == nullptr)
    {
        UE_LOG(LogTranquilMindVoidVisual, Warning,
            TEXT("[VoidVisual] Missing mesh or material — environment layer skipped"));
        return;
    }

    // Dim violet for the grid — grounding, not attention-grabbing.
    UMaterialInstanceDynamic* GridMID =
        UMaterialInstanceDynamic::Create(VoidBaseMaterial, this);
    GridMID->SetVectorParameterValue(
        TEXT("color"), FLinearColor(0.035f, 0.03f, 0.10f));

    // Calm cyan for the distant accents.
    UMaterialInstanceDynamic* RingMID =
        UMaterialInstanceDynamic::Create(VoidBaseMaterial, this);
    RingMID->SetVectorParameterValue(
        TEXT("color"), FLinearColor(0.02f, 0.16f, 0.22f));

    BuildFloorGrid(GridMID);
    BuildAccentRing(RingMID);

    UE_LOG(LogTranquilMindVoidVisual, Warning,
        TEXT("[VoidVisual] Environment layer built | cubes=%d"),
        VisualCubes.Num());
}

UStaticMeshComponent* ATranquilMindVoidVisualLayer::AddCube(
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    UMaterialInstanceDynamic* Material)
{
    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
    Comp->SetupAttachment(GetRootComponent());
    Comp->SetStaticMesh(CubeMesh);
    Comp->SetMaterial(0, Material);
    Comp->SetMobility(EComponentMobility::Movable);
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comp->SetGenerateOverlapEvents(false);
    Comp->SetCanEverAffectNavigation(false);
    Comp->SetCastShadow(false);
    Comp->SetRelativeTransform(FTransform(Rotation, Location, Scale));
    Comp->RegisterComponent();

    VisualCubes.Add(Comp);
    return Comp;
}

void ATranquilMindVoidVisualLayer::BuildFloorGrid(UMaterialInstanceDynamic* Material)
{
    using namespace TranquilMindVoidVisual;

    // Engine cube is 100 cm; scale spans/widths from that base.
    const float LengthScale = (GridHalfSpan_CM * 2.0f) / 100.0f;
    const float WidthScale  = GridLineWidth_CM / 100.0f;
    const float HeightScale = GridLineHeight_CM / 100.0f;

    for (float Offset = -GridHalfSpan_CM; Offset <= GridHalfSpan_CM; Offset += GridSpacing_CM)
    {
        // Lines running along X.
        AddCube(
            FVector(0.0f, Offset, GridZ_CM),
            FRotator::ZeroRotator,
            FVector(LengthScale, WidthScale, HeightScale),
            Material);

        // Lines running along Y.
        AddCube(
            FVector(Offset, 0.0f, GridZ_CM),
            FRotator(0.0f, 90.0f, 0.0f),
            FVector(LengthScale, WidthScale, HeightScale),
            Material);
    }
}

void ATranquilMindVoidVisualLayer::BuildAccentRing(UMaterialInstanceDynamic* Material)
{
    using namespace TranquilMindVoidVisual;

    FRandomStream Rand(5202);

    for (int32 i = 0; i < RingCount; ++i)
    {
        const float Angle_RAD = (2.0f * PI * i) / RingCount;
        const float Radius    = RingRadius_CM + Rand.FRandRange(-150.0f, 350.0f);
        const float Height    = RingBaseZ_CM + Rand.FRandRange(0.0f, 420.0f);
        const float Scale     = Rand.FRandRange(0.05f, 0.14f);

        AddCube(
            FVector(Radius * FMath::Cos(Angle_RAD), Radius * FMath::Sin(Angle_RAD), Height),
            FRotator(0.0f, Rand.FRandRange(0.0f, 90.0f), 0.0f),
            FVector(Scale),
            Material);
    }
}
