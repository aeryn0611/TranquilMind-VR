#include "TMSequenceGenerator.h"

// ============================================================================
//  Deterministic constrained GO/NOGO schedule generator with TRUE per-segment
//  balancing.
//
//  Algorithm:
//    1. Partition the N trial positions into SegmentCount CONTIGUOUS segments,
//       lengths differing by at most 1 (the first (N % S) segments are longer).
//    2. Assign a per-segment NOGO QUOTA, quotas differing by at most 1. The
//       remainder (K % S) segments that receive the +1 are chosen DETERMINISTICALLY
//       from the seed (seeded shuffle of segment indices).
//    3. Place NOGO by seeded BACKTRACKING that fills each segment EXACTLY to its
//       quota while enforcing, across segment boundaries, every global constraint:
//         - no NOGO in the first ForbidNoGoFirstN positions
//         - max consecutive NOGO
//         - min GO gap after a NOGO
//         - max consecutive GO (leading, interior, trailing, and boundary-spanning)
//    The segment quotas are part of the generation itself (enforced during search),
//    not validated after the fact.
//
//  Backtracking is complete: it finds a valid arrangement iff one exists, so an
//  infeasible configuration FAILS LOUDLY (never silently relaxed). A node budget
//  guards against pathological blow-up and is reported distinctly.
//
//  Determinism: SplitMix64 seeded once; same seed + params => identical schedule + hash.
// ============================================================================

namespace
{
    struct FSplitMix64
    {
        uint64 State;
        explicit FSplitMix64(uint64 Seed) : State(Seed) {}
        uint64 Next()
        {
            State += 0x9E3779B97F4A7C15ULL;
            uint64 Z = State;
            Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
            return Z ^ (Z >> 31);
        }
        int32 NextInt(int32 N)
        {
            if (N <= 1) return 0;
            const uint64 UN = static_cast<uint64>(N);
            const uint64 Limit = MAX_uint64 - (MAX_uint64 % UN);
            uint64 R;
            do { R = Next(); } while (R >= Limit);
            return static_cast<int32>(R % UN);
        }
    };

    void SeededShuffle(TArray<int32>& Indices, FSplitMix64& Rng)
    {
        for (int32 i = Indices.Num() - 1; i > 0; --i)
        {
            const int32 j = Rng.NextInt(i + 1);
            Indices.Swap(i, j);
        }
    }

    // Recursive constrained placement with per-segment quotas.
    struct FBacktrack
    {
        int32 N = 0, MaxGo = 0, MaxNoGo = 0, MinGap = 0, ForbidFirst = 0, K = 0;
        const TArray<int32>* SegOf = nullptr;
        const TArray<int32>* SegEnd = nullptr;   // exclusive end index per segment
        const TArray<int32>* Quota = nullptr;
        TArray<int32> SegPlaced;
        TArray<uint8> Seq;                        // 0 = Go, 1 = NoGo
        FSplitMix64* Rng = nullptr;
        int64 Budget = 0;
        bool bBudgetExhausted = false;

        bool Solve(int32 Pos, int32 RunGo, int32 RunNoGo, int32 TotalPlaced)
        {
            if (--Budget < 0)
            {
                bBudgetExhausted = true;
                return false;
            }
            if (Pos == N)
            {
                return TotalPlaced == K;
            }

            const int32 Seg = (*SegOf)[Pos];

            // Segment-fill feasibility: enough positions left in this segment to reach quota.
            if ((*Quota)[Seg] - SegPlaced[Seg] > (*SegEnd)[Seg] - Pos)
            {
                return false;
            }

            const bool bTryNoGoFirst = (Rng->Next() & 1ULL) != 0;

            for (int32 Iter = 0; Iter < 2; ++Iter)
            {
                const bool bDoNoGo = (Iter == 0) ? bTryNoGoFirst : !bTryNoGoFirst;

                if (bDoNoGo)
                {
                    if (Pos >= ForbidFirst && SegPlaced[Seg] < (*Quota)[Seg])
                    {
                        bool bOK;
                        if (RunNoGo > 0)
                        {
                            bOK = (RunNoGo < MaxNoGo);        // extend NOGO cluster within cap
                        }
                        else
                        {
                            bOK = (TotalPlaced == 0) || (RunGo >= MinGap); // gap since last NOGO
                        }
                        if (bOK)
                        {
                            Seq[Pos] = 1;
                            ++SegPlaced[Seg];
                            if (Solve(Pos + 1, 0, RunNoGo + 1, TotalPlaced + 1))
                            {
                                return true;
                            }
                            --SegPlaced[Seg];
                        }
                    }
                }
                else
                {
                    if (RunGo < MaxGo)
                    {
                        Seq[Pos] = 0;
                        if (Solve(Pos + 1, RunGo + 1, 0, TotalPlaced))
                        {
                            return true;
                        }
                    }
                }
            }
            return false;
        }
    };
}

