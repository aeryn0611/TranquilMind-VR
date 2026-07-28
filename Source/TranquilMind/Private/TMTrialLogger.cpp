#include "TMTrialLogger.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogTMResearch, Log, All);

namespace
{
    const TCHAR* StimulusToStr(ETMStimulusType T)
    {
        return (T == ETMStimulusType::NoGo) ? TEXT("NOGO") : TEXT("GO");
    }

    const TCHAR* OutcomeToStr(ETMTrialOutcome O)
    {
        switch (O)
        {
        case ETMTrialOutcome::Hit:              return TEXT("Hit");
        case ETMTrialOutcome::Omission:         return TEXT("Omission");
        case ETMTrialOutcome::CorrectRejection: return TEXT("CorrectRejection");
        case ETMTrialOutcome::Commission:       return TEXT("Commission");
        case ETMTrialOutcome::Void:             return TEXT("Void");
        default:                                return TEXT("Unresolved");
        }
    }

    const TCHAR* ModeToStr(ETMOperatingMode M)
    {
        return (M == ETMOperatingMode::Research) ? TEXT("Research") : TEXT("Demo");
    }

    const TCHAR* BoolStr(bool b) { return b ? TEXT("true") : TEXT("false"); }
}

FString FTMTrialLogger::EscapeJson(const FString& In)
{
    FString Out;
    Out.Reserve(In.Len() + 2);
    for (const TCHAR C : In)
    {
        switch (C)
        {
        case '\"': Out += TEXT("\\\""); break;
        case '\\': Out += TEXT("\\\\"); break;
        case '\n': Out += TEXT("\\n");  break;
        case '\r': Out += TEXT("\\r");  break;
        case '\t': Out += TEXT("\\t");  break;
        default:   Out.AppendChar(C);   break;
        }
    }
    return Out;
}

bool FTMTrialLogger::BeginSession(const FString& InSessionID)
{
    SessionID = InSessionID;
    bOpen = false;
    bHealthy = false;
    WriteFailureCount = 0;
    RecordsWritten = 0;
    bFinalized = false;

    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TranquilMind"), TEXT("Research"));
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
    {
        PF.CreateDirectoryTree(*Dir);
    }

    FilePath = FPaths::Combine(Dir, FString::Printf(TEXT("%s.jsonl"), *SessionID));

    const FString Header = FString::Printf(
        TEXT("{\"type\":\"session_header\",\"sessionId\":\"%s\",\"startedUtc\":\"%s\",\"schemaVersion\":2}"),
        *EscapeJson(SessionID), *FDateTime::UtcNow().ToIso8601());

    const bool bWrote = FFileHelper::SaveStringToFile(Header + LINE_TERMINATOR, *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    if (bWrote)
    {
        bOpen = true;
        bHealthy = true;
        UE_LOG(LogTMResearch, Warning, TEXT("[Research][Logger] OPEN ok | session=%s | path=%s"), *SessionID, *FilePath);
    }
    else
    {
        bOpen = false;
        bHealthy = false;
        UE_LOG(LogTMResearch, Error,
            TEXT("[Research][Logger] OPEN FAILED | session=%s | path=%s | trial records will NOT be persisted (UE_LOG mirror only)."),
            *SessionID, *FilePath);
    }
    return bOpen;
}

bool FTMTrialLogger::AppendLine(const FString& Line)
{
    if (!bOpen)
    {
        return false; // never opened; do not claim persistence
    }

    const bool bWrote = FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(), EFileWrite::FILEWRITE_Append);

    if (bWrote)
    {
        ++RecordsWritten;
        return true;
    }

    ++WriteFailureCount;
    bHealthy = false; // persistence is no longer trustworthy
    UE_LOG(LogTMResearch, Error,
        TEXT("[Research][Logger] WRITE FAILED (#%d) | session=%s | path=%s | logger now UNHEALTHY."),
        WriteFailureCount, *SessionID, *FilePath);
    return false;
}

