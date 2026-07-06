// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TranquilMindVoidVisualLayer.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Lightweight ambient environment for the void level: a dim floor grid and a
// distant ring of accent markers. Pure visual layer — unlit materials, plain
// static mesh components (the same M_TheVoid rendering path the target actor
// already uses on Quest), no tick, no gameplay coupling. Keeps the central
// target corridor clear.
UCLASS()
class TRANQUILMIND_API ATranquilMindVoidVisualLayer : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindVoidVisualLayer();

    virtual void BeginPlay() override;

private:
    UStaticMeshComponent* AddCube(
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        UMaterialInstanceDynamic* Material);

    void BuildFloorGrid(UMaterialInstanceDynamic* Material);
    void BuildAccentRing(UMaterialInstanceDynamic* Material);

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> VoidBaseMaterial;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> VisualCubes;
};
