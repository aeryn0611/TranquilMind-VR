// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TranquilMindVoidVisualLayer.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Calm therapeutic scene layer: pastel lavender sky dome, soft water-plane
// horizon, a symbolic tree silhouette, and floating orb accents. Pure visual
// layer — unlit M_TheVoid-family materials on plain static mesh components
// (the same rendering path the target actor already uses on Quest), no
// shadows, no collision, no gameplay coupling. One-shot tick aligns the scene
// to the user's eye height at startup, then ticking is disabled. Keeps the
// central target corridor clear.
UCLASS()
class TRANQUILMIND_API ATranquilMindVoidVisualLayer : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindVoidVisualLayer();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UStaticMeshComponent* AddMeshComp(
        UStaticMesh* Mesh,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        UMaterialInstanceDynamic* Material);

    UMaterialInstanceDynamic* MakeColorMID(const FLinearColor& Color);

    void BuildSkyAndWater();
    void BuildTree();
    void BuildOrbs();

    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> CylinderMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> PlaneMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> VoidBaseMaterial;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> SceneComps;

    bool bSceneBuilt = false;
};
