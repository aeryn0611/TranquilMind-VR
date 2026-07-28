# Stage 1 tracer — exact source diff, schema 1 -> schema 2

Generated with `diff -u`. Left = tracer as it stood at the end of Stage 1
(schema 1). Right = hardened schema 2. Runtime behaviour is unchanged: every
added statement is a log emission or diagnostic-counter update.


```diff
--- a/TMStage1Trace.h	2026-07-28 05:13:41
+++ b/Source/TranquilMind/Public/TMStage1Trace.h	2026-07-28 05:14:26
@@ -45,6 +45,39 @@
 //      disappeared from its own ledger. Because the defect is triggered by the
 //      exact condition Stage 1 exists to measure, it would silently corrupt the
 //      authoritative capture. This state is therefore keyed per actor.
+//
+// ---------------------------------------------------------------------------
+// SCHEMA 2 (Pre-Stage-2 instrumentation hardening)
+// ---------------------------------------------------------------------------
+// Schema 1 carried two identity defects, both confirmed against real Stage 1
+// evidence:
+//
+//  H1  Mixed sessions. A single editor process writes one log across many PIE
+//      sessions. Schema 1 had no session marker, so a whole-log parse silently
+//      merged four PIE runs (Stage 1 report §"authoritative capture" had to be
+//      isolated by hand from log line 15024).
+//
+//  H2  UObject UniqueID recycling. auid and miduid are object-table indices and
+//      are reused after GC. In the authoritative PIE capture 40 spawns produced
+//      only 39 distinct auid and 38 distinct miduid, and one miduid appeared
+//      under two auid — which the schema-1 rules would have reported as
+//      "MID SHARED ACROSS ACTORS — serious". It was recycling: the lifetimes do
+//      not overlap.
+//
+// Schema 2 fixes both by carrying a stable, non-UObject identity on every line:
+//
+//      sid       unique per world/session, stable for that world's lifetime,
+//                never reused within one editor process
+//      spawnseq  monotonic within sid, assigned exactly once at presentation
+//                creation, never derived from a UObject UniqueID
+//
+//      CANONICAL ACTOR IDENTITY = (sid, spawnseq)
+//
+// auid, miduid and the actor name remain as DIAGNOSTIC FIELDS ONLY and must
+// never be treated as globally unique.
+//
+// sid and spawnseq come from plain diagnostic counters. No RNG of any kind is
+// used, so R5 and R7 cannot be perturbed.
 
 #pragma once
 
@@ -96,6 +129,22 @@
 	/** D4: UniqueID of the most recent SPAWN, for NEXT_SPAWN's prev_auid. */
 	static uint32 LastSpawnedAuid();
 
+	// ---- session boundaries (schema 2) -------------------------------------
+
+	/**
+	 * Open a trace session for this world. Called from the spawner's BeginPlay,
+	 * which is guaranteed to precede any target presentation in that world.
+	 * Allocates a fresh sid from a diagnostic counter (never an RNG) and resets
+	 * spawnseq. Idempotent for a world already open.
+	 *
+	 * @param Mode  "Demo" / "Research" / "Unknown" — supplied by the caller so
+	 *              the tracer never reads settings or protected state itself.
+	 */
+	static void LogSessionBegin(UWorld* World, const TCHAR* Mode);
+
+	/** Close the current session, recording the teardown reason when known. */
+	static void LogSessionEnd(UWorld* World, int32 Reason, const TCHAR* ReasonText);
+
 	// ---- event logging -----------------------------------------------------
 
 	static void LogSpawn(AActor* Actor, UMeshComponent* Mesh);
@@ -126,25 +175,37 @@
 	static void TickSnapshot(AActor* Actor, UMeshComponent* Mesh, USceneComponent* VisualMotion);
 
 private:
-	static uint64  CurrentFrame();
 	static FString Header(AActor* Actor, const TCHAR* Event);
 	static FString XformToStr(const FTransform& T);
 	static int32   CountVisiblePresentations(UWorld* World);
 	static int32   QueryActiveTargets(UWorld* World);
 
-	/** D6: per-actor trace state, so overlapping presentations cannot steal
-	 *  each other's cycle number or EXIT_70 latch. */
+	static uint64  CurrentFrame();
+	static void    EnsureSession(UWorld* World);
+
+	/** D6 + schema 2: per-actor trace state. Overlapping presentations cannot
+	 *  steal each other's cycle number, EXIT_70 latch, sid or spawnseq. */
 	struct FActorTrace
 	{
 		int32  Cycle          = 0;
+		int32  SpawnSeq       = -1;   // canonical identity, with Sid
+		int32  Sid            = -1;
 		bool   bExit70Logged  = false;
 		uint64 SnapWindowEnd  = TNumericLimits<uint64>::Max();
 	};
 
-	/** Diagnostic bookkeeping only. Never read by gameplay. */
+	/** Diagnostic bookkeeping only. Never read by gameplay, never from an RNG. */
 	static TMap<uint32, FActorTrace> ActorTraces;
 	static int32   CycleCounter;
 	static uint32  LastSpawnAuid;
+
+	/** Session state (schema 2). SessionCounter never resets within a process,
+	 *  so a sid is never reused; SpawnSeqCounter resets per session. */
+	static int32   SessionCounter;
+	static int32   CurrentSid;
+	static int32   SpawnSeqCounter;
+	static int32   SessionSpawnCount;
+	static TWeakObjectPtr<UWorld> CurrentWorld;
 	static FVector ApproachAxisWorld;
 	static UClass* TargetPresentationClassPtr;
 	static TFunction<int32(UWorld*)> ActiveTargetsProviderFn;
```

