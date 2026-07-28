// Minimal non-world lifecycle coverage for interrupted Research finalization.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "TMResearchRunner.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TMLifecycleTestUtil
{
    FString UniqueSessionID(const TCHAR* Suffix)
    {
        return FString::Printf(TEXT("Automation-%s-%s"),
            Suffix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }

    int32 CountOccurrences(const FString& Text, const FString& Needle)
    {
        int32 Count = 0;
        int32 SearchFrom = 0;
        while (true)
        {
            const int32 FoundAt = Text.Find(Needle, ESearchCase::CaseSensitive,
                ESearchDir::FromStart, SearchFrom);
            if (FoundAt == INDEX_NONE)
            {
                return Count;
            }
            ++Count;
            SearchFrom = FoundAt + Needle.Len();
        }
    }

    bool LoadLog(const FString& FilePath, FString& OutText)
    {
        return FFileHelper::LoadFileToString(OutText, *FilePath);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMResearchLifecycleTest,
    "TranquilMind.Research.Lifecycle.Shutdown",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTMResearchLifecycleTest::RunTest(const FString&)
{
    const FTMResearchConfig Config;

    // No active trial: shutdown finalizes once and repeated shutdown is a no-op.
    UTMResearchRunner* NoActive = NewObject<UTMResearchRunner>();
    TestTrue(TEXT("initialize no-active runner"),
        NoActive->Initialize(nullptr, nullptr, Config, Config.TrainingSeed,
            TMLifecycleTestUtil::UniqueSessionID(TEXT("NoActive"))));
    NoActive->ShutdownResearchRun(TEXT("ComponentEndPlay"));
    const int32 NoActiveRecords = NoActive->Logger.GetRecordsWritten();
    FString NoActiveLog;
    TestTrue(TEXT("load no-active JSONL"),
        TMLifecycleTestUtil::LoadLog(NoActive->Logger.GetFilePath(), NoActiveLog));
    TestEqual(TEXT("no-active footer exactly once"),
        TMLifecycleTestUtil::CountOccurrences(NoActiveLog, TEXT("\"type\":\"session_footer\"")), 1);
    TestEqual(TEXT("no-active completion is interrupted"),
        TMLifecycleTestUtil::CountOccurrences(NoActiveLog, TEXT("\"completion\":\"interrupted\"")), 2);
    NoActive->ShutdownResearchRun(TEXT("RepeatedShutdown"));
    TestEqual(TEXT("repeated no-active shutdown writes no record"),
        NoActive->Logger.GetRecordsWritten(), NoActiveRecords);
    FString NoActiveLogAfterRepeat;
    TestTrue(TEXT("reload no-active JSONL"),
        TMLifecycleTestUtil::LoadLog(NoActive->Logger.GetFilePath(), NoActiveLogAfterRepeat));
    TestEqual(TEXT("repeated no-active shutdown writes no footer"),
        TMLifecycleTestUtil::CountOccurrences(NoActiveLogAfterRepeat, TEXT("\"type\":\"session_footer\"")), 1);

    // Active trial: exactly one Void, no scored outcome, one footer.
    UTMResearchRunner* Active = NewObject<UTMResearchRunner>();
    TestTrue(TEXT("initialize active runner"),
        Active->Initialize(nullptr, nullptr, Config, Config.TrainingSeed,
            TMLifecycleTestUtil::UniqueSessionID(TEXT("Active"))));
    Active->BeginTrial();
    TestEqual(TEXT("trial is active before shutdown"), Active->State, ETMResearchState::TrialActive);
    Active->ShutdownResearchRun(TEXT("WorldTeardown"));
    TestEqual(TEXT("active shutdown counts one Void"), Active->N_Void, 1);
    TestEqual(TEXT("active shutdown counts no Hit"), Active->N_Hit, 0);
    TestEqual(TEXT("active shutdown counts no Omission"), Active->N_Omission, 0);
    TestEqual(TEXT("active shutdown counts no CorrectRejection"), Active->N_CorrectRejection, 0);
    TestEqual(TEXT("active shutdown counts no Commission"), Active->N_Commission, 0);
    FString ActiveLog;
    TestTrue(TEXT("load active JSONL"),
        TMLifecycleTestUtil::LoadLog(Active->Logger.GetFilePath(), ActiveLog));
    TestEqual(TEXT("active shutdown writes one trial record"),
        TMLifecycleTestUtil::CountOccurrences(ActiveLog, TEXT("\"type\":\"trial\"")), 1);
    TestEqual(TEXT("active shutdown writes one Void"),
        TMLifecycleTestUtil::CountOccurrences(ActiveLog, TEXT("\"outcome\":\"Void\"")), 1);
    TestEqual(TEXT("active shutdown preserves abort reason"),
        TMLifecycleTestUtil::CountOccurrences(ActiveLog, TEXT("\"voidReason\":\"WorldTeardown\"")), 1);
    TestEqual(TEXT("active shutdown footer exactly once"),
        TMLifecycleTestUtil::CountOccurrences(ActiveLog, TEXT("\"type\":\"session_footer\"")), 1);
    const int32 ActiveRecords = Active->Logger.GetRecordsWritten();
    Active->ShutdownResearchRun(TEXT("RepeatedShutdown"));
    TestEqual(TEXT("repeated active shutdown writes no record"),
        Active->Logger.GetRecordsWritten(), ActiveRecords);

    // Completed trial waiting in ITI: preserve the Hit once; never add a Void.
    UTMResearchRunner* Completed = NewObject<UTMResearchRunner>();
    TestTrue(TEXT("initialize completed runner"),
        Completed->Initialize(nullptr, nullptr, Config, Config.TrainingSeed,
            TMLifecycleTestUtil::UniqueSessionID(TEXT("Completed"))));
    Completed->BeginTrial();
    Completed->bVisualHidden = true;
    Completed->CurRealizedOffsetSec =
        Completed->CurRealizedOnsetSec + Config.StimulusVisibleMs / 1000.0;
    Completed->bBehaviorResolved = true;
    Completed->CurOutcome = ETMTrialOutcome::Hit;
    Completed->CurResponseTsSec = Completed->CurRealizedOnsetSec + 0.2;
    Completed->CurBehavioralResolutionSec = Completed->CurResponseTsSec;
    Completed->CurRTMs = 200.0f;
    Completed->CompleteTrial();
    TestTrue(TEXT("completed trial is pending before shutdown"), Completed->bHasPending);
    Completed->ShutdownResearchRun(TEXT("ComponentDestroyed"));
    TestEqual(TEXT("completed shutdown retains one Hit"), Completed->N_Hit, 1);
    TestEqual(TEXT("completed shutdown adds no Void"), Completed->N_Void, 0);
    FString CompletedLog;
    TestTrue(TEXT("load completed JSONL"),
        TMLifecycleTestUtil::LoadLog(Completed->Logger.GetFilePath(), CompletedLog));
    TestEqual(TEXT("completed shutdown writes trial once"),
        TMLifecycleTestUtil::CountOccurrences(CompletedLog, TEXT("\"type\":\"trial\"")), 1);
    TestEqual(TEXT("completed shutdown writes Hit once"),
        TMLifecycleTestUtil::CountOccurrences(CompletedLog, TEXT("\"outcome\":\"Hit\"")), 1);
    TestEqual(TEXT("completed shutdown writes no Void"),
        TMLifecycleTestUtil::CountOccurrences(CompletedLog, TEXT("\"outcome\":\"Void\"")), 0);
    TestEqual(TEXT("completed shutdown footer exactly once"),
        TMLifecycleTestUtil::CountOccurrences(CompletedLog, TEXT("\"type\":\"session_footer\"")), 1);
    const int32 CompletedRecords = Completed->Logger.GetRecordsWritten();
    Completed->ShutdownResearchRun(TEXT("RepeatedShutdown"));
    TestEqual(TEXT("repeated completed shutdown writes no record"),
        Completed->Logger.GetRecordsWritten(), CompletedRecords);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
