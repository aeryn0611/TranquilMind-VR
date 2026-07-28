// TMStage1Trace.cpp
// TranquilMind Stage 1 instrumentation — LOG ONLY. See TMStage1Trace.h.
//
// Every material and component access below is a getter. There is no
// CreateDynamicMaterialInstance, no setter, no transform write, no Destroy and
// no random draw anywhere in this file (Kit §0, N1–N3).

#include "TMStage1Trace.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "CoreGlobals.h"                 // GFrameCounter
#include "Engine/World.h"
#include "EngineUtils.h"                 // TActorIterator
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY(LogTMStage1);

TMap<uint32, FTMStage1Trace::FActorTrace> FTMStage1Trace::ActorTraces;
int32   FTMStage1Trace::CycleCounter               = 0;
uint32  FTMStage1Trace::LastSpawnAuid              = 0;
int32   FTMStage1Trace::SessionCounter             = 0;
int32   FTMStage1Trace::CurrentSid                 = -1;
int32   FTMStage1Trace::SpawnSeqCounter            = 0;
int32   FTMStage1Trace::SessionSpawnCount          = 0;
TWeakObjectPtr<UWorld> FTMStage1Trace::CurrentWorld;
FVector FTMStage1Trace::ApproachAxisWorld          = FVector::ForwardVector;
UClass* FTMStage1Trace::TargetPresentationClassPtr = nullptr;
TFunction<int32(UWorld*)> FTMStage1Trace::ActiveTargetsProviderFn;
TFunction<int32(UWorld*)> FTMStage1Trace::LivePresentationProviderFn;

// ---------------------------------------------------------------- config ----

const TArray<FName>& FTMStage1Trace::ExitScalarParams()
{
	// Resolved from the project (Kit §1, slot P11):
	//   CenterOpacity / EdgeOpacity / BubbleBrightness — written by
	//     ATranquilMindTargetActor::HandleVisualExitFade.
	//   FresnelExponent — a Group-A parameter the exit must NOT write. Logged
	//     as a control: if it moves, something outside the exit driver is
	//     writing the MID.
	static const TArray<FName> Params = {
		FName(TEXT("CenterOpacity")),
		FName(TEXT("EdgeOpacity")),
		FName(TEXT("BubbleBrightness")),
		FName(TEXT("FresnelExponent")),
	};
	return Params;
}

FVector FTMStage1Trace::GetApproachAxis()
{
	return ApproachAxisWorld;
}

void FTMStage1Trace::SetApproachAxis(const FVector& InAxis)
{
	// Diagnostic bookkeeping only — nothing in gameplay reads this back.
	if (!InAxis.IsNearlyZero())
	{
		ApproachAxisWorld = InAxis.GetSafeNormal();
	}
}

UClass* FTMStage1Trace::GetTargetPresentationClass()
{
	return TargetPresentationClassPtr;
}

void FTMStage1Trace::SetTargetPresentationClass(UClass* InClass)
{
	TargetPresentationClassPtr = InClass;
}

void FTMStage1Trace::SetActiveTargetsProvider(TFunction<int32(UWorld*)> InProvider)
{
	ActiveTargetsProviderFn = MoveTemp(InProvider);
}

void FTMStage1Trace::ClearActiveTargetsProvider()
{
	ActiveTargetsProviderFn = nullptr;
}

void FTMStage1Trace::SetLivePresentationProvider(TFunction<int32(UWorld*)> InProvider)
{
	LivePresentationProviderFn = MoveTemp(InProvider);
}

void FTMStage1Trace::ClearLivePresentationProvider()
{
	LivePresentationProviderFn = nullptr;
}