```diff
--- a/TMStage1Trace.cpp	2026-07-28 05:13:41
+++ b/Source/TranquilMind/Private/TMStage1Trace.cpp	2026-07-28 05:15:31
@@ -23,6 +23,11 @@
 TMap<uint32, FTMStage1Trace::FActorTrace> FTMStage1Trace::ActorTraces;
 int32   FTMStage1Trace::CycleCounter               = 0;
 uint32  FTMStage1Trace::LastSpawnAuid              = 0;
+int32   FTMStage1Trace::SessionCounter             = 0;
+int32   FTMStage1Trace::CurrentSid                 = -1;
+int32   FTMStage1Trace::SpawnSeqCounter            = 0;
+int32   FTMStage1Trace::SessionSpawnCount          = 0;
+TWeakObjectPtr<UWorld> FTMStage1Trace::CurrentWorld;
 FVector FTMStage1Trace::ApproachAxisWorld          = FVector::ForwardVector;
 UClass* FTMStage1Trace::TargetPresentationClassPtr = nullptr;
 TFunction<int32(UWorld*)> FTMStage1Trace::ActiveTargetsProviderFn;
@@ -108,20 +113,105 @@
 		L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll, S.X, S.Y, S.Z);
 }
 
-FString FTMStage1Trace::Header(AActor* Actor, const TCHAR* Event)
+void FTMStage1Trace::EnsureSession(UWorld* World)
 {
-	const UWorld* W  = Actor ? Actor->GetWorld() : nullptr;
-	const float   Rt = W ? W->GetRealTimeSeconds() : -1.f;
-	const float   Wt = W ? W->GetTimeSeconds()     : -1.f;
+	// Fallback only. LogSessionBegin from the spawner's BeginPlay is the normal
+	// path and always precedes any presentation event. This exists so a stray
+	// event can never be emitted under a stale sid from a previous world.
+	if (World == nullptr)
+	{
+		return;
+	}
+	if (CurrentSid >= 0 && CurrentWorld.Get() == World)
+	{
+		return;
+	}
+	LogSessionBegin(World, TEXT("Unknown"));
+}
 
-	// D6: the cycle number comes from THIS actor's own record, not a global
-	// counter, so an overlapping successor cannot relabel these lines.
+void FTMStage1Trace::LogSessionBegin(UWorld* World, const TCHAR* Mode)
+{
+	if (World == nullptr)
+	{
+		return;
+	}
+	if (CurrentSid >= 0 && CurrentWorld.Get() == World)
+	{
+		return;   // idempotent for an already-open world
+	}
+
+	// A different world is starting: close the previous session first so the
+	// log can never contain two open sessions.
+	if (CurrentSid >= 0)
+	{
+		LogSessionEnd(CurrentWorld.Get(), -1, TEXT("SupersededByNewWorld"));
+	}
+
+	// Diagnostic counter, never an RNG (protects R5/R7). Monotonic for the
+	// lifetime of the process, so a sid is never reused.
+	CurrentSid        = ++SessionCounter;
+	CurrentWorld      = World;
+	SpawnSeqCounter   = 0;
+	SessionSpawnCount = 0;
+	ActorTraces.Empty();   // identities are scoped to a session
+
+	const bool bPIE = (World->WorldType == EWorldType::PIE);
+
+	UE_LOG(LogTMStage1, Log,
+		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=SESSION_BEGIN|sid=%d|spawnseq=-1")
+		TEXT("|aname=session|auid=0|cyc=0|map=%s|worldtype=%d|pie=%d|mode=%s|netmode=%d"),
+		static_cast<unsigned long long>(CurrentFrame()),
+		World->GetRealTimeSeconds(), World->GetTimeSeconds(),
+		CurrentSid,
+		*World->GetMapName(),
+		static_cast<int32>(World->WorldType),
+		bPIE ? 1 : 0,
+		Mode,
+		static_cast<int32>(World->GetNetMode()));
+}
+
+void FTMStage1Trace::LogSessionEnd(UWorld* World, int32 Reason, const TCHAR* ReasonText)
+{
+	if (CurrentSid < 0)
+	{
+		return;
+	}
+
+	UWorld* W = World ? World : CurrentWorld.Get();
+	const float Rt = W ? W->GetRealTimeSeconds() : -1.f;
+	const float Wt = W ? W->GetTimeSeconds()     : -1.f;
+
+	UE_LOG(LogTMStage1, Log,
+		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=SESSION_END|sid=%d|spawnseq=-1")
+		TEXT("|aname=session|auid=0|cyc=0|reason=%d|reasontext=%s|spawns=%d"),
+		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt,
+		CurrentSid, Reason, ReasonText, SessionSpawnCount);
+
+	CurrentSid = -1;
+	CurrentWorld.Reset();
+	ActorTraces.Empty();
+}
+
+FString FTMStage1Trace::Header(AActor* Actor, const TCHAR* Event)
+{
+	UWorld* W = Actor ? Actor->GetWorld() : nullptr;
+	EnsureSession(W);
+
+	const float Rt = W ? W->GetRealTimeSeconds() : -1.f;
+	const float Wt = W ? W->GetTimeSeconds()     : -1.f;
+
+	// D6: cycle, sid and spawnseq all come from THIS actor's own record, not
+	// from a global counter, so an overlapping successor cannot relabel these
+	// lines. (sid, spawnseq) is the canonical identity; auid is diagnostic only.
 	const uint32 Uid = Actor ? Actor->GetUniqueID() : 0u;
 	const FActorTrace* Trace = ActorTraces.Find(Uid);
 
 	return FString::Printf(
-		TEXT("TMS1|f=%llu|rt=%.6f|wt=%.6f|ev=%s|aname=%s|auid=%u|cyc=%d"),
+		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=%s|sid=%d|spawnseq=%d")
+		TEXT("|aname=%s|auid=%u|cyc=%d"),
 		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt, Event,
+		Trace ? Trace->Sid      : CurrentSid,
+		Trace ? Trace->SpawnSeq : -1,
 		Actor ? *Actor->GetName() : TEXT("null"),
 		Uid,
 		Trace ? Trace->Cycle : -1);
@@ -166,12 +256,23 @@
 	// D6: open a record for THIS actor. D2: the SNAP window opens here rather
 	// than retroactively at EXIT_BEGIN − 5, which could never have captured
 	// frames that had already elapsed.
+	EnsureSession(Actor ? Actor->GetWorld() : nullptr);
+
 	LastSpawnAuid = Actor ? Actor->GetUniqueID() : 0u;
 
+	// Schema 2: spawnseq is assigned EXACTLY ONCE here, from a per-session
+	// monotonic counter. It is never derived from a UObject UniqueID and never
+	// resets until the session ends, so (sid, spawnseq) survives GC recycling of
+	// auid. FindOrAdd overwrites any stale record left by a recycled index —
+	// which is precisely the H2 case, and is why the new identity is assigned
+	// rather than reused.
 	FActorTrace& Trace = ActorTraces.FindOrAdd(LastSpawnAuid);
 	Trace.Cycle         = ++CycleCounter;
+	Trace.SpawnSeq      = SpawnSeqCounter++;
+	Trace.Sid           = CurrentSid;
 	Trace.bExit70Logged = false;
 	Trace.SnapWindowEnd = TNumericLimits<uint64>::Max();   // narrowed by LogEndPlay
+	++SessionSpawnCount;
 
 	UMaterialInterface*       MI  = Mesh ? Mesh->GetMaterial(0) : nullptr;
 	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MI);   // N1: read, never create
@@ -296,14 +397,26 @@
 
 void FTMStage1Trace::LogNextSpawn(UWorld* World, uint32 PrevActorUid)
 {
+	EnsureSession(World);
+
 	const float Rt = World ? World->GetRealTimeSeconds() : -1.f;
 	const float Wt = World ? World->GetTimeSeconds()     : -1.f;
 
+	// NEXT_SPAWN is a spawner-level event, so it carries the session identity but
+	// spawnseq=-1: the presentation it announces has not been created yet and
+	// therefore has no identity to report. prev_auid stays diagnostic-only.
 	UE_LOG(LogTMStage1, Log,
-		TEXT("TMS1|f=%llu|rt=%.6f|wt=%.6f|ev=NEXT_SPAWN|aname=spawner|auid=0|cyc=%d")
-		TEXT("|prev_auid=%u|activetargets=%d|visiblepresentations=%d"),
-		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt, CycleCounter,
+		TEXT("TMS1|schema=2|f=%llu|rt=%.6f|wt=%.6f|ev=NEXT_SPAWN|sid=%d|spawnseq=-1")
+		TEXT("|aname=spawner|auid=0|cyc=%d")
+		TEXT("|prev_auid=%u|prev_spawnseq=%d|activetargets=%d|visiblepresentations=%d"),
+		static_cast<unsigned long long>(CurrentFrame()), Rt, Wt,
+		CurrentSid, CycleCounter,
 		PrevActorUid,
+		[&]() -> int32
+		{
+			const FActorTrace* T = ActorTraces.Find(PrevActorUid);
+			return T ? T->SpawnSeq : -1;
+		}(),
 		QueryActiveTargets(World),
 		CountVisiblePresentations(World));
 }
```

