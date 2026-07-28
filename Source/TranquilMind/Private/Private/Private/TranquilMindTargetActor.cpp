// Fill out your copyright notice in the Description page of Project Settings.

#include "Private/Private/TranquilMindTargetActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "TMDemoLifecycleComponent.h"
#include "TMDemoSessionFrame.h"
#include "TMStage1Trace.h"
#include "TMVisualMotionComponent.h"
#include "TargetSpawnerComponent.h"
#include "../../../TranquilMindSessionManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindTargetActor, Log, All);

namespace TranquilMindTarget
{
    constexpr float DebugResponseWindow_MS = 1500.0f;
    constexpr float DebugMovementSpeed_CMPerSec = 50.0f;
    constexpr float DebugDestroyDepth_CM = -10000.0f;
    constexpr float DebugTargetScale = 0.25f;
}

ATranquilMindTargetActor::ATranquilMindTargetActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // ============================================================
    //  COMPONENT HIERARCHY
    //
    //  GameplayAnchor (ROOT)  — stable gameplay transform: placement, travel,
    //      |                    depth, selection center. Never animated.
    //  VisualMotion           — presentation-only relative offset/rotation/
    //      |                    scale (Demo idle float + spawn ease-in).
    //  MeshComponent          — rendered visual. Name kept byte-identical to
    //                           preserve BP_Target SCS/property bindings.
    //
    //  With VisualMotion inert (the default, and forced in Research), every
    //  relative transform is identity, so the mesh world transform is exactly
    //  what it was when MeshComponent itself was the root.
    // ============================================================

    GameplayAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("GameplayAnchor"));
    SetRootComponent(GameplayAnchor);
    GameplayAnchor->SetMobility(EComponentMobility::Movable);

    VisualMotion = CreateDefaultSubobject<UTMVisualMotionComponent>(TEXT("VisualMotion"));
    VisualMotion->SetupAttachment(GameplayAnchor);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(VisualMotion);

    // STAGE 2: the Demo presentation lifecycle. Inert by default — it is a
    // non-scene ActorComponent with tick disabled and phase None, so its mere
    // presence on an actor shared with Research is behaviour-neutral.
    DemoLifecycle = CreateDefaultSubobject<UTMDemoLifecycleComponent>(TEXT("DemoLifecycle"));

    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GoMatFinder(
        TEXT("/Game/MI_Target_Go.MI_Target_Go"));
    if (GoMatFinder.Succeeded())
    {
        MeshComponent->SetMaterial(0, GoMatFinder.Object);
        DebugGoMaterial = GoMatFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NoGoMatFinder(
        TEXT("/Game/MI_Target_NoGo.MI_Target_NoGo"));
    if (NoGoMatFinder.Succeeded())
    {
        DebugNoGoMaterial = NoGoMatFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BubbleGoMatFinder(
        TEXT("/Game/Environment/Materials/MI_Bubble_Go.MI_Bubble_Go"));
    if (BubbleGoMatFinder.Succeeded())
    {
        DemoBubbleGoMaterial = BubbleGoMatFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BubbleNoGoMatFinder(
        TEXT("/Game/Environment/Materials/MI_Bubble_NoGo.MI_Bubble_NoGo"));
    if (BubbleNoGoMatFinder.Succeeded())
    {
        DemoBubbleNoGoMaterial = BubbleNoGoMatFinder.Object;
    }

    StimulusType = ETMStimulusType::Go;
    SpawnTimestamp_SEC = 0.0f;
    bResolved = false;
    bHadResponse = false;

    ResponseWindow_MS = TranquilMindTarget::DebugResponseWindow_MS;
    MovementSpeed_CMPerSec = TranquilMindTarget::DebugMovementSpeed_CMPerSec;
    DestroyDepth_CM = TranquilMindTarget::DebugDestroyDepth_CM;

    TravelDirection_World = FVector(-1.0f, 0.0f, 0.0f);

    SetActorScale3D(FVector(
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale));
}

void ATranquilMindTargetActor::BeginPlay()
{
    Super::BeginPlay();

    ResponseWindow_MS = TranquilMindTarget::DebugResponseWindow_MS;
    MovementSpeed_CMPerSec = TranquilMindTarget::DebugMovementSpeed_CMPerSec;
    DestroyDepth_CM = TranquilMindTarget::DebugDestroyDepth_CM;

    SetActorScale3D(FVector(
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale,
        TranquilMindTarget::DebugTargetScale));

    EnforceZAxisTravelDirection();
    LockCurrentYZ();

    // ---- STAGE 1 INSTRUMENTATION (log-only, Spec §11 / Kit §4 site 1) ----
    // Publish the presentation class once so SNAP can count every Demo target
    // presentation in the world, then emit SPAWN. At this point the mesh still
    // carries the constructor material; InitializeTarget() swaps it to the
    // bubble instance immediately afterwards, and the first SNAP records that.
    FTMStage1Trace::SetTargetPresentationClass(ATranquilMindTargetActor::StaticClass());
    FTMStage1Trace::LogSpawn(this, MeshComponent);
}

void ATranquilMindTargetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // ---- STAGE 1 INSTRUMENTATION (log-only, Kit §4 site 13) ----
    // Override exists solely to emit ENDPLAY. It adds no behaviour: the base
    // implementation is called unchanged.
    FTMStage1Trace::LogEndPlay(this, EndPlayReason);

    Super::EndPlay(EndPlayReason);
}

void ATranquilMindTargetActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Research Mode: static visual only. The runner owns all timing/movement/scoring.
    if (bStaticResearchStimulus)
    {
        return;
    }

    if (bResolved)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    // STAGE 2: when the nine-phase lifecycle is armed it is the SINGLE authority
    // for the end of Phase 4 (Spec §4.1), so this second clock is suppressed.
    // Two clocks against the same threshold could disagree by tick order and
    // begin the exit before the outcome was committed. Unarmed paths — Research
    // and any world without the Demo spawn path — keep this check unchanged.
    const bool bLifecycleOwnsResponseWindow =
        IsValid(DemoLifecycle) && DemoLifecycle->IsArmed();

    if (!bLifecycleOwnsResponseWindow)
    {
        const float Now_SEC = World->GetTimeSeconds();
        const float Elapsed_MS = (Now_SEC - SpawnTimestamp_SEC) * 1000.0f;
        if (Elapsed_MS >= ResponseWindow_MS)
        {
            UE_LOG(LogTranquilMindTargetActor, Warning,
                TEXT("[HardwareDebug] Response window expired | Type=%s -> %s"),
                (StimulusType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
                (StimulusType == ETMStimulusType::Go) ? TEXT("Omission") : TEXT("CorrectRejection"));
            ResolveExpiredResponseWindow();
            return;
        }
    }

    const FVector CurrentLocation = GetActorLocation();

    const float Step_CM = FMath::Abs(MovementSpeed_CMPerSec) * DeltaTime;

    // Integrate along the per-instance approach axis resolved at spawn from the
    // Demo session frame. Because the direction is constant, the components
    // perpendicular to it are preserved automatically — this is what the old
    // LockedY/LockedZ freeze was emulating for the world-X-only case.
    const FVector NewLocation = CurrentLocation + TravelDirection_World * Step_CM;
    SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

    // TEMP DEBUG MODE:
    // 暂时关闭到达 DestroyDepth 后自动 Void。
    //
    // 正式版恢复：
    // if (NewX_CM <= DestroyDepth_CM + KINDA_SMALL_NUMBER)
    // {
    //     ResolveAsVoid();
    // }
}

void ATranquilMindTargetActor::InitializeTarget(
    ETMStimulusType InType,
    float InSpawnTimestamp_SEC,
    ATranquilMindSessionManager* InSessionManager)
{
    StimulusType = InType;
    SpawnTimestamp_SEC = InSpawnTimestamp_SEC;
    SessionManager = InSessionManager;

    bResolved = false;
    bHadResponse = false;

    // 强制覆盖运行时参数，避免 BP_Target 旧默认值干扰。
    ResponseWindow_MS = TranquilMindTarget::DebugResponseWindow_MS;
    MovementSpeed_CMPerSec = TranquilMindTarget::DebugMovementSpeed_CMPerSec;
    DestroyDepth_CM = TranquilMindTarget::DebugDestroyDepth_CM;

    // TEMP hardware visibility test — restore to DebugTargetScale (0.25) once ball is confirmed visible
    SetActorScale3D(FVector(1.0f));

    EnforceZAxisTravelDirection();
    LockCurrentYZ();

    SetActorTickEnabled(true);
    SetLifeSpan(0.0f);

    // Visual-only map profile: the dedicated Environment demo uses translucent
    // bubbles. Research calls InitializeResearchVisual() and therefore keeps the
    // original MI_Target_Go / MI_Target_NoGo materials unchanged.
    const bool bUseEnvironmentBubbles = UseEnvironmentDemoVisualProfile();

    // Presentation-layer motion (Demo Visual Pipeline only). Gameplay timing,
    // response window, and the anchor transform are untouched either way.
    if (IsValid(VisualMotion))
    {
        if (bUseEnvironmentBubbles)
        {
            // Deterministic per-instance phase seed: spawn position hash mixed
            // with a monotonic spawn ordinal. No RNG stream is consumed.
            static uint32 VisualMotionSpawnOrdinal = 0;
            const int32 PhaseSeed = static_cast<int32>(
                HashCombine(GetTypeHash(GetActorLocation()), ++VisualMotionSpawnOrdinal));

            VisualMotion->BeginMotion(PhaseSeed);
        }
        else
        {
            VisualMotion->ForceInert();
        }
    }

    UMaterialInterface* TargetMaterial = nullptr;
    if (bUseEnvironmentBubbles)
    {
        TargetMaterial = (StimulusType == ETMStimulusType::Go)
            ? DemoBubbleGoMaterial.Get()
            : DemoBubbleNoGoMaterial.Get();
    }

    // Preserve the verified material as a safe fallback outside Environment or
    // if an Environment material is unavailable.
    if (!IsValid(TargetMaterial))
    {
        TargetMaterial = (StimulusType == ETMStimulusType::Go)
            ? DebugGoMaterial.Get()
            : DebugNoGoMaterial.Get();
    }

    if (IsValid(TargetMaterial))
    {
        MeshComponent->SetMaterial(0, TargetMaterial);
        UE_LOG(LogTranquilMindTargetActor, Warning,
            TEXT("[TargetVisual] Material set | Profile=%s | Type=%s | Material=%s"),
            bUseEnvironmentBubbles ? TEXT("EnvironmentBubble") : TEXT("VerifiedTarget"),
            (StimulusType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
            *TargetMaterial->GetName());
    }
    else
    {
        UE_LOG(LogTranquilMindTargetActor, Warning,
            TEXT("[HardwareDebug] Debug material missing | Type=%s"),
            (StimulusType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"));
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] Initialized | Type=%d | SpawnTime=%.3f | Speed=%.1f | ResponseWindow=%.1f | Scale=1.00 (TEMP)"),
        static_cast<int32>(StimulusType),
        SpawnTimestamp_SEC,
        MovementSpeed_CMPerSec,
        ResponseWindow_MS);
}

void ATranquilMindTargetActor::InitializeResearchVisual(ETMStimulusType InType, float InScale)
{
    StimulusType = InType;
    SessionManager = nullptr;      // runner owns scoring; actor must not self-score

    bResolved = false;
    bHadResponse = false;
    bStaticResearchStimulus = true;

    // No movement / no self-expiry in Research Mode.
    MovementSpeed_CMPerSec = 0.0f;
    TravelDirection_World = FVector::ZeroVector;

    // RESEARCH SAFETY (explicit second layer on top of inert-by-default):
    // force the presentation motion rig fully inert — component tick OFF,
    // relative location/rotation zero, relative scale one. SetActorTickEnabled
    // below only disables the ACTOR tick and would not stop a component tick.
    if (IsValid(VisualMotion))
    {
        VisualMotion->ForceInert();
    }

    // STAGE 2 RESEARCH SAFETY (explicit second layer on top of inert-by-default,
    // mirroring VisualMotion above): the Demo presentation lifecycle must never
    // run on a Research stimulus. The runner owns Research timing, visibility,
    // scoring and destruction (R1-R7).
    if (IsValid(DemoLifecycle))
    {
        DemoLifecycle->ForceInert();
    }

    SetActorScale3D(FVector(InScale));

    UMaterialInterface* Mat = (InType == ETMStimulusType::Go)
        ? DebugGoMaterial.Get()
        : DebugNoGoMaterial.Get();
    if (IsValid(Mat) && MeshComponent != nullptr)
    {
        MeshComponent->SetMaterial(0, Mat);
    }

    // Tick can stay enabled (it early-returns for static stimuli), but disabling it
    // avoids any per-frame work while the bubble is shown.
    SetActorTickEnabled(false);
    SetActorHiddenInGame(false);

    UE_LOG(
        LogTranquilMindTargetActor,
        Verbose,
        TEXT("[TargetActor] Research visual initialized | Type=%s | Scale=%.2f"),
        (InType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
        InScale);
}

void ATranquilMindTargetActor::ResolveAsTriggered(float TriggerTimestamp_SEC)
{
    if (bResolved)
    {
        return;
    }

    // STAGE 2, Spec §4.2 and §5 steps 1/3/5. Input is accepted ONLY in Phase 4
    // and ONLY until the first valid input has been accepted. Anything else is
    // discarded at the point of receipt: not buffered, not queued, not replayed,
    // not scored, and never converted into a commission error. This is defence
    // in depth behind the spawner's ActiveTargets gate — a stale array entry
    // must not be able to score outside Phase 4 (Spec §11.1 S2-G).
    if (IsValid(DemoLifecycle) && DemoLifecycle->IsArmed() && !DemoLifecycle->IsInputOpen())
    {
        UE_LOG(LogTranquilMindTargetActor, Verbose,
            TEXT("[TargetActor] Input discarded outside Phase 4 | Phase=%d"),
            static_cast<int32>(DemoLifecycle->GetPhase()));
        return;
    }

    bResolved = true;
    bHadResponse = true;

    if (StimulusType == ETMStimulusType::Go)
    {
        // Spec §3: Demo reaction time is measured from DemoTrialOnsetTime, the
        // start of Phase 4 — never from PresentationSpawnTime. Under the Stage 2
        // Compatibility timing (Entrance 0, Readable Hold 0) the two coincide,
        // so this is bit-identical today and stays correct once Stage 4 gives
        // Entrance and Readable Hold real durations.
        const float OnsetTime_SEC = (IsValid(DemoLifecycle) && DemoLifecycle->IsArmed())
            ? DemoLifecycle->GetDemoTrialOnsetTime_SEC()
            : SpawnTimestamp_SEC;

        const float RT_MS = FMath::Max(
            0.0f,
            (TriggerTimestamp_SEC - OnsetTime_SEC) * 1000.0f);

        if (IsValid(SessionManager.Get()))
        {
            SessionManager->RecordTrialOutcome(ETMTrialOutcome::Hit, RT_MS);
        }

        UE_LOG(
            LogTranquilMindTargetActor,
            Warning,
            TEXT("[TargetActor] Triggered GO -> Hit | RT=%.1f ms"),
            RT_MS);

        FTMStage1Trace::LogOutcome(this, TEXT("Hit"), RT_MS);   // Kit §4 site 4
    }
    else
    {
        if (IsValid(SessionManager.Get()))
        {
            SessionManager->RecordTrialOutcome(ETMTrialOutcome::Commission, 0.0f);
        }

        UE_LOG(
            LogTranquilMindTargetActor,
            Warning,
            TEXT("[TargetActor] Triggered NOGO -> Commission"));

        FTMStage1Trace::LogOutcome(this, TEXT("Commission"), 0.0f);   // Kit §4 site 4
    }

    // Scored outcome: the presentation continues to ScheduledPhase4End (Spec §5).
    FinishResolution(/*bCancellation=*/false);
}

void ATranquilMindTargetActor::ResolveAsVoid()
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->RecordTrialOutcome(ETMTrialOutcome::Void, 0.0f);
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] Resolved as Void"));

    FTMStage1Trace::LogOutcome(this, TEXT("Void"), 0.0f);   // Kit §4 site 4

    // A Void is a SESSION CANCELLATION (hard gate, phase change, restart, demo
    // end, world teardown), not a scored response. Spec §5 governs accepted
    // input and natural expiry; it does not require a cancelled presentation to
    // linger to its schedule. Ending promptly here keeps every existing
    // cancellation behaviour bit-identical to the Stage 1 baseline.
    FinishResolution(/*bCancellation=*/true);
}

float ATranquilMindTargetActor::GetDepthCM() const
{
    // Depth = distance remaining along the approach axis, measured from the
    // approach origin. Smaller means nearer the participant, which is the
    // ordering SelectTargetByTieBreak relies on.
    //
    // This must follow the same basis as the travel integration: once approach
    // is player-relative, a raw world-X read would mis-rank targets for any
    // heading other than world +X. (Tie-break is currently unused in Demo, so
    // this is a latent-defect fix rather than a behaviour change.)
    if (!TravelDirection_World.IsNearlyZero())
    {
        return FVector::DotProduct(GetActorLocation() - ApproachOriginWorld, -TravelDirection_World);
    }

    return GetActorLocation().X;
}

FVector ATranquilMindTargetActor::GetTargetCenterWorld() const
{
    // GAMEPLAY TRANSFORM RULE: selection reads the stable GameplayAnchor
    // (actor root), never the mesh — the mesh is presentation-layer and may
    // carry a Demo-only visual float offset that must not leak into gaze
    // selection, tie-breaks, or scoring.
    return GetActorLocation();
}

void ATranquilMindTargetActor::ResolveExpiredResponseWindow()
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;

    const ETMTrialOutcome Outcome =
        StimulusType == ETMStimulusType::Go
            ? ETMTrialOutcome::Omission
            : ETMTrialOutcome::CorrectRejection;

    if (IsValid(SessionManager.Get()))
    {
        SessionManager->RecordTrialOutcome(Outcome, 0.0f);
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] Response window expired | Outcome=%d"),
        static_cast<int32>(Outcome));

    // Kit §4 site 4. This is the natural-expiry path Kit §5.3 requires the
    // capture to exercise — no input, window simply runs out.
    FTMStage1Trace::LogOutcome(
        this,
        (Outcome == ETMTrialOutcome::Omission) ? TEXT("Omission") : TEXT("CorrectRejection"),
        0.0f);

    // Natural expiry is a scored outcome. It resolves AT ScheduledPhase4End, so
    // it converges on exactly the same scheduled exit path an early accepted
    // response takes (Spec §5, final paragraph).
    FinishResolution(/*bCancellation=*/false);
}

