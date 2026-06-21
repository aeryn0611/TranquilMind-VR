// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TranquilMindTypes.generated.h"

// ============================================================
//  SECTION 1 — ENUMERATIONS
// ============================================================

/**
 * Macro-timeline phase of a single 1200s session.
 * T_master holds highest authority; transitions are one-way (no backtrack).
 */
UENUM(BlueprintType)
enum class ETMSessionPhase : uint8
{
    Phase_1A_Desensitization UMETA(DisplayName = "1A - Desensitization  (00:00-01:00)"),
    Phase_1B_Baseline UMETA(DisplayName = "1B - Baseline         (01:00-02:00)"),
    Phase_II_CoreTraining UMETA(DisplayName = "II  - Core Training    (02:00-18:00)"),
    Phase_III_Cooldown UMETA(DisplayName = "III - Cooldown         (18:00-20:00)"),
    Phase_Terminated UMETA(DisplayName = "Terminated")
};

/**
 * Preemption interrupt hierarchy (ascending priority, higher value = higher authority).
 * State machine MUST respect this ordering; a lower-priority interrupt
 * cannot override a currently active higher-priority one.
 */
UENUM(BlueprintType)
enum class ETMInterruptType : uint8
{
    None UMETA(DisplayName = "None"),
    SpamArtifact_BlockQuarantine UMETA(DisplayName = "Spam - Block Quarantine"),
    HardGate_GazeLost UMETA(DisplayName = "Hard Gate - Gaze Lost"),
    SysAbort UMETA(DisplayName = "SYS_ABORT")
};

/** Reason recorded in the termination log when SYS_ABORT fires. */
UENUM(BlueprintType)
enum class ETMAbortReason : uint8
{
    None UMETA(DisplayName = "None"),
    HardwareLost UMETA(DisplayName = "Hardware Lost / HMD Disconnected"),
    HMDRemovedTimeout UMETA(DisplayName = "HMD Removed > 10 s"),
    HardGateSuspendTimeout UMETA(DisplayName = "Hard Gate Suspend > 30 s"),
    ConsecutiveSpamBlocks UMETA(DisplayName = "3 Consecutive Spam Blocks")
};

/**
 * Per-trial outcome used for both the double-entry ledger and staircase evaluation.
 * Void covers all non-scored exposures (Phase_1A discards, Hard Gate kills, partial block).
 */
UENUM(BlueprintType)
enum class ETMTrialOutcome : uint8
{
    Unresolved UMETA(DisplayName = "Unresolved"),
    Hit UMETA(DisplayName = "Hit"),
    Omission UMETA(DisplayName = "Omission"),
    CorrectRejection UMETA(DisplayName = "Correct Rejection"),
    Commission UMETA(DisplayName = "Commission"),
    Void UMETA(DisplayName = "Void")
};

/** Stimulus identity — determines Go/No-Go track assignment. */
UENUM(BlueprintType)
enum class ETMStimulusType : uint8
{
    Go UMETA(DisplayName = "Go - Deep Indigo Sphere   (80%)"),
    NoGo UMETA(DisplayName = "No-Go - Dark Crimson Sphere (20%)")
};

/**
 * Direction of staircase step, shared by both Track A and Track B.
 * Used in logging; actual delta magnitude is track-specific.
 */
UENUM(BlueprintType)
enum class ETMStaircaseStep : uint8
{
    Ascend UMETA(DisplayName = "Ascend  (difficulty up)"),
    Descend UMETA(DisplayName = "Descend (difficulty down)"),
    Hold_Deadband UMETA(DisplayName = "Hold - Deadband"),
    Hold_Microperturbation UMETA(DisplayName = "Hold - Deadband triggered micro-perturbation")
};

// ============================================================
//  SECTION 2 — PHYSICAL LIMITS & PROTOCOL CONSTANTS
// ============================================================

namespace TranquilMind
{
    inline constexpr float SESSION_TOTAL_SEC = 1200.0f;
    inline constexpr float PHASE_1A_END_SEC = 2.0f;
    inline constexpr float PHASE_1B_END_SEC = 12.0f;
    inline constexpr float PHASE_II_END_SEC = 1080.0f;
    inline constexpr float PHASE_III_END_SEC = 1200.0f;

    inline constexpr float BLOCK_DURATION_SEC = 30.0f;
    inline constexpr float RESPONSE_WINDOW_MS = 1500.0f;
    inline constexpr float DEBOUNCE_MIN_MS = 50.0f;

    inline constexpr float MIN_ISI_MS = 800.0f;
    inline constexpr float MAX_ISI_MS = 4000.0f;
    inline constexpr float ISI_STEP_MAJOR_MS = 100.0f;
    inline constexpr float ISI_STEP_MICRO_MS = 50.0f;
    inline constexpr int32 DEADBAND_ISI_THRESHOLD = 3;