```diff
--- a/TargetSpawnerComponent.cpp	2026-07-28 05:13:41
+++ b/Source/TranquilMind/Private/TargetSpawnerComponent.cpp	2026-07-28 05:15:43
@@ -110,6 +110,15 @@
         TEXT("[Mode] ===== TranquilMind OperatingMode = %s ====="),
         (OperatingMode == ETMOperatingMode::Research) ? TEXT("RESEARCH") : TEXT("DEMO"));
 
+    // ---- STAGE 1 INSTRUMENTATION, schema 2 (log-only) ----
+    // Opened here because this is the earliest TranquilMind hook in a world and
+    // it necessarily precedes every target presentation (this component is what
+    // spawns them). The mode string is passed in rather than read by the tracer,
+    // so the tracer never touches settings or protected state.
+    FTMStage1Trace::LogSessionBegin(
+        GetWorld(),
+        (OperatingMode == ETMOperatingMode::Research) ? TEXT("Research") : TEXT("Demo"));
+
     if (OperatingMode == ETMOperatingMode::Research)
     {
         const UTMResearchSettings* Settings = UTMResearchSettings::Get();
@@ -213,7 +222,13 @@
     DestroyAllActiveTargetsAsVoid();
 
     // Stage 1 instrumentation teardown (log-only state; no gameplay effect).
+    // SESSION_END records the real EEndPlayReason so a manually-stopped PIE run
+    // is distinguishable from a natural session end.
     FTMStage1Trace::ClearActiveTargetsProvider();
+    FTMStage1Trace::LogSessionEnd(
+        GetWorld(),
+        static_cast<int32>(EndPlayReason),
+        *UEnum::GetValueAsString(EndPlayReason));
 
     Super::EndPlay(EndPlayReason);
 }
```