void ATranquilMindTargetActor::FinishResolution(bool bCancellation)
{
    // Gameplay is fully over at this point: the outcome has been recorded by
    // the caller, bResolved gates every resolve path and the selection loops,
    // and disabling the actor tick stops travel. Only presentation remains.
    //
    // STAGE 2 — this call is deliberately UNCHANGED. Stage 1 §3.2 measured that
    // the actor-level approach freeze during the exit is a direct product of it
    // (the actor transform is bit-identical across every exit frame). Spec §4.1
    // additionally requires motion to be frozen from the outcome lock onward.
    SetActorTickEnabled(false);

    // STAGE 2: when the nine-phase lifecycle is armed it owns everything from
    // the outcome lock onward — deregistration from ActiveTargets (Phase 5),
    // Persistence (Phase 6), the exit envelope (Phase 7), Hidden (Phase 8) and
    // Destroy (Phase 9). This function no longer decides any of it.
    if (IsValid(DemoLifecycle) && DemoLifecycle->IsArmed())
    {
        DemoLifecycle->NotifyOutcomeCommitted(bCancellation);
        return;
    }

    // ---- Unarmed legacy path, byte-for-byte as before Stage 2 ----------------
    // Reached by Research (which never resolves through here anyway) and by any
    // world where the Demo spawn path did not arm the lifecycle.

    if (!bStaticResearchStimulus &&
        IsValid(VisualMotion) &&
        VisualMotion->IsMotionActive())
    {
        UE_LOG(LogTranquilMindTargetActor, Warning,
            TEXT("[TargetActor] FinishResolution -> visual exit (%.2f s) then destroy"),
            VisualMotion->MotionParams.ExitDuration_SEC);

        BeginDemoVisualExit();
        return;
    }

    UE_LOG(
        LogTranquilMindTargetActor,
        Warning,
        TEXT("[TargetActor] FinishResolution -> Destroy immediately"));

    FTMStage1Trace::LogDestroyReq(   // Kit §4 site 12 — BEFORE the call
        this, TEXT("FinishResolution.Immediate"), TEXT("destroy"));

    Destroy();
}