void FTMTrialLogger::LogTrial(const FTMTrialRecord& R)
{
    FString GsrJson = TEXT("null");
    if (R.bGSRAvailable)
    {
        GsrJson = FString::Printf(
            TEXT("{\"raw\":%d,\"smoothed\":%d,\"quality\":%d,\"deviceTsMs\":%lld,\"note\":\"1Hz latest sample, contextual only, NOT synchronized EDA\"}"),
            R.GSR_Raw, R.GSR_Smoothed, R.GSR_Quality, static_cast<long long>(R.GSR_DeviceTimestampMs));
    }

    const FString Line = FString::Printf(
        TEXT("{\"type\":\"trial\",\"sessionId\":\"%s\",\"mode\":\"%s\",\"block\":%d,\"trial\":%d,")
        TEXT("\"stimulus\":\"%s\",\"seed\":%d,\"seqHash\":\"%llu\",")
        TEXT("\"scheduledStimulusOnsetSec\":%.6f,\"realizedStimulusOnsetSec\":%.6f,")
        TEXT("\"scheduledStimulusOffsetSec\":%.6f,\"realizedStimulusOffsetSec\":%.6f,")
        TEXT("\"responseWindowDeadlineSec\":%.6f,\"responseTimestampSec\":%.6f,")
        TEXT("\"behavioralResolutionTimestampSec\":%.6f,\"trialCompleteTimestampSec\":%.6f,")
        TEXT("\"hadResponse\":%s,\"rtMs\":%.2f,\"outcome\":\"%s\",")
        TEXT("\"visualTerminatedByResponse\":%s,\"responseAcceptedAfterVisualOffset\":%s,")
        TEXT("\"responseWindowMs\":%.1f,\"scheduledItiMs\":%.1f,\"realizedItiMs\":%.1f,")
        TEXT("\"valid\":%s,\"voidReason\":\"%s\",\"gsr\":%s}"),
        *EscapeJson(R.SessionID), ModeToStr(R.OperatingMode), R.BlockIndex, R.TrialIndex,
        StimulusToStr(R.StimulusType), R.ScheduleSeed, static_cast<unsigned long long>(R.SequenceHash),
        R.ScheduledStimulusOnsetSec, R.RealizedStimulusOnsetSec,
        R.ScheduledStimulusOffsetSec, R.RealizedStimulusOffsetSec,
        R.ResponseWindowDeadlineSec, R.ResponseTimestampSec,
        R.BehavioralResolutionTimestampSec, R.TrialCompleteTimestampSec,
        BoolStr(R.bHadResponse), R.RTMs, OutcomeToStr(R.Outcome),
        BoolStr(R.bVisualTerminatedByResponse), BoolStr(R.bResponseAcceptedAfterVisualOffset),
        R.ResponseWindowMs, R.ScheduledITIMs, R.RealizedITIMs,
        BoolStr(R.bValid), *EscapeJson(R.VoidReason), *GsrJson);

    const bool bPersisted = AppendLine(Line);

    // Compute realized visual exposure for the mirror (guard against not-yet-hidden).
    const double VisibleMs = (R.RealizedStimulusOffsetSec >= 0.0)
        ? (R.RealizedStimulusOffsetSec - R.RealizedStimulusOnsetSec) * 1000.0
        : -1.0;

    UE_LOG(LogTMResearch, Warning,
        TEXT("[Research][Trial] B%d T%02d | %-4s -> %-16s | RT=%7.1fms | visible=%.0fms | afterOffset=%s | win=%.0fms | iti(sched/real)=%.0f/%.0f | valid=%s | persisted=%s"),
        R.BlockIndex, R.TrialIndex, StimulusToStr(R.StimulusType), OutcomeToStr(R.Outcome),
        R.RTMs, VisibleMs, BoolStr(R.bResponseAcceptedAfterVisualOffset), R.ResponseWindowMs,
        R.ScheduledITIMs, R.RealizedITIMs, BoolStr(R.bValid), BoolStr(bPersisted));
}

