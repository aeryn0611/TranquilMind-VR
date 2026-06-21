// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TranquilMindTypes.h"
#include "TranquilMindSessionManager.generated.h"

// ============================================================
//  MULTICAST DELEGATES
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnPhaseChanged,
    ETMSessionPhase, NewPhase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnInterruptStateChanged,
    ETMInterruptType, InterruptType,
    bool, bActive);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBlockEvaluated,
    FBlockStats, CompletedBlock);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnSessionTerminated,
    ETMAbortReason, Reason);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHardGateResumed);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnStaircaseParamsUpdated,
    float, New_ISI_MS,
    float, New_NoiseAlpha);

// ============================================================
//  CLASS DECLARATION
// ============================================================

UCLASS(BlueprintType, Blueprintable)
class TRANQUILMIND_API ATranquilMindSessionManager : public AActor
{
    GENERATED_BODY()

// ============================================================
//  SECTION A — CONSTRUCTION & UNREAL LIFECYCLE
// ============================================================

public:
    ATranquilMindSessionManager();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;

// ============================================================
//  SECTION B — MULTICAST DELEGATES
// ============================================================

public:
    UPROPERTY(BlueprintAssignable, Category = "TranquilMind|Events")
    FOnPhaseChanged OnPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "TranquilMind|Events")
    FOnInterruptStateChanged OnInterruptStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "TranquilMind|Events")
    FOnBlockEvaluated OnBlockEvaluated;

    UPROPERTY(BlueprintAssignable, Category = "TranquilMind|Events")
    FOnSessionTerminated OnSessionTerminated;

    UPROPERTY(BlueprintAssignable, Category = "TranquilMind|Events")
    FOnHardGateResumed OnHardGateResumed;

    UPROPERTY(BlueprintAssignable, Category = "TranquilMind|Events")
    FOnStaircaseParamsUpdated OnStaircaseParamsUpdated;

// ============================================================
//  SECTION D — EDITOR-TUNABLE CONFIGURATION
// ============================================================

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Config|HardGate",
        meta = (ClampMin = "1.0", ClampMax = "30.0", Units = "Degrees"))
    float HardGate_CPTZoneHalfAngle_DEG = 10.0f;

// ============================================================
//  SECTION E — PUBLIC API: INTERRUPT TRIGGERS
// ============================================================

public:
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Interrupts")
    void TriggerHardGate();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Interrupts")
    void ResumeFromHardGate();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Interrupts")
    void TriggerSysAbort(ETMAbortReason Reason);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Interrupts")
    void ReportSpamInput();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Interrupts")
    void NotifyHMDRemoved();

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Interrupts")
    void NotifyHMDResumed();

// ============================================================
//  SECTION F — PUBLIC API: TRIAL DATA INGESTION
// ============================================================

public:
    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Trial")
    void RecordTrialOutcome(ETMTrialOutcome Outcome, float RT_MS = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "TranquilMind|Gaze")
    void UpdateGazeVector(FVector InRawGazeDirection_World);

// ============================================================
//  SECTION G — PUBLIC API: READ-ONLY OBSERVERS
// ============================================================

