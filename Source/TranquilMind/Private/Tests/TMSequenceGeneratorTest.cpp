// Automation tests for FTMSequenceGenerator.
// Run in the editor via Session Frontend > Automation, filter "TranquilMind".
// Mirrors the standalone sandbox tests (all validated before device build).

#include "Misc/AutomationTest.h"
#include "TMSequenceGenerator.h"
#include "TMResearchConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMSeqTestUtil
{
    FTMSequenceParams DefaultParams(uint64 Seed)
    {
        FTMSequenceParams P;
        P.NTrials = 50;
        P.TargetNoGoRatio = 0.20f;
        P.MaxConsecutiveGo = 6;
        P.MaxConsecutiveNoGo = 1;
        P.MinGapAfterNoGo = 1;
        P.ForbidNoGoFirstN = 1;
        P.SegmentCount = 3;
        P.Seed = Seed;
        return P;
    }

    void RunStats(const TArray<ETMStimulusType>& S, int32& MaxGoRun, int32& MaxNoGoRun, int32& FirstNoGoIdx)
    {
        MaxGoRun = MaxNoGoRun = 0; FirstNoGoIdx = -1;
        int32 RunGo = 0, RunNoGo = 0;
        for (int32 i = 0; i < S.Num(); ++i)
        {
            if (S[i] == ETMStimulusType::NoGo) { ++RunNoGo; RunGo = 0; if (FirstNoGoIdx < 0) FirstNoGoIdx = i; }
            else { ++RunGo; RunNoGo = 0; }
            MaxGoRun = FMath::Max(MaxGoRun, RunGo);
            MaxNoGoRun = FMath::Max(MaxNoGoRun, RunNoGo);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqExactCountsTest,
    "TranquilMind.Research.Sequence.ExactCounts",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqExactCountsTest::RunTest(const FString&)
{
    FTMSequenceResult R; FString Err;
    const bool bOK = FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(20260708), R, Err);
    TestTrue(TEXT("generate succeeds"), bOK);
    TestEqual(TEXT("50 trials"), R.Sequence.Num(), 50);
    TestEqual(TEXT("40 GO"), R.GoCount, 40);
    TestEqual(TEXT("10 NOGO"), R.NoGoCount, 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqDefaultContractTest,
    "TranquilMind.Research.Sequence.DefaultContract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqDefaultContractTest::RunTest(const FString&)
{
    FTMSequenceResult R;
    FString Err;
    const bool bOK = FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(20260708), R, Err);
    if (!TestTrue(TEXT("compiled default schedule generates"), bOK))
    {
        AddError(FString::Printf(TEXT("generation error: %s"), *Err));
        return false;
    }
    if (!TestEqual(TEXT("default segment start count"), R.SegmentStartIndices.Num(), 3) ||
        !TestEqual(TEXT("default segment NOGO count"), R.SegmentNoGoCounts.Num(), 3))
    {
        return false;
    }

    FString SequenceString;
    SequenceString.Reserve(R.Sequence.Num());
    for (const ETMStimulusType Stimulus : R.Sequence)
    {
        SequenceString.AppendChar(Stimulus == ETMStimulusType::NoGo ? TEXT('N') : TEXT('.'));
    }

    AddInfo(FString::Printf(
        TEXT("COMPILED DEFAULT CONTRACT | seed=20260708 | sequence=%s | hash=%llu | segmentStarts=[%d,%d,%d] | segmentNoGo=[%d,%d,%d]"),
        *SequenceString, static_cast<unsigned long long>(R.SequenceHash),
        R.SegmentStartIndices[0], R.SegmentStartIndices[1], R.SegmentStartIndices[2],
        R.SegmentNoGoCounts[0], R.SegmentNoGoCounts[1], R.SegmentNoGoCounts[2]));

    TestEqual(TEXT("exact default sequence"),
        SequenceString, FString(TEXT(".....N......N.N...N.N......N......N...N..N.N......")));
    TestEqual(TEXT("exact default hash"),
        static_cast<int64>(R.SequenceHash), static_cast<int64>(4922870221080512783ULL));
    TestEqual(TEXT("segment 0 start"), R.SegmentStartIndices[0], 0);
    TestEqual(TEXT("segment 1 start"), R.SegmentStartIndices[1], 17);
    TestEqual(TEXT("segment 2 start"), R.SegmentStartIndices[2], 34);
    TestEqual(TEXT("segment 0 length"), R.SegmentStartIndices[1] - R.SegmentStartIndices[0], 17);
    TestEqual(TEXT("segment 1 length"), R.SegmentStartIndices[2] - R.SegmentStartIndices[1], 17);
    TestEqual(TEXT("segment 2 length"), R.Sequence.Num() - R.SegmentStartIndices[2], 16);
    TestEqual(TEXT("segment 0 NOGO"), R.SegmentNoGoCounts[0], 3);
    TestEqual(TEXT("segment 1 NOGO"), R.SegmentNoGoCounts[1], 3);
    TestEqual(TEXT("segment 2 NOGO"), R.SegmentNoGoCounts[2], 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqConstraintsTest,
    "TranquilMind.Research.Sequence.Constraints",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqConstraintsTest::RunTest(const FString&)
{
    FTMSequenceResult R; FString Err;
    FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(20260708), R, Err);
    int32 MaxGo, MaxNoGo, FirstNoGo;
    TMSeqTestUtil::RunStats(R.Sequence, MaxGo, MaxNoGo, FirstNoGo);
    TestTrue(TEXT("max GO run <= 6"), MaxGo <= 6);
    TestTrue(TEXT("max NOGO run <= 1"), MaxNoGo <= 1);
    TestTrue(TEXT("no NOGO in first trial (index >= 1)"), FirstNoGo >= 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqSegmentBalanceTest,
    "TranquilMind.Research.Sequence.SegmentBalance",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqSegmentBalanceTest::RunTest(const FString&)
{
    FTMSequenceResult R; FString Err;
    const bool bOK = FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(20260708), R, Err);
    TestTrue(TEXT("generate succeeds"), bOK);

    // Segment counts are produced by the generation algorithm itself (per-segment quotas),
    // not derived after the fact.
    TestEqual(TEXT("segment count reported == 3"), R.SegmentNoGoCounts.Num(), 3);

    int32 Sum = 0, Lo = MAX_int32, Hi = MIN_int32;
    for (int32 C : R.SegmentNoGoCounts)
    {
        Sum += C; Lo = FMath::Min(Lo, C); Hi = FMath::Max(Hi, C);
    }
    TestEqual(TEXT("segment NOGO counts sum to 10"), Sum, 10);
    TestTrue(TEXT("per-segment NOGO counts differ by at most 1"), (Hi - Lo) <= 1);

    // Independently confirm the reported per-segment counts match the actual sequence.
    int32 Actual[3] = {0, 0, 0};
    for (int32 i = 0; i < R.Sequence.Num(); ++i)
    {
        if (R.Sequence[i] == ETMStimulusType::NoGo)
        {
            const int32 Seg = (i * 3) / R.Sequence.Num();
            Actual[FMath::Min(Seg, 2)] += 1;
        }
    }
    for (int32 s = 0; s < 3; ++s)
    {
        TestEqual(TEXT("reported segment count matches sequence"), R.SegmentNoGoCounts[s], Actual[s]);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqBoundaryStressTest,
    "TranquilMind.Research.Sequence.BoundaryStress",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqBoundaryStressTest::RunTest(const FString&)
{
    // N=9, K=3, 3 segments of length 3, quota 1 each, MaxConsecutiveGo=2. A naive
    // per-segment fill could leave a GO run of 3 straddling a segment boundary; the
    // generator must avoid that by construction (cross-boundary run constraint).
    FTMSequenceParams P;
    P.NTrials = 9;
    P.TargetNoGoRatio = 1.0f / 3.0f;
    P.MaxConsecutiveGo = 2;
    P.MaxConsecutiveNoGo = 1;
    P.MinGapAfterNoGo = 1;
    P.ForbidNoGoFirstN = 1;
    P.SegmentCount = 3;
    P.Seed = 42;

    FTMSequenceResult R; FString Err;
    const bool bOK = FTMSequenceGenerator::Generate(P, R, Err);
    TestTrue(TEXT("boundary-stress config is solvable"), bOK);
    if (bOK)
    {
        int32 MaxGo, MaxNoGo, FirstNoGo;
        TMSeqTestUtil::RunStats(R.Sequence, MaxGo, MaxNoGo, FirstNoGo);
        TestTrue(TEXT("cross-boundary max GO run <= 2"), MaxGo <= 2);
        TestTrue(TEXT("max NOGO run <= 1"), MaxNoGo <= 1);
        TestTrue(TEXT("first trial not NOGO"), FirstNoGo >= 1);
        TestEqual(TEXT("exactly 3 NOGO"), R.NoGoCount, 3);
        int32 Lo = MAX_int32, Hi = MIN_int32;
        for (int32 C : R.SegmentNoGoCounts) { Lo = FMath::Min(Lo, C); Hi = FMath::Max(Hi, C); }
        TestTrue(TEXT("per-segment quotas differ by at most 1"), (Hi - Lo) <= 1);
    }

    // Infeasible boundary case: MaxConsecutiveGo=1 with 6 GO / 3 NOGO cannot be arranged.
    FTMSequenceParams P2 = P;
    P2.MaxConsecutiveGo = 1;
    FTMSequenceResult R2; FString Err2;
    const bool bOK2 = FTMSequenceGenerator::Generate(P2, R2, Err2);
    TestFalse(TEXT("maxGo=1 with 6 GO fails loudly"), bOK2);
    TestTrue(TEXT("infeasible error populated"), !Err2.IsEmpty());
    TestEqual(TEXT("no partial schedule"), R2.Sequence.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqDeterminismTest,
    "TranquilMind.Research.Sequence.Determinism",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqDeterminismTest::RunTest(const FString&)
{
    FTMSequenceResult A, B; FString Err;
    FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(20260708), A, Err);
    FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(20260708), B, Err);
    TestTrue(TEXT("same seed => identical sequence"), A.Sequence == B.Sequence);
    TestEqual(TEXT("same seed => identical hash"), (int64)A.SequenceHash, (int64)B.SequenceHash);

    FTMSequenceResult C; FString Err2;
    FTMSequenceGenerator::Generate(TMSeqTestUtil::DefaultParams(19850611), C, Err2);
    TestFalse(TEXT("different seed => different sequence"), A.Sequence == C.Sequence);
    // Different-seed sequence must still be valid.
    TestEqual(TEXT("eval seed 40 GO"), C.GoCount, 40);
    TestEqual(TEXT("eval seed 10 NOGO"), C.NoGoCount, 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMSeqInfeasibleTest,
    "TranquilMind.Research.Sequence.InfeasibleFailsLoudly",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FTMSeqInfeasibleTest::RunTest(const FString&)
{
    // 40 GO cannot fit across 11 gaps at max run 3 (capacity 33) -> infeasible.
    FTMSequenceParams P = TMSeqTestUtil::DefaultParams(20260708);
    P.MaxConsecutiveGo = 3;
    FTMSequenceResult R; FString Err;
    const bool bOK = FTMSequenceGenerator::Generate(P, R, Err);
    TestFalse(TEXT("over-constrained returns false"), bOK);
    TestEqual(TEXT("no partial schedule emitted"), R.Sequence.Num(), 0);
    TestTrue(TEXT("error message is populated"), !Err.IsEmpty());

    // ForbidNoGoFirstN > MaxConsecutiveGo is also infeasible.
    FTMSequenceParams P2 = TMSeqTestUtil::DefaultParams(20260708);
    P2.ForbidNoGoFirstN = 7;
    FTMSequenceResult R2; FString Err2;
    TestFalse(TEXT("forbidFirst>maxGo returns false"), FTMSequenceGenerator::Generate(P2, R2, Err2));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