    inline constexpr float MIN_NOISE_ALPHA = 0.0f;
    inline constexpr float MAX_NOISE_ALPHA = 0.8f;
    inline constexpr float NOISE_STEP_MAJOR = 0.10f;
    inline constexpr float NOISE_STEP_MICRO = 0.05f;
    inline constexpr int32 DEADBAND_NOISE_THRESHOLD = 3;

    inline constexpr float TIEBREAK_DEPTH_DELTA_CM = 5.0f;
    inline constexpr float TIEBREAK_GAZE_DELTA_DEG = 0.5f;

    inline constexpr float HARDGATE_GAZE_SMOOTHING_MS = 500.0f;
    inline constexpr float HARDGATE_OFFCENTER_ACCUM_SEC = 3.0f;
    inline constexpr float HARDGATE_RESUME_DWELL_SEC = 3.0f;
    inline constexpr float HARDGATE_SUSPEND_ABORT_SEC = 30.0f;

    inline constexpr int32 SPAM_BLOCK_THRESHOLD = 15;
    inline constexpr int32 SPAM_CONSECUTIVE_ABORT = 3;

    inline constexpr float HMD_REMOVAL_ABORT_SEC = 10.0f;

    inline constexpr float QA_MAX_VOID_RATE = 0.15f;

    inline constexpr float RCS_COMMISSION_PENALTY = 1.5f;
}

// ============================================================
//  SECTION 3 — STATISTICAL STRUCTURES  (double-entry ledger)
// ============================================================

/**
 * Per-30s block accumulator.
 */
USTRUCT(BlueprintType)
struct TRANQUILMIND_API FBlockStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Block|Identity")
    int32 BlockIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Identity")
    float WallClockStartSec = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Ledger")
    int32 Trial_Count_Total = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Ledger")
    int32 Trial_Count_Valid = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Outcomes")
    int32 N_Hit = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Outcomes")
    int32 N_Omission = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Outcomes")
    int32 N_CorrectRejection = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Outcomes")
    int32 N_Commission = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Outcomes")
    int32 N_Void = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|RT")
    TArray<float> RT_Samples_MS;

    UPROPERTY(BlueprintReadOnly, Category = "Block|RT")
    float RT_Median_MS = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Staircase")
    float Param_ISI_MS = TranquilMind::MAX_ISI_MS;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Staircase")
    float Param_NoiseAlpha = TranquilMind::MIN_NOISE_ALPHA;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Staircase")
    ETMStaircaseStep StepTaken_TrackA = ETMStaircaseStep::Hold_Deadband;

    UPROPERTY(BlueprintReadOnly, Category = "Block|Staircase")
    ETMStaircaseStep StepTaken_TrackB = ETMStaircaseStep::Hold_Deadband;

    UPROPERTY(BlueprintReadOnly, Category = "Block|QA")
    int32 Spam_Counter = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Block|QA")
    bool Flag_Spam_Suspect = false;

    UPROPERTY(BlueprintReadOnly, Category = "Block|HardGate")
    float HardGate_AccumOffcenter_SEC = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Block|HardGate")
    int32 HardGate_FireCount = 0;
};

/**
 * Full-session aggregate.
 */
USTRUCT(BlueprintType)
struct TRANQUILMIND_API FSessionStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Session|Identity")
    FString SessionID;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Identity")
    FDateTime SessionStartUTC;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Identity")
    ETMSessionPhase TerminatedAtPhase = ETMSessionPhase::Phase_Terminated;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Identity")
    ETMAbortReason TerminationReason = ETMAbortReason::None;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Identity")
    bool bProtocolCompliant = true;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_Trial_Count_Total = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_Trial_Count_Valid = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_N_Hit = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_N_Omission = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_N_CorrectRejection = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_N_Commission = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Ledger")
    int32 Total_N_Void = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Baseline")
    float RT_Baseline_MS = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Baseline")
    bool bBaselineEstablished = false;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Staircase")
    float Current_ISI_MS = TranquilMind::MAX_ISI_MS;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Staircase")
    float Current_NoiseAlpha = TranquilMind::MIN_NOISE_ALPHA;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Staircase")
    int32 Deadband_ISI = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Staircase")
    int32 Deadband_Noise = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Spam")
    int32 ConsecutiveSpamBlocks = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Session|QA")
    float Void_Rate = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Session|QA")
    bool bQA_LowExposure = false;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Blocks")
    TArray<FBlockStats> BlockHistory;

    UPROPERTY(BlueprintReadOnly, Category = "Session|Offline")
    float RCS_mod = 0.0f;
};

/**
 * Empty UObject wrapper so this header can exist as a reflected type asset.
 */
UCLASS(BlueprintType)
class TRANQUILMIND_API UTranquilMindTypes : public UObject
{
    GENERATED_BODY()
};