uint32 FTMStage1Trace::LastSpawnedAuid()
{
	return LastSpawnAuid;
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
	// D5: the kit's tracer separates the L/R/S groups with '|', which is also
	// parse_tm_trace.py's field separator — the parser would split the payload
	// and keep only "[L=x,y,z" under the axform/mxform/vmrel key, colliding the
	// stray R= and S= fragments across all three transforms. ';' keeps each
	// transform whole, so Kit §6.2's before/after comparison sees rotation and
	// scale changes too, not just translation. Format only; no semantic change.
	return FString::Printf(
		TEXT("[L=%.3f,%.3f,%.3f;R=%.3f,%.3f,%.3f;S=%.4f,%.4f,%.4f]"),
		L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll, S.X, S.Y, S.Z);
}

void FTMStage1Trace::EnsureSession(UWorld* World)
{
	// Fallback only. LogSessionBegin from the spawner's BeginPlay is the normal
	// path and always precedes any presentation event. This exists so a stray
	// event can never be emitted under a stale sid from a previous world.
	if (World == nullptr)
	{
		return;
	}
	if (CurrentSid >= 0 && CurrentWorld.Get() == World)
	{
		return;
	}
	LogSessionBegin(World, TEXT("Unknown"));
}

void FTMStage1Trace::LogSessionBegin(UWorld* World, const TCHAR* Mode)
{
	if (World == nullptr)
	{
		return;
	}
	if (CurrentSid >= 0 && CurrentWorld.Get() == World)
	{
		return;   // idempotent for an already-open world
	}

	// A different world is starting: close the previous session first so the
	// log can never contain two open sessions.
	if (CurrentSid >= 0)
	{
		LogSessionEnd(CurrentWorld.Get(), -1, TEXT("SupersededByNewWorld"));
	}

	// Diagnostic counter, never an RNG (protects R5/R7). Monotonic for the
	// lifetime of the process, so a sid is never reused.
	CurrentSid        = ++SessionCounter;
	CurrentWorld      = World;
	SpawnSeqCounter   = 0;
	SessionSpawnCount = 0;
	ActorTraces.Empty();   // identities are scoped to a session

	const bool bPIE = (World->WorldType == EWorldType::PIE);

	UE_LOG(LogTMStage1, Log,
		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=SESSION_BEGIN|sid=%d|spawnseq=-1")
		TEXT("|aname=session|auid=0|cyc=0|map=%s|worldtype=%d|pie=%d|mode=%s|netmode=%d"),
		static_cast<unsigned long long>(CurrentFrame()),
		World->GetRealTimeSeconds(), World->GetTimeSeconds(),
		CurrentSid,
		*World->GetMapName(),
		static_cast<int32>(World->WorldType),
		bPIE ? 1 : 0,
		Mode,
		static_cast<int32>(World->GetNetMode()));
}

void FTMStage1Trace::LogSessionEnd(UWorld* World, int32 Reason, const TCHAR* ReasonText)
{
	if (CurrentSid < 0)
	{
		return;
	}

	UWorld* W = World ? World : CurrentWorld.Get();
	const float Rt = W ? W->GetRealTimeSeconds() : -1.f;
	const float Wt = W ? W->GetTimeSeconds()     : -1.f;

	UE_LOG(LogTMStage1, Log,
		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=SESSION_END|sid=%d|spawnseq=-1")
		TEXT("|aname=session|auid=0|cyc=0|reason=%d|reasontext=%s|spawns=%d"),
		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt,
		CurrentSid, Reason, ReasonText, SessionSpawnCount);

	CurrentSid = -1;
	CurrentWorld.Reset();
	ActorTraces.Empty();
}

FString FTMStage1Trace::Header(AActor* Actor, const TCHAR* Event)
{
	UWorld* W = Actor ? Actor->GetWorld() : nullptr;
	EnsureSession(W);

	const float Rt = W ? W->GetRealTimeSeconds() : -1.f;
	const float Wt = W ? W->GetTimeSeconds()     : -1.f;

	// D6: cycle, sid and spawnseq all come from THIS actor's own record, not
	// from a global counter, so an overlapping successor cannot relabel these
	// lines. (sid, spawnseq) is the canonical identity; auid is diagnostic only.
	const uint32 Uid = Actor ? Actor->GetUniqueID() : 0u;
	const FActorTrace* Trace = ActorTraces.Find(Uid);

	return FString::Printf(
		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=%s|sid=%d|spawnseq=%d")
		TEXT("|aname=%s|auid=%u|cyc=%d"),
		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt, Event,
		Trace ? Trace->Sid      : CurrentSid,
		Trace ? Trace->SpawnSeq : -1,
		Actor ? *Actor->GetName() : TEXT("null"),
		Uid,
		Trace ? Trace->Cycle : -1);
}