public:
    UFUNCTION(BlueprintPure, Category = "TranquilMind|State")
    ETMSessionPhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|State")
    ETMInterruptType GetActiveInterrupt() const { return ActiveInterrupt; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Clocks")
    float GetMasterClock_SEC() const { return MasterClock_SEC; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Clocks")
    float GetBlockClock_SEC() const { return BlockClock_SEC; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Stats")
    FSessionStats GetSessionStats() const { return SessionStats; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Stats")
    FBlockStats GetCurrentBlockStats() const { return CurrentBlockStats; }

    const FSessionStats& GetSessionStatsRef() const { return SessionStats; }

    const FBlockStats& GetCurrentBlockStatsRef() const { return CurrentBlockStats; }

// ============================================================
//  SECTION H — PROTECTED: HFSM PHASE ENTRY POINTS
// ============================================================

protected:
    void EnterPhase_1A();

    void EnterPhase_1B();

    void EnterPhase_II();

    void EnterPhase_III();

    void CheckPhaseTransitions();

// ============================================================
//  SECTION I — PROTECTED: BLOCK EVALUATION & STAIRCASE
// ============================================================

protected:
    void EvaluateMicroBlock();

    void SealCurrentBlock();

    void ResetCurrentBlock();

    const FBlockStats* FindLastCleanBlock() const;

    void StepTrackA_ISI(const FBlockStats& CurrentBlock, const FBlockStats* ReferenceBlock);

    void StepTrackB_Noise(const FBlockStats& CurrentBlock);

    void BroadcastStaircaseParams();

// ============================================================
//  SECTION J — PROTECTED: GAZE PROCESSING & HARD GATE LOGIC
// ============================================================

protected:
    void TickGazeAndHardGate(float DeltaTime);

    FVector SmoothenGazeVector(const FVector& RawGaze_World);

    bool IsGazeOnCenter(const FVector& SmoothedGaze) const;

// ============================================================
//  SECTION K — PROTECTED: HARDWARE WATCHDOGS
// ============================================================

protected:
    void TickHMDRemovalWatchdog(float DeltaTime);

    void TickHardGateSuspendWatchdog(float DeltaTime);

// ============================================================
//  SECTION L — PROTECTED: SESSION PERSISTENCE
// ============================================================

protected:
    void BeginAsyncSessionFlush();

    void FinalizeSessionRecord(ETMAbortReason Reason);

// ============================================================
//  SECTION M — PRIVATE: MACRO-TIMELINE CLOCKS
// ============================================================

private:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Clocks",
        meta = (AllowPrivateAccess = "true"))
    float MasterClock_SEC = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Clocks",
        meta = (AllowPrivateAccess = "true"))
    float BlockClock_SEC = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Clocks",
        meta = (AllowPrivateAccess = "true"))
    bool bBlockClockFrozen = false;

// ============================================================
//  SECTION N — PRIVATE: HFSM STATE
// ============================================================

private:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|State",
        meta = (AllowPrivateAccess = "true"))
    ETMSessionPhase CurrentPhase = ETMSessionPhase::Phase_1A_Desensitization;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|State",
        meta = (AllowPrivateAccess = "true"))
    ETMInterruptType ActiveInterrupt = ETMInterruptType::None;

    bool bPhaseTransitionInProgress = false;

// ============================================================
//  SECTION O — PRIVATE: SESSION & BLOCK STATISTICS
// ============================================================

private:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Stats",
        meta = (AllowPrivateAccess = "true"))
    FSessionStats SessionStats;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TranquilMind|Stats",
        meta = (AllowPrivateAccess = "true"))
    FBlockStats CurrentBlockStats;

// ============================================================
//  SECTION P — PRIVATE: GAZE SMOOTHING STATE
// ============================================================

private:
    FVector SmoothedGazeDirection = FVector::ForwardVector;

    bool bHasValidGazeSample = false;

    float GazeOffCenter_AccumSEC = 0.0f;

    float GazeOnCenter_DwellSEC = 0.0f;

    bool bHardGateActive = false;

    float HardGate_SuspendTimer_SEC = 0.0f;

// ============================================================
//  SECTION Q — PRIVATE: HMD REMOVAL WATCHDOG STATE
// ============================================================

private:
    bool bHMDRemoved = false;

    float HMDRemoval_CountdownSEC = 0.0f;

// ============================================================
//  SECTION R — PRIVATE: SPAM / FRAME / GAZE CONFIG
// ============================================================

private:
    int32 ConsecutiveSpamBlocks = 0;

    float FrameDeltaSEC = 0.0f;

    FVector RawGazeDirection_World = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Config|HardGate",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "2.0", Units = "Seconds"))
    float HardGate_GazeSmoothWindow_SEC = 0.5f;
};