// ============================================================
//  STAGE 2 — LIFECYCLE AND PRESENTATION OWNERSHIP
// ============================================================

void ATranquilMindTargetActor::SetOwningSpawner(UTargetSpawnerComponent* InSpawner)
{
    OwningSpawner = InSpawner;
}

void ATranquilMindTargetActor::ArmDemoPresentationLifecycle(
    float InPresentationSpawnTime_SEC,
    const FTMDemoLifecycleTiming& InTiming)
{
    if (bStaticResearchStimulus)
    {
        // Unreachable by construction (the Demo spawn path is behind the
        // operating-mode guard), asserted anyway: R1-R7 must be untouchable.
        return;
    }

    if (IsValid(DemoLifecycle))
    {
        DemoLifecycle->BeginPresentation(InPresentationSpawnTime_SEC, InTiming);
    }
}

bool ATranquilMindTargetActor::IsDemoInputOpen() const
{
    if (IsValid(DemoLifecycle) && DemoLifecycle->IsArmed())
    {
        return DemoLifecycle->IsInputOpen();
    }

    // Unarmed: pre-Stage-2 condition.
    return !bResolved;
}

void ATranquilMindTargetActor::HandleLifecycleEnterPhase4()
{
    // Spec §4.3: the actor is registered in ActiveTargets ONLY during Phase 4,
    // registered at Phase 4 entry. Not register-then-filter.
    if (UTargetSpawnerComponent* Spawner = OwningSpawner.Get())
    {
        Spawner->RegisterPhase4Target(this);
    }
}

