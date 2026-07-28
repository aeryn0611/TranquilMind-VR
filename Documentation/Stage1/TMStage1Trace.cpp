// TMStage1Trace.cpp
// TranquilMind Stage 1 instrumentation — LOG ONLY. See TMStage1Trace.h.
//
// Verify accessor names against your UE version before building.
// Change accessors if needed; never change semantics.

#include "TMStage1Trace.h"

#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"                 // TActorIterator
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTMStage1);

int32  FTMStage1Trace::CycleIndex      = 0;
uint64 FTMStage1Trace::SnapWindowStart = TNumericLimits<uint64>::Max();
uint64 FTMStage1Trace::SnapWindowEnd   = 0;
bool   FTMStage1Trace::bExit70Logged   = false;

// ---------------------------------------------------------------- config ----

TArray<FName>& FTMStage1Trace::ExitScalarParams()
{
	// POPULATE FROM THE PROJECT (Kit §1, slot P11).
	// Example only — replace with the real parameter names:
	//   TEXT("Opacity"), TEXT("RimWidth"), TEXT("RimIntensity"), TEXT("ScaleMul")
	static TArray<FName> Params;
	return Params;
}

FVector FTMStage1Trace::ApproachAxis()
{
	// Adjust to the project's approach convention.
	return FVector::ForwardVector;
}

UClass*& FTMStage1Trace::TargetPresentationClass()
{
	// POPULATE FROM THE PROJECT (Kit §1, slot P1).
	static UClass* Cls = nullptr;
	return Cls;
}

TFunction<int32(UWorld*)>& FTMStage1Trace::ActiveTargetsProvider()
{
	static TFunction<int32(UWorld*)> Provider;
	return Provider;
}

// --------------------------------------------------------------- helpers ----

uint64 FTMStage1Trace::CurrentFrame()
{
	return GFrameCounter;
}

FString FTMStage1Trace::XformToStr(const FTransform& T)
{
	const FVector  L = T.GetLocation();
	const FRotator R = T.Rotator();
	const FVector  S = T.GetScale3D();
	return FString::Printf(
		TEXT("[L=%.3f,%.3f,%.3f|R=%.3f,%.3f,%.3f|S=%.4f,%.4f,%.4f]"),
		L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll, S.X, S.Y, S.Z);
}

FString FTMStage1Trace::Header(AActor* Actor, const TCHAR* Event)
{
	const UWorld* W  = Actor ? Actor->GetWorld() : nullptr;
	const float   Rt = W ? W->GetRealTimeSeconds()  : -1.f;
	const float   Wt = W ? W->GetTimeSeconds()      : -1.f;

	return FString::Printf(
		TEXT("TMS1|f=%llu|rt=%.6f|wt=%.6f|ev=%s|aname=%s|auid=%u|cyc=%d"),
		CurrentFrame(), Rt, Wt, Event,
		Actor ? *Actor->GetName() : TEXT("null"),
		Actor ? Actor->GetUniqueID() : 0u,
		CycleIndex);
}

float FTMStage1Trace::ApproachDistanceCm(AActor* Actor)
{
	if (!Actor) { return -1.f; }

	const UWorld* W = Actor->GetWorld();
	if (!W) { return -1.f; }

	// Read-only: viewpoint of the local player.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(Actor, 0);
	if (!PC) { return -1.f; }

	FVector  ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector Delta = Actor->GetActorLocation() - ViewLoc;
	// Straight-line distance plus the projection onto the approach axis, so
	// §6.2 can distinguish depth change from lateral drift.
	const float Straight  = Delta.Size();
	const float Projected = FVector::DotProduct(Delta, ApproachAxis().GetSafeNormal());
	// Return straight-line; projection is emitted separately in SNAP.
	(void)Projected;
	return Straight;
}

int32 FTMStage1Trace::CountVisiblePresentations(UWorld* World)
{
	UClass* Cls = TargetPresentationClass();
	if (!World || !Cls) { return -1; }

	int32 Count = 0;
	for (TActorIterator<AActor> It(World, Cls); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->IsActorBeingDestroyed()) { continue; }

		bool bAnyVisibleMesh = false;
		TArray<UMeshComponent*> Meshes;
		A->GetComponents<UMeshComponent>(Meshes);
		for (const UMeshComponent* M : Meshes)
		{
			if (M && M->IsVisible() && !M->bHiddenInGame)
			{
				bAnyVisibleMesh = true;
				break;
			}
		}
		if (bAnyVisibleMesh && !A->IsHidden()) { ++Count; }
	}
	return Count;
}

