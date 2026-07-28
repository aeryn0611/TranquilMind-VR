// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../../TranquilMindTypes.h"
#include "TranquilMindTargetActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTMVisualMotionComponent;
class UTMDemoLifecycleComponent;
class UTargetSpawnerComponent;
class ATranquilMindSessionManager;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FTMDemoLifecycleTiming;

UCLASS(BlueprintType, Blueprintable)
class TRANQUILMIND_API ATranquilMindTargetActor : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindTargetActor();

protected:
    virtual void BeginPlay() override;

    /** Stage 1 instrumentation only: emits ENDPLAY, then calls Super. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;

// ============================================================
//  COMPONENTS
// ============================================================

public:
    /**
     * Stable gameplay transform (ROOT). Gameplay owns this exclusively:
     * spawn placement, travel, depth, and selection center all read/write the
     * anchor. It is NEVER animated by the presentation layer.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|Target")
    TObjectPtr<USceneComponent> GameplayAnchor;

    /**
     * Presentation-layer motion rig (Demo Visual Pipeline). Owns ONLY its
     * relative offset/rotation/scale (idle float, drift, spawn ease-in).
     * Inert by default; Research initialization forces it inert explicitly.
     * Gameplay must never read transforms at or below this component.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|Target")
    TObjectPtr<UTMVisualMotionComponent> VisualMotion;

    /** Rendered target visual. Child of VisualMotion; floats with it in Demo. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|Target")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    /**
     * STAGE 2. Demo-only nine-phase presentation lifecycle (Spec §4.1). Inert
     * until armed by UTargetSpawnerComponent's Demo path; Research forces it
     * inert explicitly in InitializeResearchVisual(). When it is not armed the
     * actor keeps its pre-Stage-2 behaviour exactly.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TranquilMind|Target")
    TObjectPtr<UTMDemoLifecycleComponent> DemoLifecycle;

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

    /** When true (Research Mode), Tick does nothing: no movement, no self-expiry. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|State")
    bool bStaticResearchStimulus = false;

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

    /**
     * Research Mode: initialize this actor as a STATIC VISUAL ONLY.
     * No movement, no self-expiry, no self-scoring. The UTMResearchRunner owns all
     * timing, visibility (via SetActorHiddenInGame), scoring, and destruction.
     */
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Target")
    void InitializeResearchVisual(ETMStimulusType InType, float InScale);

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

    /**
     * Stable gameplay selection center: the GameplayAnchor (actor root)
     * location. Intentionally NOT the mesh — the mesh is presentation-layer
     * and may carry a visual float offset in Demo Mode.
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    FVector GetTargetCenterWorld() const;

// ============================================================
//  STAGE 2 — LIFECYCLE AND PRESENTATION OWNERSHIP
// ============================================================

public:
    /**
     * Record the spawner that owns this presentation, so Phase-4 registration
     * and deregistration can be driven by the lifecycle rather than by a tick
     * sweep. Weak: the spawner may die first at world teardown.
     */
    void SetOwningSpawner(UTargetSpawnerComponent* InSpawner);

    /** Arm the nine-phase Demo lifecycle. Demo path only. */
    void ArmDemoPresentationLifecycle(
        float InPresentationSpawnTime_SEC,
        const FTMDemoLifecycleTiming& InTiming);

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    UTMDemoLifecycleComponent* GetDemoLifecycle() const { return DemoLifecycle; }

    /**
     * Spec §4.2 / §5: true only during Phase 4 and only until the first valid
     * input has been accepted. When the lifecycle is not armed this reports the
     * pre-Stage-2 condition (unresolved), so non-Demo paths are unchanged.
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    bool IsDemoInputOpen() const;

    // ---- Callbacks from UTMDemoLifecycleComponent. Not for general use. ----

    void HandleLifecycleEnterPhase4();
    void HandleLifecycleOutcomeLock();
    void HandleLifecycleResponseWindowExpired();

    /** @return true if a 350 ms exit envelope is now running (C1, retained). */
    bool HandleLifecycleEnterExit();

    void HandleLifecycleDestroy();

// ============================================================
//  INTERNAL STATE
// ============================================================

private:
    UPROPERTY(Transient)
    TObjectPtr<ATranquilMindSessionManager> SessionManager = nullptr;

    /** STAGE 2: owner of ActiveTargets. Weak — the spawner may die first. */
    TWeakObjectPtr<UTargetSpawnerComponent> OwningSpawner;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> DebugGoMaterial = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> DebugNoGoMaterial = nullptr;

    /** Environment Demo visual profile. Research continues using DebugGo/NoGoMaterial. */
    UPROPERTY()
    TObjectPtr<UMaterialInterface> DemoBubbleGoMaterial = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> DemoBubbleNoGoMaterial = nullptr;

    float LockedY_CM = 0.0f;

    float LockedZ_CM = 0.0f;

    /**
     * World point the approach axis is measured from — the Demo session frame
     * origin when available, otherwise this actor's spawn location. Used only
     * by GetDepthCM() to express depth in the same basis the travel uses.
     */
    FVector ApproachOriginWorld = FVector::ZeroVector;

    // ---- Post-resolve visual exit (Pass 1A.2, Demo Environment only) ----

    /** One-shot fade MID created at exit begin; parented to the live Go/NoGo
     *  instance so every role parameter is preserved. Dies with the actor. */
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ExitFadeMID = nullptr;

    float ExitBaseCenterOpacity = 0.0f;
    float ExitBaseEdgeOpacity = 0.0f;
    float ExitBaseBubbleBrightness = 0.0f;

    /** STAGE 2: BeginDemoVisualExit() is one-shot. See its comment. */
    bool bDemoVisualExitStarted = false;

// ============================================================
//  INTERNAL HELPERS
// ============================================================

private:
    void ResolveExpiredResponseWindow();

    /**
     * @param bCancellation  true only for a Void — a session cancellation (hard
     *   gate, phase change, restart, demo end, teardown). Scored outcomes pass
     *   false and keep the presentation running to ScheduledPhase4End (Spec §5).
     */
    void FinishResolution(bool bCancellation);

    void LockCurrentYZ();
    void EnforceZAxisTravelDirection();

    /**
     * Single seam for the Environment Demo visual profile (bubble materials +
     * visual motion). Wraps the existing map-name check so it lives in exactly
     * one place; the future visual-profile system replaces this helper.
     */
    bool UseEnvironmentDemoVisualProfile() const;

    /**
     * Begin the calm post-resolve exit: gameplay is already fully resolved
     * (outcome recorded, tick off, pruned from selection by bResolved); only
     * the presentation lingers for ExitDuration_SEC before Destroy().
     */
    void BeginDemoVisualExit();

    void HandleVisualExitFade(float SmoothAlpha);
    void HandleVisualExitComplete();
};
