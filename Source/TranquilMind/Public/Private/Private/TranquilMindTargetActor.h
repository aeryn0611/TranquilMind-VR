// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../../TranquilMindTypes.h"
#include "TranquilMindTargetActor.generated.h"

class UStaticMeshComponent;
class ATranquilMindSessionManager;

UCLASS(BlueprintType, Blueprintable)
class TRANQUILMIND_API ATranquilMindTargetActor : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindTargetActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

// ============================================================
//  COMPONENTS
// ============================================================

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|Target")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

// ============================================================
//  TARGET STATE
// ============================================================

public:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Target")
    ETMStimulusType StimulusType = ETMStimulusType::Go;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Timing", meta = (Units = "Seconds"))
    float SpawnTimestamp_SEC = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|State")
    bool bResolved = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|State")
    bool bHadResponse = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Timing", meta = (ClampMin = "1.0", Units = "Milliseconds"))
    float ResponseWindow_MS = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Motion", meta = (ClampMin = "1.0", Units = "cm/s"))
    float MovementSpeed_CMPerSec = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Motion", meta = (Units = "cm"))
    float DestroyDepth_CM = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Motion")
    FVector TravelDirection_World = FVector(0.0f, 0.0f, -1.0f);

// ============================================================
//  PUBLIC API
// ============================================================

public:
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void InitializeTarget(
        ETMStimulusType InType,
        float InSpawnTimestamp_SEC,
        ATranquilMindSessionManager* InSessionManager);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void ResolveAsTriggered(float TriggerTimestamp_SEC);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void ResolveAsVoid();

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    bool IsResolved() const { return bResolved; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    bool HasHadResponse() const { return bHadResponse; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    float GetDepthCM() const;

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    float GetSpawnTimestampSEC() const { return SpawnTimestamp_SEC; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    ETMStimulusType GetStimulusType() const { return StimulusType; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    FVector GetTargetCenterWorld() const;

// ============================================================
//  INTERNAL STATE
// ============================================================

private:
    UPROPERTY(Transient)
    TObjectPtr<ATranquilMindSessionManager> SessionManager = nullptr;

    float LockedY_CM = 0.0f;

    float LockedZ_CM = 0.0f;

// ============================================================
//  INTERNAL HELPERS
// ============================================================

private:
    void ResolveExpiredResponseWindow();

    void FinishResolution();

    void LockCurrentYZ();
    void EnforceZAxisTravelDirection();
};
