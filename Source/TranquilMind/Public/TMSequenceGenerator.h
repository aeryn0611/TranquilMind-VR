// TranquilMind — deterministic constrained GO/NOGO sequence generator (Phase 1).
//
// Produces a reproducible, non-misleading 80:20 schedule per block:
//   - EXACT NOGO count (no ratio drift; not Bernoulli)
//   - no NOGO within the first N trials
//   - run-length caps for GO and NOGO
//   - minimum GO gap after a NOGO
//   - NOGO balanced across contiguous segments (early / middle / late)
//   - same seed + params => identical sequence (byte-for-byte), with an FNV-1a hash
//   - INFEASIBLE constraints FAIL LOUDLY (returns false); constraints are never
//     silently relaxed.
//
// The core uses a self-contained SplitMix64 RNG (not FRandomStream) so the exact
// same algorithm is portable and deterministic across engine and standalone builds.
//
// Authority: Docs/Research/TranquilMind_Protocol_Specification_v1.md §8.

#pragma once

#include "CoreMinimal.h"
#include "../TranquilMindTypes.h"
#include "TMResearchConfig.h"

struct FTMSequenceParams
{
    int32 NTrials = 50;
    float TargetNoGoRatio = 0.20f;
    int32 MaxConsecutiveGo = 6;
    int32 MaxConsecutiveNoGo = 1;
    int32 MinGapAfterNoGo = 1;
    int32 ForbidNoGoFirstN = 1;
    int32 SegmentCount = 3;
    uint64 Seed = 0;

    static FTMSequenceParams FromConfig(const FTMResearchConfig& Config, uint64 InSeed)
    {
        FTMSequenceParams P;
        P.NTrials = Config.TrialsPerBlock;
        P.TargetNoGoRatio = Config.TargetNoGoRatio;
        P.MaxConsecutiveGo = Config.MaxConsecutiveGo;
        P.MaxConsecutiveNoGo = Config.MaxConsecutiveNoGo;
        P.MinGapAfterNoGo = Config.MinGapAfterNoGo;
        P.ForbidNoGoFirstN = Config.ForbidNoGoFirstN;
        P.SegmentCount = Config.SegmentCount;
        P.Seed = InSeed;
        return P;
    }
};

struct FTMSequenceResult
{
    /** Ordered schedule, one entry per trial. */
    TArray<ETMStimulusType> Sequence;

    int32 GoCount = 0;
    int32 NoGoCount = 0;

    /** NOGO count actually placed in each contiguous segment (early / middle / late ...). */
    TArray<int32> SegmentNoGoCounts;

    /** Trial index where each segment starts (parallel to SegmentNoGoCounts). */
    TArray<int32> SegmentStartIndices;

    /** FNV-1a 64-bit hash over the sequence, for replay verification / logging. */
    uint64 SequenceHash = 0;
};

class TRANQUILMIND_API FTMSequenceGenerator
{
public:
    /**
     * Generate a constrained schedule. Returns true on success; on failure returns
     * false and fills OutError (infeasible constraints, bad params). On failure the
     * result is left empty — callers MUST treat this as fatal and never proceed with
     * a partial/relaxed schedule.
     */
    static bool Generate(const FTMSequenceParams& Params, FTMSequenceResult& OutResult, FString& OutError);

    /** FNV-1a 64-bit over the sequence (Go=0x01, NoGo=0x02 byte stream). */
    static uint64 HashSequence(const TArray<ETMStimulusType>& Sequence);
};
