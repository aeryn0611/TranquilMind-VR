// TranquilMind Research Mode — discrete-flashed trial state machine (Phase 1).
//
// Owned and ticked by UTargetSpawnerComponent only when OperatingMode == Research.
// Drives its OWN timing via a monotonic clock (FPlatformTime::Seconds); it does NOT
// use the moving-target actor's tick-based expiry as a timing authority.
//
// Per trial:
//   ITI (jittered) -> stimulus appears at ONE fixed location -> visible 400ms ->
//   hidden -> response window stays open until 1200ms from onset -> resolve -> ITI...
//
// Scoring: only the current active trial is resolved (never resolve-all). Adaptation
// is NOT implemented in Phase 1 (fixed response window). GSR never affects behavior.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../TranquilMindTypes.h"
#include "TMResearchConfig.h"
#include "TMSequenceGenerator.h"
#include "TMTrialLogger.h"
#include "TMResearchRunner.generated.h"

class UTargetSpawnerComponent;
class ATranquilMindSessionManager;
class ATranquilMindTargetActor;
class UWorld;

UENUM()
enum class ETMResearchState : uint8
{
    Idle,
    InterTrialInterval,
    // One active state. Within it, the VISUAL and the BEHAVIORAL response are tracked
    // independently: the bubble is shown for a FIXED StimulusVisibleMs (a response never
    // shortens it), while the response window stays open until ResponseWindowMs. The trial
    // completes at max(realized visual offset, behavioral resolution) and only then does ITI begin.
    TrialActive,
    BlockComplete
};

UCLASS()
class TRANQUILMIND_API UTMResearchRunner : public UObject
{
    GENERATED_BODY()

public:
    /** One-time setup. Generates the block schedule (fail-loud) and opens the trial log. */
    bool Initialize(UTargetSpawnerComponent* InOwner,
                    ATranquilMindSessionManager* InSessionManager,
                    const FTMResearchConfig& InConfig,
                    int32 InScheduleSeed,
                    const FString& InSessionID);

    /** Advance the state machine. Call every frame from the owner's tick. */
    void Tick(float DeltaTime);

    /** A response (right-trigger) occurred. Resolves ONLY the current active trial. */
    void HandleResponse();

    /** Void the current trial and stop (e.g., SysAbort). Safe to call anytime. */
    void AbortCurrentTrialAsVoid(const FString& Reason);

    /**
     * Stop the Research run and finalize persistence exactly once.
     * An active started trial is written once as Void; a completed pending trial is
     * preserved. Safe for EndPlay and repeated shutdown calls.
     */
    void ShutdownResearchRun(const FString& Reason);

    bool IsBlockComplete() const { return State == ETMResearchState::BlockComplete; }
    ETMResearchState GetState() const { return State; }

private:
    friend class FTMResearchLifecycleTest;

    void BeginTrial();
    void HideStimulusVisual();             // hide only (fixed-exposure offset); does not destroy
    void CompleteTrial();                  // both visual offset done AND behavior resolved
    void DestroyStimulusActor();
    void FinishBlock(const FString& FinalizationReason = FString());

    double NowSec() const;                 // monotonic
    float DrawITIms();                     // seeded, deterministic
    FVector ComputeFixedStimulusLocation();
    void FillGSR(FTMTrialRecord& Record) const;
    void FlushPending(double RealizedNextOnsetSec);

private:
    UPROPERTY(Transient) TObjectPtr<UTargetSpawnerComponent> Owner = nullptr;
    UPROPERTY(Transient) TObjectPtr<ATranquilMindSessionManager> SessionManager = nullptr;
    UPROPERTY(Transient) TObjectPtr<ATranquilMindTargetActor> ActiveStimulus = nullptr;

    FTMResearchConfig Config;
    int32 ScheduleSeed = 0;
    FString SessionID;

    FTMSequenceResult Schedule;
    bool bScheduleValid = false;
    bool bRunFinalized = false;

    ETMResearchState State = ETMResearchState::Idle;
    int32 BlockIndex = 0;
    int32 TrialIndex = -1;                 // index of the CURRENT/last trial

    // ---- Per-trial timing (monotonic seconds) ----
    double NextOnsetSec = 0.0;             // scheduled onset of the next stimulus
    double CurScheduledOnsetSec = 0.0;     // scheduled onset of the CURRENT trial
    double CurRealizedOnsetSec = 0.0;      // when the bubble actually appeared
    double CurScheduledOffsetSec = 0.0;    // realized onset + StimulusVisibleMs
    double CurRealizedOffsetSec = -1.0;    // when the bubble was actually hidden (-1 until)
    double CurDeadlineSec = 0.0;           // realized onset + ResponseWindowMs
    double LastCompleteSec = 0.0;          // trial-complete time (ITI measured from here)
    float  ScheduledITIms_AfterCurrent = 0.0f;

    // ---- Per-trial visual/behavioral decoupling ----
    bool   bVisualHidden = false;          // fixed-exposure offset has occurred
    bool   bBehaviorResolved = false;      // outcome decided (by response or deadline)
    bool   bTrialCompleted = false;        // ITI has been scheduled for this trial
    ETMTrialOutcome CurOutcome = ETMTrialOutcome::Unresolved;
    double CurResponseTsSec = -1.0;
    double CurBehavioralResolutionSec = 0.0;
    float  CurRTMs = -1.0f;
    bool   bCurResponseAfterOffset = false;

    FVector FixedStimulusLocation = FVector::ZeroVector;
    bool bFixedLocationComputed = false;

    // Deferred record so realized ITI (measured at the NEXT onset) can be recorded.
    FTMTrialRecord PendingRecord;
    bool bHasPending = false;

    // Block tallies.
    int32 N_Hit = 0, N_Omission = 0, N_CorrectRejection = 0, N_Commission = 0, N_Void = 0;
    int32 DuplicateResponseCount = 0;

    FRandomStream ITIStream;
    FTMTrialLogger Logger;
};
