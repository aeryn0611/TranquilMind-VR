// TranquilMind Research — per-trial structured logger.
//
// Writes one JSON object per line (JSONL) to:
//   <ProjectSaved>/TranquilMind/Research/<SessionID>.jsonl
// and mirrors a compact summary to UE_LOG (so Quest logcat shows every trial).
//
// No personal identifiers, no network upload. GSR fields are the CURRENT 1 Hz
// latest-sample values only and are explicitly NOT synchronized scientific EDA.

#pragma once

#include "CoreMinimal.h"
#include "../TranquilMindTypes.h"

/**
 * One fully-resolved research trial. All *Sec times are monotonic (FPlatformTime::Seconds);
 * only deltas are meaningful. Scheduled and realized timestamps are kept SEPARATE and a
 * scheduled value is never reported as realized.
 */
struct FTMTrialRecord
{
    FString SessionID;
    ETMOperatingMode OperatingMode = ETMOperatingMode::Research;

    int32 BlockIndex = 0;
    int32 TrialIndex = 0;                 // 0-based within block
    ETMStimulusType StimulusType = ETMStimulusType::Go;

    int32 ScheduleSeed = 0;
    uint64 SequenceHash = 0;

    // --- Onset ---
    double ScheduledStimulusOnsetSec = 0.0;   // planned onset (from the ITI schedule)
    double RealizedStimulusOnsetSec = 0.0;    // when the bubble actually appeared

    // --- Visual offset (fixed exposure) ---
    double ScheduledStimulusOffsetSec = 0.0;  // realized onset + StimulusVisibleMs
    double RealizedStimulusOffsetSec = -1.0;  // when the bubble was actually hidden (-1 until hidden)

    // --- Response window ---
    double ResponseWindowDeadlineSec = 0.0;   // realized onset + ResponseWindowMs
    double ResponseTimestampSec = -1.0;       // when the response arrived (-1 if none)
    bool   bHadResponse = false;
    double BehavioralResolutionTimestampSec = 0.0; // when outcome decided (response ts OR deadline)
    double TrialCompleteTimestampSec = 0.0;   // when ITI started = max(realized offset, behavioral resolution)

    float RTMs = -1.0f;                    // response - realized onset, ms; -1 if no response
    ETMTrialOutcome Outcome = ETMTrialOutcome::Unresolved;

    // Fixed-exposure guarantees (Research Mode):
    bool bVisualTerminatedByResponse = false;   // ALWAYS false: a response never shortens the 400ms
    bool bResponseAcceptedAfterVisualOffset = false; // true if the accepted response arrived after offset

    float ResponseWindowMs = 0.0f;         // authoritative window in effect
    float ScheduledITIMs = 0.0f;           // ITI scheduled AFTER this trial
    float RealizedITIMs = -1.0f;

    bool bValid = true;                    // false => Void
    FString VoidReason;                    // hard-gate / abort reason if invalidated

    // Current 1 Hz GSR latest sample (contextual only; NOT synchronized scientific EDA).
    bool bGSRAvailable = false;
    int32 GSR_Raw = 0;
    int32 GSR_Smoothed = 0;
    int32 GSR_Quality = 0;
    int64 GSR_DeviceTimestampMs = 0;
};

class TRANQUILMIND_API FTMTrialLogger
{
public:
    /** Opens (creates) the JSONL file and writes a header. Returns true on success. */
    bool BeginSession(const FString& InSessionID);

    /** Appends one trial record as a JSON line and mirrors a summary to UE_LOG. */
    void LogTrial(const FTMTrialRecord& Record);

    /** Writes a block-summary line (counts) for convenience. */
    void LogBlockSummary(int32 BlockIndex, int32 GoCount, int32 NoGoCount, int32 Hits,
                         int32 Omissions, int32 CorrectRejections, int32 Commissions, int32 Voids,
                         int32 ScheduleSeed, uint64 SequenceHash,
                         const FString& TerminationReason = FString());

    /** Writes one final status line and marks closed. Empty reason means normal completion. */
    void Finalize(const FString& TerminationReason = FString());

    const FString& GetFilePath() const { return FilePath; }
    bool IsOpen() const { return bOpen; }

    /** True only if the file opened AND every append so far succeeded. */
    bool IsHealthy() const { return bOpen && bHealthy; }
    int32 GetWriteFailureCount() const { return WriteFailureCount; }
    int32 GetRecordsWritten() const { return RecordsWritten; }

private:
    /** Appends a line; returns true on success. Updates health state on failure. */
    bool AppendLine(const FString& Line);
    static FString EscapeJson(const FString& In);

    FString FilePath;
    FString SessionID;
    bool bOpen = false;         // file successfully created
    bool bHealthy = false;      // file open AND no write has failed
    int32 WriteFailureCount = 0;
    int32 RecordsWritten = 0;   // trial + summary lines successfully appended
    bool bFinalized = false;
};
