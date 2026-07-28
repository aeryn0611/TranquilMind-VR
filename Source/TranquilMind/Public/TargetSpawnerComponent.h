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
class UTMResearchRunner;

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

    /**
     * SCORING-ELIGIBILITY SET — Spec §4.9. Contains an actor ONLY while it is in
     * Phase 4 and can still accept input. It is NOT a presentation set: see
     * LiveDemoPresentation below.
     *
     * Registered at Phase 4 entry, deregistered at the outcome lock, both driven
     * by UTMDemoLifecycleComponent through the target actor. Max size 1.
     */
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug")
    bool bDemoMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug",
        meta = (ClampMin = "1000.0", ClampMax = "10000.0", Units = "Milliseconds",
                EditCondition = "bDemoMode"))
    float DemoMode_ISI_MS = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug",
        meta = (ClampMin = "500.0", ClampMax = "8000.0", Units = "Milliseconds",
                EditCondition = "bDemoMode"))
    float DemoMode_ResponseWindow_MS = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Debug",
        meta = (ClampMin = "30.0", ClampMax = "600.0", Units = "Seconds",
                EditCondition = "bDemoMode"))
    float DemoMode_Duration_SEC = 120.0f;

// ============================================================
//  DEMO PRESENTATION (Pass 1A) — comfort distance and apparent size.
//  Demo-only: SpawnNextTarget() is Research-guarded, so neither value can
//  reach the Research stimulus (which is placed by UTMResearchRunner from
//  FTMResearchConfig). Geometry: with the engine sphere (50 cm unscaled
//  radius), scale 0.45, 50 cm/s approach and a 2500 ms window, the bubble
//  ends at 225 cm centre / ~202.5 cm nearest-surface distance.
// ============================================================

public:
    /** Spawn-centre distance from the CURRENT camera along the immutable
     *  Demo session forward. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Demo Presentation",
        meta = (ClampMin = "150.0", ClampMax = "800.0", Units = "cm",
                EditCondition = "bDemoMode"))
    float DemoMode_SpawnDistance_CM = 350.0f;

    /** Actor scale applied to the Demo target after initialization. Purely
     *  presentational: selection, depth, collision and resolution never read
     *  mesh bounds (audited — anchor-point math only). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Demo Presentation",
        meta = (ClampMin = "0.10", ClampMax = "1.00",
                EditCondition = "bDemoMode"))
    float DemoMode_TargetScale = 0.45f;

    /**
     * Demo approach speed along the immutable session forward (Pass 1A.1).
     * Replaces the TEMP actor-side 50 cm/s overwrite for Demo runs; pushed
     * onto the spawned actor after InitializeTarget. Research is independent:
     * InitializeResearchVisual zeroes MovementSpeed and the runner owns all
     * Research stimulus placement/timing. With 350 cm spawn, 2500 ms window
     * and scale 0.45: travel 50 cm, end centre 300 cm, nearest surface
     * ~277.5 cm, angular diameter ~7.4 deg -> ~8.6 deg.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TranquilMind|Demo Presentation",
        meta = (ClampMin = "0.0", ClampMax = "200.0", Units = "cm/s",
                EditCondition = "bDemoMode"))
    float DemoMode_ApproachSpeed_CMPerSec = 20.0f;

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

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Debug")
    bool IsDemoEnded() const { return bDemoEnded; }

// ============================================================
//  STAGE 2 — OFFICIAL DEMO PRESENTATION OWNERSHIP (Spec §4.9)
// ============================================================

public:
    /**
     * Is an official Demo presentation still visually alive?
     *
     * This is the spawn gate. Stage 1 §3.3 proved the previous gate —
     * `ActiveTargets.Num() > 0` — is the root of C7: deregistration happens at
     * resolution while the actor keeps rendering for the whole 350 ms exit, so a
     * gate reading REGISTRATION rather than VISIBILITY admitted a successor into
     * a frame where the predecessor was still at 57.7% edge opacity.
     *
     * A weak reference makes release self-clearing: Destroy() marks the actor
     * garbage, so an actor that died without running its normal path counts as
     * released and this gate can never deadlock the Demo (Spec §4.9.1).
     */
    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    bool HasLiveDemoPresentation() const;

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Target")
    ATranquilMindTargetActor* GetLiveDemoPresentation() const { return LiveDemoPresentation.Get(); }

    /** Phase 4 entry. Called by the target's lifecycle component. Max size 1. */
    void RegisterPhase4Target(ATranquilMindTargetActor* Target);

    /** Outcome lock. Called by the target's lifecycle component. */
    void DeregisterPhase4Target(ATranquilMindTargetActor* Target);

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

    // ---- Operating mode (single authority; see UTMResearchSettings) ----

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Mode",
        meta = (AllowPrivateAccess = "true"))
    ETMOperatingMode OperatingMode = ETMOperatingMode::Demo;

    /** Research-mode trial state machine. Created + ticked only when OperatingMode == Research. */
    UPROPERTY(Transient)
    TObjectPtr<UTMResearchRunner> ResearchRunner = nullptr;

    float DemoElapsed_SEC = 0.0f;
    bool  bDemoEnded = false;

    /**
     * PRESENTATION SET — Spec §4.9. The single official active Demo bubble.
     * Claimed at presentation creation (Phase 1) and released only when the
     * actor actually dies at the end of Phase 9. Weak by design: see
     * HasLiveDemoPresentation().
     */
    TWeakObjectPtr<ATranquilMindTargetActor> LiveDemoPresentation;

    /**
     * Latches after the first Research-mode spawn suppression so the expected,
     * benign repeats (every gaze hard-gate resume reaches those entry points)
     * do not flood a researcher's log at Warning level.
     */
    bool bLoggedResearchSuppression = false;

    // ---- Pending-spawn exactly-once handshake (Demo only) ----
    //
    // When SpawnNextTarget() finds the Demo session frame still waiting on a
    // first usable XR pose, it records ONE coalesced pending request here and
    // returns without constructing a target (and without consuming a stimulus
    // draw). The subsystem's OnDemoFrameCaptured broadcast — fired once per
    // latch, whether from a valid pose or the bounded-wait timeout — is
    // consumed by HandleDemoFrameCaptured(), which clears the flag BEFORE
    // spawning. The spawner owns the request; the subsystem never spawns.

    /** One coalesced request; repeated attempts while waiting do not stack. */
    bool bDemoSpawnPendingFrame = false;

    /**
     * True while SpawnNextTarget() is on the stack. If the frame latches
     * INSIDE that call (EnsureCaptured succeeding synchronously), the capture
     * broadcast must not recurse into a second spawn — the in-flight call
     * completes the request itself.
     */
    bool bSpawnCallInProgress = false;

    /** True after the coalescing log fired once for the current wait. */
    bool bLoggedPendingSpawnWait = false;

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
    /** Consume a pending Demo spawn request exactly once per frame capture. */
    void HandleDemoFrameCaptured();

    /** Warning on first Research-mode spawn suppression, Verbose thereafter. */
    void LogResearchSuppressionOnce(const TCHAR* Context);

    bool CanSpawnTargetsNow() const;

    ETMStimulusType ChooseNextStimulusType();

    ATranquilMindTargetActor* SelectTargetByTieBreak(const FVector& InSmoothedGazeDirection_World) const;

    float ComputeGazeAngleToTargetDEG(
        const ATranquilMindTargetActor* Target,
        const FVector& InSmoothedGazeDirection_World) const;
};
