// TranquilMind Research Mode — fixed-parameter configuration (Phase 1).
//
// This struct is the SINGLE SOURCE OF TRUTH for all Research-mode protocol
// parameters. No Research protocol value may be hard-coded elsewhere; every
// consumer (runner, scoring, logger, future adaptation) must read it from here.
//
// Authority: Docs/Research/TranquilMind_Protocol_Specification_v1.md
// Population: adolescents ~13-17. All values are PILOT DEFAULTS, not validated optima.

#pragma once

#include "CoreMinimal.h"
#include "TMResearchConfig.generated.h"

USTRUCT(BlueprintType)
struct TRANQUILMIND_API FTMResearchConfig
{
    GENERATED_BODY()

    // ---- Sequence / ratio (exact 80:20) ----

    /** Exact NOGO ratio. GO probability is (1 - this). Kept exact per block, not Bernoulli. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "0.05", ClampMax = "0.5"))
    float TargetNoGoRatio = 0.20f;

    /** Trials per block. round(TrialsPerBlock * TargetNoGoRatio) NOGO; remainder GO. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "10", ClampMax = "500"))
    int32 TrialsPerBlock = 50;

    /** Max consecutive GO trials allowed. Provisional engineering constraint. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxConsecutiveGo = 6;

    /** Max consecutive NOGO trials allowed. Default 1 (no back-to-back NOGO). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "1", ClampMax = "5"))
    int32 MaxConsecutiveNoGo = 1;

    /** Minimum number of GO trials required after a NOGO before the next NOGO. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "0", ClampMax = "10"))
    int32 MinGapAfterNoGo = 1;

    /** No NOGO within the first N trials of a block (>=1 forbids NOGO on trial 0). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "0", ClampMax = "10"))
    int32 ForbidNoGoFirstN = 1;

    /** Number of contiguous segments across which NOGO trials are balanced (early/mid/late). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence",
        meta = (ClampMin = "1", ClampMax = "10"))
    int32 SegmentCount = 3;

    /** Seed for training blocks. Deterministic replay: same seed + params => same schedule. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence")
    int32 TrainingSeed = 20260708;

    /** Separate seed for evaluation blocks. Kept independent from TrainingSeed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Sequence")
    int32 EvaluationSeed = 19850611;

    // ---- Stimulus timing ----

    /** How long the bubble is visible before it is hidden, per trial (ms). Frozen in Phase 1. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Timing",
        meta = (ClampMin = "100.0", ClampMax = "1000.0", Units = "Milliseconds"))
    float StimulusVisibleMs = 400.0f;

    /**
     * THE authoritative response-window value (ms), measured from stimulus onset.
     * Used by: trial expiry, RT validity, omission resolution, logging, and future adaptation.
     * A response may be accepted after visual offset but before this deadline.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Timing",
        meta = (ClampMin = "200.0", ClampMax = "5000.0", Units = "Milliseconds"))
    float ResponseWindowMs = 1200.0f;

    // ---- ITI ----

    /** Mean inter-trial interval (ms), applied AFTER each trial resolves. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Timing",
        meta = (ClampMin = "200.0", ClampMax = "5000.0", Units = "Milliseconds"))
    float ITIMeanMs = 1200.0f;

    /** Uniform jitter fraction. 0.30 => ITI drawn uniformly in [mean*0.7, mean*1.3]. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Timing",
        meta = (ClampMin = "0.0", ClampMax = "0.9"))
    float ITIJitterFraction = 0.30f;

    // ---- Future-use fields (present in config, DISABLED in Phase 1) ----

    /**
     * Response-window adaptation is NOT implemented in Phase 1. These bounds/step
     * exist only so future work reads them from the same authoritative config.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|FutureAdaptation",
        meta = (ClampMin = "200.0", ClampMax = "5000.0", Units = "Milliseconds"))
    float ResponseWindowMinMs = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|FutureAdaptation",
        meta = (ClampMin = "200.0", ClampMax = "5000.0", Units = "Milliseconds"))
    float ResponseWindowMaxMs = 1400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|FutureAdaptation",
        meta = (ClampMin = "10.0", ClampMax = "500.0", Units = "Milliseconds"))
    float ResponseWindowStepMs = 50.0f;

    /** Master switch. MUST remain false in Phase 1. Guards any future staircase code. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|FutureAdaptation")
    bool bEnableResponseWindowAdaptation = false;

    // ---- Stimulus placement (fixed location) ----

    /**
     * Fixed spatial presentation location relative to the participant's camera at
     * trial onset: forward distance and vertical offset (cm). Research stimuli do
     * NOT move in depth. Kept modest so the bubble sits comfortably in view.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Placement",
        meta = (ClampMin = "50.0", ClampMax = "1000.0", Units = "cm"))
    float FixedForwardDistanceCm = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Placement",
        meta = (Units = "cm"))
    float FixedVerticalOffsetCm = 0.0f;

    /** Uniform world scale applied to the stimulus actor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research|Placement",
        meta = (ClampMin = "0.05", ClampMax = "5.0"))
    float StimulusScale = 1.0f;

    // ---- Convenience accessors (single authority for derived values) ----

    /** GO probability implied by the exact NOGO ratio. */
    float GetGoProbability() const { return 1.0f - TargetNoGoRatio; }

    float GetITIMinMs() const { return ITIMeanMs * (1.0f - ITIJitterFraction); }
    float GetITIMaxMs() const { return ITIMeanMs * (1.0f + ITIJitterFraction); }
};