int32 FTMStage1Trace::QueryActiveTargets(UWorld* World)
{
	const TFunction<int32(UWorld*)>& P = ActiveTargetsProvider();
	return P ? P(World) : -1;
}

// ------------------------------------------------------------ events --------

void FTMStage1Trace::LogSpawn(AActor* Actor, UMeshComponent* Mesh)
{
	++CycleIndex;
	bExit70Logged   = false;
	SnapWindowStart = TNumericLimits<uint64>::Max();
	SnapWindowEnd   = 0;

	UMaterialInterface*       MI  = Mesh ? Mesh->GetMaterial(0) : nullptr;
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MI);   // N1: read, never create

	UE_LOG(LogTMStage1, Log,
		TEXT("%s|matiface=%s|midvalid=%d|miduid=%d|midcreated=0"),
		*Header(Actor, TEXT("SPAWN")),
		MI ? *MI->GetName() : TEXT("null"),
		MID ? 1 : 0,
		MID ? static_cast<int32>(MID->GetUniqueID()) : -1);
}

void FTMStage1Trace::LogMidCreate(AActor* Actor, UMeshComponent* Mesh, const TCHAR* CallSite)
{
	UMaterialInstanceDynamic* MID =
		Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;

	UE_LOG(LogTMStage1, Log, TEXT("%s|miduid=%d|caller=%s"),
		*Header(Actor, TEXT("MID_CREATE")),
		MID ? static_cast<int32>(MID->GetUniqueID()) : -1,
		CallSite);
}

void FTMStage1Trace::LogPhase(AActor* Actor, const TCHAR* From, const TCHAR* To)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|from=%s|to=%s"),
		*Header(Actor, TEXT("PHASE")), From, To);
}

void FTMStage1Trace::LogOutcome(AActor* Actor, const TCHAR* Outcome, float RtMs)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|outcome=%s|rt_ms=%.3f"),
		*Header(Actor, TEXT("OUTCOME")), Outcome, RtMs);
}

void FTMStage1Trace::LogExitBegin(AActor* Actor)
{
	const uint64 F = CurrentFrame();
	SnapWindowStart = (F > static_cast<uint64>(SnapLeadFrames)) ? F - SnapLeadFrames : 0;
	SnapWindowEnd   = TNumericLimits<uint64>::Max();   // closed by LogEndPlay
	bExit70Logged   = false;

	UE_LOG(LogTMStage1, Log, TEXT("%s"), *Header(Actor, TEXT("EXIT_BEGIN")));
}

void FTMStage1Trace::LogExit70(AActor* Actor, float ElapsedMs)
{
	if (bExit70Logged) { return; }
	bExit70Logged = true;

	UE_LOG(LogTMStage1, Log, TEXT("%s|elapsed_ms=%.3f"),
		*Header(Actor, TEXT("EXIT_70")), ElapsedMs);
}

void FTMStage1Trace::LogMatWrite(AActor* Actor, FName Param, float Value)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|param=%s|value=%.6f"),
		*Header(Actor, TEXT("MAT_WRITE")), *Param.ToString(), Value);
}

void FTMStage1Trace::LogVisFalse(AActor* Actor, const TCHAR* CallSite)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|caller=%s"),
		*Header(Actor, TEXT("VIS_FALSE")), CallSite);
}

void FTMStage1Trace::LogHiddenTrue(AActor* Actor, const TCHAR* CallSite)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|caller=%s"),
		*Header(Actor, TEXT("HIDDEN_TRUE")), CallSite);
}

void FTMStage1Trace::LogProgress(AActor* Actor, float Alpha)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|alpha=%.6f"),
		*Header(Actor, TEXT("DELEGATE_PROGRESS")), Alpha);
}

void FTMStage1Trace::LogComplete(AActor* Actor)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s"), *Header(Actor, TEXT("DELEGATE_COMPLETE")));
}