int32 FTMStage1Trace::CountVisiblePresentations(UWorld* World)
{
	UClass* Cls = GetTargetPresentationClass();
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
	return ActiveTargetsProviderFn ? ActiveTargetsProviderFn(World) : -1;
}

int32 FTMStage1Trace::QueryLivePresentation(UWorld* World)
{
	return LivePresentationProviderFn ? LivePresentationProviderFn(World) : -1;
}

// ------------------------------------------------------------ events --------

void FTMStage1Trace::LogSpawn(AActor* Actor, UMeshComponent* Mesh)
{
	// D6: open a record for THIS actor. D2: the SNAP window opens here rather
	// than retroactively at EXIT_BEGIN − 5, which could never have captured
	// frames that had already elapsed.
	EnsureSession(Actor ? Actor->GetWorld() : nullptr);

	LastSpawnAuid = Actor ? Actor->GetUniqueID() : 0u;

	// Schema 2: spawnseq is assigned EXACTLY ONCE here, from a per-session
	// monotonic counter. It is never derived from a UObject UniqueID and never
	// resets until the session ends, so (sid, spawnseq) survives GC recycling of
	// auid. FindOrAdd overwrites any stale record left by a recycled index —
	// which is precisely the H2 case, and is why the new identity is assigned
	// rather than reused.
	FActorTrace& Trace = ActorTraces.FindOrAdd(LastSpawnAuid);
	Trace.Cycle         = ++CycleCounter;
	Trace.SpawnSeq      = SpawnSeqCounter++;
	Trace.Sid           = CurrentSid;
	Trace.bExit70Logged = false;
	Trace.SnapWindowEnd = TNumericLimits<uint64>::Max();   // narrowed by LogEndPlay
	++SessionSpawnCount;

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
	if (FActorTrace* Trace = ActorTraces.Find(Actor ? Actor->GetUniqueID() : 0u))
	{
		Trace->bExit70Logged = false;
	}

	UE_LOG(LogTMStage1, Log, TEXT("%s"), *Header(Actor, TEXT("EXIT_BEGIN")));
}

void FTMStage1Trace::LogExit70(AActor* Actor, float ElapsedMs)
{
	// D6: the latch is per actor. With a single global flag an overlapping
	// successor's exit would suppress this actor's EXIT_70 entirely.
	FActorTrace* Trace = ActorTraces.Find(Actor ? Actor->GetUniqueID() : 0u);
	if (Trace == nullptr || Trace->bExit70Logged) { return; }
	Trace->bExit70Logged = true;

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

	// Keep this actor's window nominally open a few more frames; in practice
	// the mesh and motion component die with the actor, so trailing SNAPs only
	// appear if something outlives EndPlay. The record itself is retained (not
	// erased) so any such late line still carries the correct cycle number.
	if (FActorTrace* Trace = ActorTraces.Find(Actor ? Actor->GetUniqueID() : 0u))
	{
		Trace->SnapWindowEnd = CurrentFrame() + SnapTrailFrames;
	}

	// Bound the map: UE recycles object indices, so without pruning a long
	// session could both grow this map and let a recycled uid inherit a stale
	// cycle number. Anything already past its trail window is finished.
	if (ActorTraces.Num() > 64)
	{
		const uint64 Now = CurrentFrame();
		for (auto It = ActorTraces.CreateIterator(); It; ++It)
		{
			if (It.Value().SnapWindowEnd + SnapTrailFrames < Now)
			{
				It.RemoveCurrent();
			}
		}
	}
}

