// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TranquilMindVRPawn.generated.h"

class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class ATranquilMindSessionManager;
class UTargetSpawnerComponent;

UCLASS(BlueprintType, Blueprintable)
class TRANQUILMIND_API ATranquilMindVRPawn : public APawn
{
    GENERATED_BODY()

public:
    ATranquilMindVRPawn();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
    // ============================================================
    // COMPONENTS
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|VR")
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|VR")
    TObjectPtr<USceneComponent> VROrigin;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|VR")
    TObjectPtr<UCameraComponent> VRCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|VR")
    TObjectPtr<UMotionControllerComponent> LeftMotionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|VR")
    TObjectPtr<UMotionControllerComponent> RightMotionController;

public:
    // ============================================================
    // REFERENCES
    // ============================================================

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Runtime")
    TObjectPtr<ATranquilMindSessionManager> SessionManager = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Runtime")
    TObjectPtr<UTargetSpawnerComponent> TargetSpawner = nullptr;

public:
    // ============================================================
    // DEBUG / FALLBACK GAZE
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Gaze")
    bool bUseHeadForwardAsGazeFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug")
    bool bLogAutoBinding = true;

public:
    // ============================================================
    // BLUEPRINT CALLABLE API
    // ============================================================

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Input")
    void HandleTriggerPressed();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Hardware")
    void NotifyHMDRemoved();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Hardware")
    void NotifyHMDResumed();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Runtime")
    void AutoBindRuntimeReferences();

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Gaze")
    FVector GetCurrentGazeDirectionWorld() const;
};