void FTMStage1Trace::LogDestroyReq(AActor* Actor, const TCHAR* CallSite, const TCHAR* Mode)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|caller=%s|mode=%s"),
		*Header(Actor, TEXT("DESTROY_REQ")), CallSite, Mode);
}

void FTMStage1Trace::LogEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	UE_LOG(LogTMStage1, Log, TEXT("%s|reason=%d"),
		*Header(Actor, TEXT("ENDPLAY")), static_cast<int32>(Reason));

	// Keep the SNAP window open a few more frames to catch post-teardown state.
	SnapWindowEnd = CurrentFrame() + SnapTrailFrames;
}

void FTMStage1Trace::LogNextSpawn(UWorld* World, uint32 PrevActorUid)
{
	const float Rt = World ? World->GetRealTimeSeconds() : -1.f;
	const float Wt = World ? World->GetTimeSeconds()     : -1.f;

	UE_LOG(LogTMStage1, Log,
		TEXT("TMS1|f=%llu|rt=%.6f|wt=%.6f|ev=NEXT_SPAWN|aname=spawner|auid=0|cyc=%d")
		TEXT("|prev_auid=%u|activetargets=%d|visiblepresentations=%d"),
		CurrentFrame(), Rt, Wt, CycleIndex,
		PrevActorUid,
		QueryActiveTargets(World),
		CountVisiblePresentations(World));
}

// ------------------------------------------------------------- snapshot -----

void FTMStage1Trace::TickSnapshot(AActor* Actor, UMeshComponent* Mesh, USceneComponent* VisualMotion)
{
	const uint64 F = CurrentFrame();
	if (F < SnapWindowStart || F > SnapWindowEnd) { return; }   // outside window: no-op
	if (!Actor) { return; }

	UWorld* W = Actor->GetWorld();

	UMaterialInterface*       MI  = Mesh ? Mesh->GetMaterial(0) : nullptr;
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MI);   // N1

	// Scalars — read-only.
	FString ScalarStr;
	if (MID)
	{
		for (const FName& P : ExitScalarParams())
		{
			const float V = MID->K2_GetScalarParameterValue(P);   // N2: getter
			ScalarStr += FString::Printf(TEXT("|sc:%s=%.6f"), *P.ToString(), V);
		}
	}
	else
	{
		ScalarStr = TEXT("|sc:UNAVAILABLE=1");   // itself a finding — see Kit §6.1
	}

	const FVector Delta =
		Actor->GetActorLocation() -
		[&]() -> FVector
		{
			FVector L(0); FRotator R(0);
			if (const APlayerController* PC = UGameplayStatics::GetPlayerController(Actor, 0))
			{
				PC->GetPlayerViewPoint(L, R);
			}
			return L;
		}();

	const float ApDistStraight  = Delta.Size();
	const float ApDistProjected = FVector::DotProduct(Delta, ApproachAxis().GetSafeNormal());

	UE_LOG(LogTMStage1, Log,
		TEXT("%s|axform=%s|mxform=%s|vmrel=%s|apdist=%.3f|apdist_proj=%.3f")
		TEXT("|matiface=%s|midvalid=%d|miduid=%d%s")
		TEXT("|meshvis=%d|meshhidden=%d|actorhidden=%d|beingdestroyed=%d")
		TEXT("|activetargets=%d|visiblepresentations=%d"),
		*Header(Actor, TEXT("SNAP")),
		*XformToStr(Actor->GetActorTransform()),
		Mesh         ? *XformToStr(Mesh->GetComponentTransform())     : TEXT("null"),
		VisualMotion ? *XformToStr(VisualMotion->GetRelativeTransform()) : TEXT("null"),
		ApDistStraight, ApDistProjected,
		MI  ? *MI->GetName() : TEXT("null"),
		MID ? 1 : 0,
		MID ? static_cast<int32>(MID->GetUniqueID()) : -1,
		*ScalarStr,
		(Mesh && Mesh->IsVisible())   ? 1 : 0,
		(Mesh && Mesh->bHiddenInGame) ? 1 : 0,
		Actor->IsHidden()             ? 1 : 0,
		Actor->IsActorBeingDestroyed()? 1 : 0,
		QueryActiveTargets(W),
		CountVisiblePresentations(W));
}
