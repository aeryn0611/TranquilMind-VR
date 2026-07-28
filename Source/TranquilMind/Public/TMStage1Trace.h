// TMStage1Trace.h
// TranquilMind Stage 1 instrumentation — LOG ONLY.
//
// Implements TranquilMind_Implementation_Spec.md §11 via
// Documentation/Stage1/Stage1_Instrumentation_Kit.md.
//
// HARD CONSTRAINTS (Kit §0):
//   N1  Never calls CreateDynamicMaterialInstance.
//   N2  Never calls any setter on a component, actor, material or transform.
//   N3  Never touches a random stream.
//
// This file adds no behaviour. If any function here mutates gameplay state, it
// is a bug and Stage 1 is invalid.
//
// PROJECT DEVIATIONS FROM THE SUPPLIED KIT (each recorded, none semantic):
//
//  D1  EEndPlayReason is a namespaced enum in UE, not `enum class`. The kit's
//      forward declaration does not compile; Engine/EngineTypes.h is included
//      instead.
//
//  D2  The kit opens the SNAP window retroactively at EXIT_BEGIN − 5 frames,
//      which cannot capture frames that have already elapsed. The window is
//      opened at SPAWN instead, so the pre-exit approach and the exit window
//      are both recorded and Kit §6.2's "before vs after" comparison has real
//      lead frames. Strictly more evidence; no field or event changes.
//
//  D3  ApproachAxis is per-instance in this project (the target's
//      TravelDirection_World, resolved from the Demo session frame), not a
//      fixed world axis. It is published here by the target actor so
//      apdist_proj is measured against the axis actually travelled.
//
//  D4  LogNextSpawn's prev_auid is served from the tracer's own record of the
//      last SPAWN, so the spawner needs no new gameplay-visible state.
//
//  D5  Transform payloads separate their L/R/S groups with ';' rather than the
//      kit's '|', which is parse_tm_trace.py's own field separator. See
//      XformToStr in the .cpp. Format only; no field or event changes.
//
//  D6  The kit keeps cyc / the SNAP window / the EXIT_70 latch in single global
//      statics. Measured in the first headless capture: presentations OVERLAP
//      (visiblepresentations reached 2 for 26 frames), so the next SPAWN bumps
//      the global counter while the previous actor is still exiting, and that
//      actor's remaining exit frames are emitted under the SUCCESSOR's cycle
//      number. In that capture cycle 4's entire exit was relabelled cycle 5 and
//      disappeared from its own ledger. Because the defect is triggered by the
//      exact condition Stage 1 exists to measure, it would silently corrupt the
//      authoritative capture. This state is therefore keyed per actor.
//
// ---------------------------------------------------------------------------
// SCHEMA 2 (Pre-Stage-2 instrumentation hardening)
// ---------------------------------------------------------------------------
// Schema 1 carried two identity defects, both confirmed against real Stage 1
// evidence:
//
//  H1  Mixed sessions. A single editor process writes one log across many PIE
//      sessions. Schema 1 had no session marker, so a whole-log parse silently
//      merged four PIE runs (Stage 1 report §"authoritative capture" had to be
//      isolated by hand from log line 15024).
//
//  H2  UObject UniqueID recycling. auid and miduid are object-table indices and
//      are reused after GC. In the authoritative PIE capture 40 spawns produced
//      only 39 distinct auid and 38 distinct miduid, and one miduid appeared
//      under two auid — which the schema-1 rules would have reported as
//      "MID SHARED ACROSS ACTORS — serious". It was recycling: the lifetimes do
//      not overlap.
//
// Schema 2 fixes both by carrying a stable, non-UObject identity on every line:
//
//      sid       unique per world/session, stable for that world's lifetime,
//                never reused within one editor process
//      spawnseq  monotonic within sid, assigned exactly once at presentation
//                creation, never derived from a UObject UniqueID
//
//      CANONICAL ACTOR IDENTITY = (sid, spawnseq)
//
// auid, miduid and the actor name remain as DIAGNOSTIC FIELDS ONLY and must
// never be treated as globally unique.
//
// sid and spawnseq come from plain diagnostic counters. No RNG of any kind is
// used, so R5 and R7 cannot be perturbed.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"        // D1: EEndPlayReason::Type
#include "Logging/LogMacros.h"

class AActor;
class UMeshComponent;
class USceneComponent;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogTMStage1, Log, All);

/**
 * Static, stateless-except-for-diagnostics tracer.
 *
 * The only retained state is diagnostic bookkeeping (cycle counter, SNAP window
 * bounds, last-spawn identity, approach axis). None of it is read by gameplay.
 */
struct TRANQUILMIND_API FTMStage1Trace
{
	// ---- configuration -----------------------------------------------------

	/** Frames to keep the SNAP window open after ENDPLAY. Kit §3.1. */
	static constexpr int32 SnapTrailFrames = 5;

	/**
	 * Exit-controlled scalar parameter names (Kit §1, slot P11).
	 * Resolved from the project: HandleVisualExitFade writes the first three;
	 * FresnelExponent is included as a Group-A control that the exit must NOT
	 * be touching, so the log proves it stays put.
	 */
	static const TArray<FName>& ExitScalarParams();

	/** D3: approach axis in world space, published by the target actor. */
	static FVector GetApproachAxis();
	static void    SetApproachAxis(const FVector& InAxis);

