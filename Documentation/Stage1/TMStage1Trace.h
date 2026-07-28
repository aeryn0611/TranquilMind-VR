// TMStage1Trace.h
// TranquilMind Stage 1 instrumentation — LOG ONLY.
//
// Implements TranquilMind_Implementation_Spec.md §11.
//
// HARD CONSTRAINTS (Stage1_Instrumentation_Kit.md §0):
//   N1  Never calls CreateDynamicMaterialInstance.
//   N2  Never calls any setter. Read accessors only.
//   N3  Never touches a random stream.
//
// This file adds no behaviour. If any function here mutates state, it is a bug
// and Stage 1 is invalid.
//
// API-VERSION NOTE: accessor names below target UE 5.x. Verify against your
// engine version before building — in particular K2_GetScalarParameterValue,
// GetVisibleFlag, and bHiddenInGame. If a name differs, change the accessor,
// never the semantics.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

class AActor;
class UMeshComponent;
class USceneComponent;
class UWorld;
enum class EEndPlayReason : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogTMStage1, Log, All);

/**
 * Static, stateless-except-for-diagnostics tracer.
 *
 * The only retained state is diagnostic bookkeeping (cycle counter, SNAP window
 * bounds, last-seen identity map). None of it is read by gameplay.
 */
struct FTMStage1Trace
{
	// ---- configuration -----------------------------------------------------

	/** Frames to capture before EXIT_BEGIN and after ENDPLAY. Kit §3.1. */
	static constexpr int32 SnapLeadFrames  = 5;
	static constexpr int32 SnapTrailFrames = 5;

	/**
	 * Exit-controlled scalar parameter names (Kit §1, slot P11).
	 * POPULATE FROM THE PROJECT. Leaving this empty means SNAP records no
	 * scalars and §6.1 cannot be answered.
	 */
	static TArray<FName>& ExitScalarParams();

	/**
	 * Approach axis in world space, used for apdist. Default is +X.
	 * Set to match the project's approach convention.
	 */
	static FVector ApproachAxis();

	/**
	 * Class of Demo target presentations, used for visiblepresentations.
	 * POPULATE FROM THE PROJECT (slot P1). If null, visiblepresentations
	 * reports -1 and §6.3 cannot be answered.
	 */
	static UClass*& TargetPresentationClass();

	/**
	 * Optional hook returning the live ActiveTargets count. If unset,
	 * activetargets reports -1.
	 */
	static TFunction<int32(UWorld*)>& ActiveTargetsProvider();

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
	 * Call unconditionally from the target actor's Tick.
	 * No-ops outside the SNAP window, so it is safe to leave in place.
	 */
	static void TickSnapshot(AActor* Actor, UMeshComponent* Mesh, USceneComponent* VisualMotion);

private:
	static uint64 CurrentFrame();
	static FString Header(AActor* Actor, const TCHAR* Event);
	static FString XformToStr(const FTransform& T);
	static float   ApproachDistanceCm(AActor* Actor);
	static int32   CountVisiblePresentations(UWorld* World);
	static int32   QueryActiveTargets(UWorld* World);

	/** Diagnostic bookkeeping only. Never read by gameplay. */
	static int32  CycleIndex;
	static uint64 SnapWindowStart;
	static uint64 SnapWindowEnd;
	static bool   bExit70Logged;
};