void ATranquilMindTargetActor::HandleLifecycleOutcomeLock()
{
    // Spec §4.3 / §5 step 4: deregistered at the outcome lock, IMMEDIATELY —
    // on every resolve path, including an early accepted response. This is what
    // replaces the old lazy CleanupResolvedTargets() sweep as the authority.
    if (UTargetSpawnerComponent* Spawner = OwningSpawner.Get())
    {
        Spawner->DeregisterPhase4Target(this);
    }
}

void ATranquilMindTargetActor::HandleLifecycleResponseWindowExpired()
{
    UE_LOG(LogTranquilMindTargetActor, Warning,
        TEXT("[HardwareDebug] Response window expired | Type=%s -> %s"),
        (StimulusType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
        (StimulusType == ETMStimulusType::Go) ? TEXT("Omission") : TEXT("CorrectRejection"));

    ResolveExpiredResponseWindow();
}

bool ATranquilMindTargetActor::HandleLifecycleEnterExit()
{
    // C1 RETAINED. The exit envelope is the existing 350 ms VisualMotion form,
    // with the existing MID fade (C2) and the existing progress/completion
    // delegates (C4). Only the decision of WHEN it starts has moved here.
    if (!bStaticResearchStimulus &&
        IsValid(VisualMotion) &&
        VisualMotion->IsMotionActive())
    {
        UE_LOG(LogTranquilMindTargetActor, Warning,
            TEXT("[TargetActor] Phase 7 exit (%.2f s) then destroy"),
            VisualMotion->MotionParams.ExitDuration_SEC);

        BeginDemoVisualExit();
        return true;
    }

    // No envelope armed (Void demo profile, scratch worlds): the exit is
    // instantaneous, exactly as it is today.
    return false;
}

void ATranquilMindTargetActor::HandleLifecycleDestroy()
{
    FTMStage1Trace::LogDestroyReq(   // Kit §4 site 12 — BEFORE the call
        this, TEXT("Lifecycle.Phase9"), TEXT("destroy"));

    Destroy();
}

// STAGE 1 NOTE (Kit §4 sites 8 and 9 — VIS_FALSE and HIDDEN_TRUE):
// There is no SetVisibility(false) and no SetHiddenInGame(true) anywhere on the
// Demo exit path, so neither event has a call site to attach to and neither
// will ever appear in the trace. Spec §4.1 Phase 8 (Hidden) does not exist in
// this code. The Research path's SetActorHiddenInGame calls in TMResearchRunner
// are deliberately NOT instrumented — Stage 1 leaves Research untouched.
void ATranquilMindTargetActor::BeginDemoVisualExit()
{
    // STAGE 2: made explicitly one-shot. VisualMotion::BeginVisualExit() is
    // idempotent while running, but this function is not — it re-creates the MID
    // and re-binds both delegates with AddUObject. It was unreachable twice
    // behind the one-way bResolved latch; now that Phase 7 entry is a scheduled
    // transition rather than an immediate consequence of resolution, the latent
    // hazard is closed rather than left to an invariant elsewhere.
    if (bDemoVisualExitStarted)
    {
        return;
    }
    bDemoVisualExitStarted = true;

    // One-shot fade MID, parented to the CURRENT slot-0 material — the live
    // MI_Bubble_Go or MI_Bubble_NoGo — so every role parameter is inherited
    // unchanged and only the three faded scalars are overridden per frame,
    // strictly within the short exit window.
    // Kit §4 site 5 — emitted BEFORE the MID swap so the MID_CREATE frame and
    // the frame the exit began are unambiguously ordered in the log.
    FTMStage1Trace::LogExitBegin(this);

    if (MeshComponent != nullptr)
    {
        ExitFadeMID = MeshComponent->CreateDynamicMaterialInstance(0);

        // Kit §4 site 2 — immediately after the create call. This is P5, and
        // it is the ONLY CreateDynamicMaterialInstance call site in the project.
        FTMStage1Trace::LogMidCreate(this, MeshComponent, TEXT("BeginDemoVisualExit"));
    }

    if (ExitFadeMID != nullptr)
    {
        ExitBaseCenterOpacity = ExitFadeMID->K2_GetScalarParameterValue(TEXT("CenterOpacity"));
        ExitBaseEdgeOpacity = ExitFadeMID->K2_GetScalarParameterValue(TEXT("EdgeOpacity"));
        ExitBaseBubbleBrightness = ExitFadeMID->K2_GetScalarParameterValue(TEXT("BubbleBrightness"));
    }

    VisualMotion->OnVisualExitProgress.AddUObject(
        this, &ATranquilMindTargetActor::HandleVisualExitFade);
    VisualMotion->OnVisualExitComplete.AddUObject(
        this, &ATranquilMindTargetActor::HandleVisualExitComplete);

    VisualMotion->BeginVisualExit();
}

void ATranquilMindTargetActor::HandleVisualExitFade(float SmoothAlpha)
{
    if (ExitFadeMID == nullptr)
    {
        return;
    }

    // Rim and core fade smoothly toward zero together; brightness rides the
    // same curve so the emissive rim dies with the alpha instead of lingering
    // as a glow on an invisible sphere.
    const float Remain = 1.0f - SmoothAlpha;

    // Kit §4 site 7 — each existing write is left exactly as it was and the log
    // line is appended after it, so MAT_WRITE reports what was actually stored.
    ExitFadeMID->SetScalarParameterValue(TEXT("CenterOpacity"), ExitBaseCenterOpacity * Remain);
    FTMStage1Trace::LogMatWrite(this, TEXT("CenterOpacity"), ExitBaseCenterOpacity * Remain);

    ExitFadeMID->SetScalarParameterValue(TEXT("EdgeOpacity"), ExitBaseEdgeOpacity * Remain);
    FTMStage1Trace::LogMatWrite(this, TEXT("EdgeOpacity"), ExitBaseEdgeOpacity * Remain);

    ExitFadeMID->SetScalarParameterValue(TEXT("BubbleBrightness"), ExitBaseBubbleBrightness * Remain);
    FTMStage1Trace::LogMatWrite(this, TEXT("BubbleBrightness"), ExitBaseBubbleBrightness * Remain);
}

void ATranquilMindTargetActor::HandleVisualExitComplete()
{
    // STAGE 2: the exit envelope completing ends Phase 7. Phases 8 (Hidden) and
    // 9 (Destroy) are the lifecycle's to take — and in Stage 2 it takes them on
    // this same frame, so the rendered frame sequence is identical to the
    // Stage 1 baseline. Giving Hidden a real frame is Stage 3.
    if (IsValid(DemoLifecycle) && DemoLifecycle->IsArmed())
    {
        DemoLifecycle->NotifyExitEnvelopeComplete();
        return;
    }

    FTMStage1Trace::LogDestroyReq(   // Kit §4 site 12 — BEFORE the call
        this, TEXT("HandleVisualExitComplete"), TEXT("destroy"));

    Destroy();
}

void ATranquilMindTargetActor::LockCurrentYZ()
{
    const FVector CurrentLocation = GetActorLocation();

    LockedY_CM = CurrentLocation.Y;
    LockedZ_CM = CurrentLocation.Z;
}

void ATranquilMindTargetActor::EnforceZAxisTravelDirection()
{
    // Resolve the per-instance approach axis from the Demo session frame, so
    // the bubble closes on the participant's captured heading rather than a
    // hard-coded world axis. TravelDirection_World is now genuinely READ by
    // Tick (it was previously written three times and never consumed).
    //
    // Legacy fallback is the historic world -X so behaviour outside a valid
    // Demo frame (Research, headless, no pawn) is bit-identical to before.
    TravelDirection_World = FVector(-1.0f, 0.0f, 0.0f);
    ApproachOriginWorld = GetActorLocation();

    const UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    if (const UTMDemoSessionFrameSubsystem* Frame =
            World->GetSubsystem<UTMDemoSessionFrameSubsystem>())
    {
        if (Frame->IsDemoSessionFrameValid())
        {
            TravelDirection_World = -Frame->GetDemoSessionForward();

            // Depth is "distance still to travel", so it is measured from the
            // point being approached — the camera at spawn — not the immutable
            // environment origin, which the participant may have moved away from.
            ApproachOriginWorld = Frame->GetCurrentCameraLocationOrOrigin();
        }
    }

    // STAGE 1 (log-only, deviation D3): publish the axis actually travelled so
    // SNAP's apdist_proj is measured against it rather than a fixed world axis.
    // Nothing in gameplay reads this back.
    FTMStage1Trace::SetApproachAxis(TravelDirection_World);
}

bool ATranquilMindTargetActor::UseEnvironmentDemoVisualProfile() const
{
    // KNOWN TECH DEBT (intentional, do not expand): map-name gate for the
    // Environment demo visuals. This helper is the single seam the future
    // configurable visual-profile system will replace.
    const UWorld* World = GetWorld();
    return World != nullptr &&
        World->GetMapName().EndsWith(TEXT("L_TranquilMind_Environment"));
}