	/** Class of Demo target presentations (slot P1 base class), used for
	 *  visiblepresentations. If null, visiblepresentations reports -1. */
	static UClass* GetTargetPresentationClass();
	static void    SetTargetPresentationClass(UClass* InClass);

	/** Live ActiveTargets count hook. If unset, activetargets reports -1. */
	static void SetActiveTargetsProvider(TFunction<int32(UWorld*)> InProvider);
	static void ClearActiveTargetsProvider();

	/**
	 * STAGE 2 (log-only). Official LiveDemoPresentation count, 0 or 1. If unset,
	 * livepresentation reports -1.
	 *
	 * visiblepresentations is a world scan and answers "how many target actors
	 * are currently rendering", which is what detected C7. This answers the
	 * different question Spec §11.1 S2-A actually asks — "how many OFFICIAL Demo
	 * presentations does the spawner own" — so the ownership invariant is
	 * measured directly instead of inferred from the world scan.
	 */
	static void SetLivePresentationProvider(TFunction<int32(UWorld*)> InProvider);
	static void ClearLivePresentationProvider();

	/** D4: UniqueID of the most recent SPAWN, for NEXT_SPAWN's prev_auid. */
	static uint32 LastSpawnedAuid();

	// ---- session boundaries (schema 2) -------------------------------------

	/**
	 * Open a trace session for this world. Called from the spawner's BeginPlay,
	 * which is guaranteed to precede any target presentation in that world.
	 * Allocates a fresh sid from a diagnostic counter (never an RNG) and resets
	 * spawnseq. Idempotent for a world already open.
	 *
	 * @param Mode  "Demo" / "Research" / "Unknown" — supplied by the caller so
	 *              the tracer never reads settings or protected state itself.
	 */
	static void LogSessionBegin(UWorld* World, const TCHAR* Mode);

	/** Close the current session, recording the teardown reason when known. */
	static void LogSessionEnd(UWorld* World, int32 Reason, const TCHAR* ReasonText);

	// ---- event logging -----------------------------------------------------

	static void LogSpawn(AActor* Actor, UMeshComponent* Mesh);
	static void LogMidCreate(AActor* Actor, UMeshComponent* Mesh, const TCHAR* CallSite);
	static void LogPhase(AActor* Actor, const TCHAR* From, const TCHAR* To);
	static void LogOutcome(AActor* Actor, const TCHAR* Outcome, float RtMs);
	static void LogExitBegin(AActor* Actor);
	static void LogExit70(AActor* Actor, float ElapsedMs);
	static void LogMatWrite(AActor* Actor, FName Param, float Value);
	static void LogVisFalse(AActor* Actor, const TCHAR* CallSite);
	static void LogHiddenTrue(AActor* Actor, const TCHAR* CallSite);
	static void LogProgress(AActor* Actor, float Alpha);
	static void LogComplete(AActor* Actor);
	static void LogDestroyReq(AActor* Actor, const TCHAR* CallSite, const TCHAR* Mode);
	static void LogEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	static void LogNextSpawn(UWorld* World, uint32 PrevActorUid);

	/**
	 * Per-frame state record. No-ops outside the SNAP window, so it is safe to
	 * leave in place.
	 *
	 * Called from UTMVisualMotionComponent::TickComponent, NOT from the actor
	 * Tick: FinishResolution() calls SetActorTickEnabled(false) before the exit
	 * begins, so the actor does not tick during the window this must observe.
	 * The component tick is unaffected by SetActorTickEnabled and runs for the
	 * whole exit.
	 */
	static void TickSnapshot(AActor* Actor, UMeshComponent* Mesh, USceneComponent* VisualMotion);

private:
	static FString Header(AActor* Actor, const TCHAR* Event);
	static FString XformToStr(const FTransform& T);
	static int32   CountVisiblePresentations(UWorld* World);
	static int32   QueryActiveTargets(UWorld* World);
	static int32   QueryLivePresentation(UWorld* World);

	static uint64  CurrentFrame();
	static void    EnsureSession(UWorld* World);

	/** D6 + schema 2: per-actor trace state. Overlapping presentations cannot
	 *  steal each other's cycle number, EXIT_70 latch, sid or spawnseq. */
	struct FActorTrace
	{
		int32  Cycle          = 0;
		int32  SpawnSeq       = -1;   // canonical identity, with Sid
		int32  Sid            = -1;
		bool   bExit70Logged  = false;
		uint64 SnapWindowEnd  = TNumericLimits<uint64>::Max();
	};

	/** Diagnostic bookkeeping only. Never read by gameplay, never from an RNG. */
	static TMap<uint32, FActorTrace> ActorTraces;
	static int32   CycleCounter;
	static uint32  LastSpawnAuid;

	/** Session state (schema 2). SessionCounter never resets within a process,
	 *  so a sid is never reused; SpawnSeqCounter resets per session. */
	static int32   SessionCounter;
	static int32   CurrentSid;
	static int32   SpawnSeqCounter;
	static int32   SessionSpawnCount;
	static TWeakObjectPtr<UWorld> CurrentWorld;
	static FVector ApproachAxisWorld;
	static UClass* TargetPresentationClassPtr;
	static TFunction<int32(UWorld*)> ActiveTargetsProviderFn;
	static TFunction<int32(UWorld*)> LivePresentationProviderFn;
};
