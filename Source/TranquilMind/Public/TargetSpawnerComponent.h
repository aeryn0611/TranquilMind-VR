// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "Templates/SubclassOf.h"
#include "../TranquilMindTypes.h"
#include "TargetSpawnerComponent.generated.h"

class ATranquilMindSessionManager;
class ATranquilMindTargetActor;

UCLASS(ClassGroup = (TranquilMind), meta = (BlueprintSpawnableComponent))
class TRANQUILMIND_API UTargetSpawnerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTargetSpawnerComponent();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

// ============================================================
//  CONFIG / REFERENCES
// ============================================================

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Target")
    TSubclassOf<ATranquilMindTargetActor> TargetActorClass;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Session")
    TObjectPtr<ATranquilMindSessionManager> SessionManager = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Target")
    TArray<TObjectPtr<ATranquilMindTargetActor>> ActiveTargets;

// ============================================================
//  LIVE PARAMETERS
// ============================================================

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Staircase",
        meta = (ClampMin = "800.0", ClampMax = "4000.0", Units = "Milliseconds"))
    float CurrentISI_MS = TranquilMind::MAX_ISI_MS;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Staircase")
    float CurrentNoiseAlpha = TranquilMind::MIN_NOISE_ALPHA;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Input", meta = (Units = "Seconds"))
    float LastTriggerTimestamp_SEC = -1.0f;

// ============================================================
//  DEBUG
// ============================================================

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug")
    bool bDebugHardwareMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug",
        meta = (ClampMin = "200.0", ClampMax = "8000.0", Units = "Milliseconds",
                EditCondition = "bDebugHardwareMode"))
    float DebugHardware_ISI_MS = 2000.0f;

// ============================================================
//  PUBLIC API
// ============================================================

public:
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Staircase")
    void ApplyStaircaseParams(float NewISI_MS, float NewNoiseAlpha);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void RestartTrial();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void DestroyAllActiveTargetsAsVoid();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Input")
    void HandleTriggerPulled(FVector InSmoothedGazeDirection_World);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Session")
    void RegisterSessionManager(ATranquilMindSessionManager* InSessionManager);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void SpawnNextTarget();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void CleanupResolvedTargets();

// ============================================================
//  INTERNAL STATE
// ============================================================

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Audit",
        meta = (AllowPrivateAccess = "true"))
    int32 StimulusRandomSeed = 5202;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Timing",
        meta = (AllowPrivateAccess = "true", Units = "Seconds"))
    float LastSpawnTimestamp_SEC = -1.0f;

    FRandomStream StimulusRandomStream;

    int32 DebugAlternateTypeCounter = 0;

// ============================================================
//  SESSION EVENT HANDLERS
// ============================================================

private:
    UFUNCTION()
    void HandleInterruptStateChanged(ETMInterruptType InterruptType, bool bActive);

    UFUNCTION()
    void HandlePhaseChanged(ETMSessionPhase NewPhase);

    UFUNCTION()
    void HandleHardGateResumed();

// ============================================================
//  INTERNAL HELPERS
// ============================================================

private:
    bool CanSpawnTargetsNow() const;

    ETMStimulusType ChooseNextStimulusType();

    ATranquilMindTargetActor* SelectTargetByTieBreak(const FVector& InSmoothedGazeDirection_World) const;

    float ComputeGazeAngleToTargetDEG(
        const ATranquilMindTargetActor* Target,
        const FVector& InSmoothedGazeDirection_World) const;
};