void FTMTrialLogger::LogBlockSummary(int32 BlockIndex, int32 GoCount, int32 NoGoCount, int32 Hits,
                                     int32 Omissions, int32 CorrectRejections, int32 Commissions, int32 Voids,
                                     int32 ScheduleSeed, uint64 SequenceHash,
                                     const FString& TerminationReason)
{
    const bool bNormalCompletion = TerminationReason.IsEmpty();
    const FString Line = FString::Printf(
        TEXT("{\"type\":\"block_summary\",\"sessionId\":\"%s\",\"block\":%d,\"goCount\":%d,\"noGoCount\":%d,")
        TEXT("\"hits\":%d,\"omissions\":%d,\"correctRejections\":%d,\"commissions\":%d,\"voids\":%d,")
        TEXT("\"seed\":%d,\"seqHash\":\"%llu\",\"completion\":\"%s\",\"terminationReason\":\"%s\"}"),
        *EscapeJson(SessionID), BlockIndex, GoCount, NoGoCount,
        Hits, Omissions, CorrectRejections, Commissions, Voids,
        ScheduleSeed, static_cast<unsigned long long>(SequenceHash),
        bNormalCompletion ? TEXT("normal") : TEXT("interrupted"), *EscapeJson(TerminationReason));

    const bool bPersisted = AppendLine(Line);

    UE_LOG(LogTMResearch, Warning,
        TEXT("[Research][BlockSummary] B%d %s | reason=%s | GO=%d NOGO=%d | Hit=%d Omit=%d CR=%d Comm=%d Void=%d | seed=%d hash=%llu | summaryPersisted=%s"),
        BlockIndex, bNormalCompletion ? TEXT("BEHAVIORAL-COMPLETE") : TEXT("INTERRUPTED"),
        bNormalCompletion ? TEXT("<none>") : *TerminationReason,
        GoCount, NoGoCount, Hits, Omissions, CorrectRejections, Commissions, Voids,
        ScheduleSeed, static_cast<unsigned long long>(SequenceHash), BoolStr(bPersisted));
}

void FTMTrialLogger::Finalize(const FString& TerminationReason)
{
    if (bFinalized)
    {
        return;
    }
    bFinalized = true;

    const bool bNormalCompletion = TerminationReason.IsEmpty();
    if (bOpen)
    {
        const FString Line = FString::Printf(
            TEXT("{\"type\":\"session_footer\",\"sessionId\":\"%s\",\"recordsWritten\":%d,\"writeFailures\":%d,")
            TEXT("\"healthy\":%s,\"completion\":\"%s\",\"terminationReason\":\"%s\",\"endedUtc\":\"%s\"}"),
            *EscapeJson(SessionID), RecordsWritten, WriteFailureCount, BoolStr(IsHealthy()),
            bNormalCompletion ? TEXT("normal") : TEXT("interrupted"), *EscapeJson(TerminationReason),
            *FDateTime::UtcNow().ToIso8601());
        const bool bFooterWrote = FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *FilePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
        if (!bFooterWrote)
        {
            ++WriteFailureCount;
            bHealthy = false;
            UE_LOG(LogTMResearch, Error,
                TEXT("[Research][Logger] FOOTER WRITE FAILED (#%d) | session=%s | path=%s | logger now UNHEALTHY."),
                WriteFailureCount, *SessionID, *FilePath);
        }
    }

    if (IsHealthy())
    {
        UE_LOG(LogTMResearch, Warning,
            TEXT("[Research][Logger] CLOSED healthy | completion=%s | reason=%s | session=%s | records=%d | failures=%d | path=%s"),
            bNormalCompletion ? TEXT("normal") : TEXT("interrupted"),
            bNormalCompletion ? TEXT("<none>") : *TerminationReason,
            *SessionID, RecordsWritten, WriteFailureCount, *FilePath);
    }
    else
    {
        UE_LOG(LogTMResearch, Error,
            TEXT("[Research][Logger] CLOSED UNHEALTHY | completion=%s | reason=%s | session=%s | records=%d | failures=%d | open=%s | path=%s | persistence is NOT reliable."),
            bNormalCompletion ? TEXT("normal") : TEXT("interrupted"),
            bNormalCompletion ? TEXT("<none>") : *TerminationReason,
            *SessionID, RecordsWritten, WriteFailureCount, BoolStr(bOpen), *FilePath);
    }
}