void FTMStage1Trace::LogNextSpawn(UWorld* World, uint32 PrevActorUid)
{
	EnsureSession(World);

	const float Rt = World ? World->GetRealTimeSeconds() : -1.f;
	const float Wt = World ? World->GetTimeSeconds()     : -1.f;

	// NEXT_SPAWN is a spawner-level event, so it carries the session identity but
	// spawnseq=-1: the presentation it announces has not been created yet and
	// therefore has no identity to report. prev_auid stays diagnostic-only.
	UE_LOG(LogTMStage1, Log,
		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=NEXT_SPAWN|sid=%d|spawnseq=-1")
		TEXT("|aname=spawner|auid=0|cyc=%d")
		TEXT("|prev_auid=%u|prev_spawnseq=%d|activetargets=%d|visiblepresentations=%d")
		TEXT("|livepresentation=%d"),
		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt,
		CurrentSid, CycleCounter,
		PrevActorUid,
		[&]() -> int32
		{
			const FActorTrace* T = ActorTraces.Find(PrevActorUid);
			return T ? T->SpawnSeq : -1;
		}(),
		QueryActiveTargets(World),
		CountVisiblePresentations(World),
		QueryLivePresentation(World));
}

// ------------------------------------------------------------- snapshot -----

void FTMStage1Trace::TickSnapshot(AActor* Actor, UMeshComponent* Mesh, USceneComponent* VisualMotion)
{
	if (!Actor) { return; }

	// D6: the window belongs to this actor. Absent record = not yet spawned or
	// already pruned; no-op either way.
	const FActorTrace* Trace = ActorTraces.Find(Actor->GetUniqueID());
	if (Trace == nullptr || CurrentFrame() > Trace->SnapWindowEnd) { return; }

	UWorld* W = Actor->GetWorld();

	UMaterialInterface*       MI  = Mesh ? Mesh->GetMaterial(0) : nullptr;
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MI);   // N1

	// Scalars — read-only. When slot 0 is not a MID the values live on the
	// shared MI and cannot be read through this path; that absence is itself
	// evidence (Kit §6.1, "MID lost mid-fade" row) so it is recorded, not fixed.
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
		ScalarStr = TEXT("|sc:UNAVAILABLE=1");
	}

	FVector ViewLoc = FVector::ZeroVector;
	{
		FRotator ViewRot = FRotator::ZeroRotator;
		if (const APlayerController* PC = UGameplayStatics::GetPlayerController(Actor, 0))
		{
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		}
	}

	const FVector Delta = Actor->GetActorLocation() - ViewLoc;
	const float ApDistStraight  = Delta.Size();
	const float ApDistProjected = FVector::DotProduct(Delta, GetApproachAxis());

	UE_LOG(LogTMStage1, Log,
		TEXT("%s|axform=%s|mxform=%s|vmrel=%s|apdist=%.3f|apdist_proj=%.3f")
		TEXT("|matiface=%s|midvalid=%d|miduid=%d%s")
		TEXT("|meshvis=%d|meshhidden=%d|actorhidden=%d|beingdestroyed=%d")
		TEXT("|activetargets=%d|visiblepresentations=%d|livepresentation=%d"),
		*Header(Actor, TEXT("SNAP")),
		*XformToStr(Actor->GetActorTransform()),
		Mesh         ? *XformToStr(Mesh->GetComponentTransform())       : TEXT("null"),
		VisualMotion ? *XformToStr(VisualMotion->GetRelativeTransform()) : TEXT("null"),
		ApDistStraight, ApDistProjected,
		MI  ? *MI->GetName() : TEXT("null"),
		MID ? 1 : 0,
		MID ? static_cast<int32>(MID->GetUniqueID()) : -1,
		*ScalarStr,
		(Mesh && Mesh->IsVisible())    ? 1 : 0,
		(Mesh && Mesh->bHiddenInGame)  ? 1 : 0,
		Actor->IsHidden()              ? 1 : 0,
		Actor->IsActorBeingDestroyed() ? 1 : 0,
		QueryActiveTargets(W),
		CountVisiblePresentations(W),
		QueryLivePresentation(W));
}