uint64 FTMSequenceGenerator::HashSequence(const TArray<ETMStimulusType>& Sequence)
{
    uint64 Hash = 0xCBF29CE484222325ULL;
    const uint64 Prime = 0x00000100000001B3ULL;
    for (const ETMStimulusType S : Sequence)
    {
        const uint8 Byte = (S == ETMStimulusType::NoGo) ? 0x02 : 0x01;
        Hash ^= static_cast<uint64>(Byte);
        Hash *= Prime;
    }
    return Hash;
}

bool FTMSequenceGenerator::Generate(const FTMSequenceParams& Params, FTMSequenceResult& OutResult, FString& OutError)
{
    OutResult = FTMSequenceResult();
    OutError.Reset();

    const int32 N = Params.NTrials;
    const int32 MaxGo = Params.MaxConsecutiveGo;
    const int32 MaxNoGo = Params.MaxConsecutiveNoGo;
    const int32 MinGap = Params.MinGapAfterNoGo;
    const int32 ForbidFirst = Params.ForbidNoGoFirstN;
    const int32 S = Params.SegmentCount;

    // ---- Parameter validation ----
    if (N < 1) { OutError = TEXT("NTrials must be >= 1."); return false; }
    if (!(Params.TargetNoGoRatio > 0.0f && Params.TargetNoGoRatio < 1.0f))
    {
        OutError = FString::Printf(TEXT("TargetNoGoRatio must be in (0,1); got %f."), Params.TargetNoGoRatio);
        return false;
    }
    if (MaxGo < 1 || MaxNoGo < 1) { OutError = TEXT("MaxConsecutiveGo/NoGo must be >= 1."); return false; }
    if (MinGap < 0 || ForbidFirst < 0) { OutError = TEXT("MinGapAfterNoGo/ForbidNoGoFirstN must be >= 0."); return false; }
    if (S < 1) { OutError = TEXT("SegmentCount must be >= 1."); return false; }

    const int32 K = FMath::RoundToInt(static_cast<float>(N) * Params.TargetNoGoRatio); // NOGO
    const int32 G = N - K;                                                             // GO
    if (K < 0 || G < 0) { OutError = TEXT("Computed negative GO/NOGO count."); return false; }

    // ---- Segment partition (contiguous; lengths differ by <= 1) ----
    const int32 SegEffective = FMath::Min(S, N); // never more segments than trials
    TArray<int32> SegStart, SegEnd;
    SegStart.SetNumZeroed(SegEffective);
    SegEnd.SetNumZeroed(SegEffective);
    {
        const int32 Base = N / SegEffective;
        const int32 Rem = N % SegEffective;
        int32 Cursor = 0;
        for (int32 s = 0; s < SegEffective; ++s)
        {
            const int32 Len = Base + (s < Rem ? 1 : 0);
            SegStart[s] = Cursor;
            SegEnd[s] = Cursor + Len;
            Cursor += Len;
        }
    }

    TArray<int32> SegOf;
    SegOf.SetNumZeroed(N);
    for (int32 s = 0; s < SegEffective; ++s)
    {
        for (int32 p = SegStart[s]; p < SegEnd[s]; ++p) { SegOf[p] = s; }
    }

    FSplitMix64 Rng(Params.Seed);

    // ---- Per-segment NOGO quotas (differ by <= 1; remainder seed-chosen) ----
    TArray<int32> Quota;
    Quota.Init(K / SegEffective, SegEffective);
    {
        const int32 QRem = K % SegEffective;
        if (QRem > 0)
        {
            TArray<int32> Order;
            Order.Reserve(SegEffective);
            for (int32 s = 0; s < SegEffective; ++s) { Order.Add(s); }
            SeededShuffle(Order, Rng); // deterministic remainder assignment
            for (int32 i = 0; i < QRem; ++i) { Quota[Order[i]] += 1; }
        }
    }

    // ---- Degenerate: no NOGO ----
    if (K == 0)
    {
        if (G > MaxGo)
        {
            OutError = FString::Printf(TEXT("Infeasible: %d GO with no NOGO exceeds MaxConsecutiveGo=%d."), G, MaxGo);
            return false;
        }
        OutResult.Sequence.Init(ETMStimulusType::Go, G);
        OutResult.GoCount = G; OutResult.NoGoCount = 0;
        OutResult.SegmentNoGoCounts.Init(0, SegEffective);
        OutResult.SegmentStartIndices = SegStart;
        OutResult.SequenceHash = HashSequence(OutResult.Sequence);
        return true;
    }

    // ---- Fast infeasibility: leading run cap ----
    if (ForbidFirst > MaxGo)
    {
        OutError = FString::Printf(TEXT("Infeasible: ForbidNoGoFirstN=%d exceeds MaxConsecutiveGo=%d (leading GO run cap)."), ForbidFirst, MaxGo);
        return false;
    }

    // ---- Seeded backtracking placement ----
    FBacktrack BT;
    BT.N = N; BT.MaxGo = MaxGo; BT.MaxNoGo = MaxNoGo; BT.MinGap = MinGap;
    BT.ForbidFirst = ForbidFirst; BT.K = K;
    BT.SegOf = &SegOf; BT.SegEnd = &SegEnd; BT.Quota = &Quota;
    BT.SegPlaced.SetNumZeroed(SegEffective);
    BT.Seq.SetNumZeroed(N);
    BT.Rng = &Rng;
    BT.Budget = 2000000; // generous; typical solves use far fewer nodes

    const bool bSolved = BT.Solve(0, 0, 0, 0);

    if (!bSolved)
    {
        if (BT.bBudgetExhausted)
        {
            OutError = FString::Printf(
                TEXT("Placement exceeded node budget (seed=%llu). Constraints may be over-tight; not relaxing."),
                static_cast<unsigned long long>(Params.Seed));
        }
        else
        {
            OutError = FString::Printf(
                TEXT("Infeasible: no schedule satisfies the constraints (N=%d, K=%d, maxGo=%d, maxNoGo=%d, minGap=%d, forbidFirst=%d, segments=%d)."),
                N, K, MaxGo, MaxNoGo, MinGap, ForbidFirst, SegEffective);
        }
        return false;
    }

    // ---- Materialize + defensive validation ----
    OutResult.Sequence.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        OutResult.Sequence.Add(BT.Seq[i] == 1 ? ETMStimulusType::NoGo : ETMStimulusType::Go);
    }

    // Recount per-segment NOGO and verify all global constraints defensively.
    OutResult.SegmentNoGoCounts.Init(0, SegEffective);
    OutResult.SegmentStartIndices = SegStart;
    {
        int32 GoN = 0, NoGoN = 0, RunGo = 0, RunNoGo = 0, MaxRunGo = 0, MaxRunNoGo = 0, FirstNoGo = -1;
        int32 GapSinceNoGo = 0, MinObservedGap = MAX_int32;
        for (int32 i = 0; i < N; ++i)
        {
            if (OutResult.Sequence[i] == ETMStimulusType::NoGo)
            {
                ++NoGoN; ++RunNoGo;
                if (FirstNoGo < 0) { FirstNoGo = i; }
                if (NoGoN > 1 && RunNoGo == 1) { MinObservedGap = FMath::Min(MinObservedGap, GapSinceNoGo); }
                RunGo = 0; GapSinceNoGo = 0;
                OutResult.SegmentNoGoCounts[SegOf[i]] += 1;
            }
            else
            {
                ++GoN; ++RunGo; RunNoGo = 0; ++GapSinceNoGo;
            }
            MaxRunGo = FMath::Max(MaxRunGo, RunGo);
            MaxRunNoGo = FMath::Max(MaxRunNoGo, RunNoGo);
        }
        if (GoN != G || NoGoN != K)
        { OutError = FString::Printf(TEXT("Internal: counts GO=%d/%d NOGO=%d/%d."), GoN, G, NoGoN, K); OutResult = FTMSequenceResult(); return false; }
        if (MaxRunGo > MaxGo)
        { OutError = FString::Printf(TEXT("Internal: max GO run %d > %d."), MaxRunGo, MaxGo); OutResult = FTMSequenceResult(); return false; }
        if (MaxRunNoGo > MaxNoGo)
        { OutError = FString::Printf(TEXT("Internal: max NOGO run %d > %d."), MaxRunNoGo, MaxNoGo); OutResult = FTMSequenceResult(); return false; }
        if (FirstNoGo >= 0 && FirstNoGo < ForbidFirst)
        { OutError = FString::Printf(TEXT("Internal: NOGO at %d within first %d."), FirstNoGo, ForbidFirst); OutResult = FTMSequenceResult(); return false; }
        if (MinObservedGap != MAX_int32 && MinObservedGap < MinGap)
        { OutError = FString::Printf(TEXT("Internal: min gap %d < %d."), MinObservedGap, MinGap); OutResult = FTMSequenceResult(); return false; }

        // Per-segment quota exactness + balance (differ by <= 1).
        int32 QLo = MAX_int32, QHi = MIN_int32;
        for (int32 s = 0; s < SegEffective; ++s)
        {
            if (OutResult.SegmentNoGoCounts[s] != Quota[s])
            { OutError = FString::Printf(TEXT("Internal: segment %d placed %d != quota %d."), s, OutResult.SegmentNoGoCounts[s], Quota[s]); OutResult = FTMSequenceResult(); return false; }
            QLo = FMath::Min(QLo, Quota[s]);
            QHi = FMath::Max(QHi, Quota[s]);
        }
        if (QHi - QLo > 1)
        { OutError = FString::Printf(TEXT("Internal: segment quota spread %d > 1."), QHi - QLo); OutResult = FTMSequenceResult(); return false; }
    }

    OutResult.GoCount = G;
    OutResult.NoGoCount = K;
    OutResult.SequenceHash = HashSequence(OutResult.Sequence);
    return true;
}
