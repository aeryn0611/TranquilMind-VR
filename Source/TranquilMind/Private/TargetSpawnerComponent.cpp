// Fill out your copyright notice in the Description page of Project Settings.

#include "TargetSpawnerComponent.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "../TranquilMindSessionManager.h"
#include "../Public/Private/Private/TranquilMindTargetActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargetSpawnerComponent, Log, All);

namespace TranquilMindSpawner
{
    constexpr float SpawnDepth_CM = 2000.0f;
    constexpr float GoProbability = 0.80f;

    // TEMP DEBUG POSITION
    constexpr float DebugSpawnX_CM = 800.0f;
    constexpr float DebugSpawnY_CM = 0.0f;
    constexpr float DebugSpawnZ_CM = 150.0f;
}

UTargetSpawnerComponent::UTargetSpawnerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    TargetActorClass = ATranquilMindTargetActor::StaticClass();

    CurrentISI_MS = TranquilMind::MAX_ISI_MS;
    CurrentNoiseAlpha = TranquilMind::MIN_NOISE_ALPHA;
    LastTriggerTimestamp_SEC = -1.0f;
    LastSpawnTimestamp_SEC = -1.0f;

    StimulusRandomSeed = 5202;
    StimulusRandomStream.Initialize(StimulusRandomSeed);
}

void UTargetSpawnerComponent::BeginPlay()
{
    Super::BeginPlay();

    StimulusRandomStream.Initialize(StimulusRandomSeed);

    if (TargetActorClass.Get() == nullptr)
    {
        TargetActorClass = ATranquilMindTargetActor::StaticClass();
    }

    if (!IsValid(SessionManager.Get()))
    {
        AActor* FoundActor = UGameplayStatics::GetActorOfClass(
            this,
            ATranquilMindSessionManager::StaticClass());

        ATranquilMindSessionManager* FoundSessionManager =
            Cast<ATranquilMindSessionManager>(FoundActor);

        if (IsValid(FoundSessionManager))
        {
            RegisterSessionManager(FoundSessionManager);

            UE_LOG(
                LogTargetSpawnerComponent,
                Warning,
                TEXT("[TargetSpawner] Auto-registered SessionManager: %s"),
                *FoundSessionManager->GetName());
        }
        else
        {
            UE_LOG(
                LogTargetSpawnerComponent,
                Error,
                TEXT("[TargetSpawner] No ATranquilMindSessionManager found in level. Spawning disabled."));
        }
    }

    if (bDebugHardwareMode)
    {
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[HardwareDebug] Enabled | ISI=%.0f ms | Alternating GO/NOGO | HardGate bypassed"),
            DebugHardware_ISI_MS);
    }

    if (bDemoMode)
    {
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[DemoMode] Enabled | ISI=%.0f ms | ResponseWindow=%.0f ms | Duration=%.0f s"),
            DemoMode_ISI_MS, DemoMode_ResponseWindow_MS, DemoMode_Duration_SEC);
    }
}

void UTargetSpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(SessionManager.Get()))
    {
        SessionManager->OnStaircaseParamsUpdated.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::ApplyStaircaseParams);

        SessionManager->OnHardGateResumed.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleHardGateResumed);

        SessionManager->OnInterruptStateChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleInterruptStateChanged);

        SessionManager->OnPhaseChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandlePhaseChanged);
    }

    DestroyAllActiveTargetsAsVoid();

    Super::EndPlay(EndPlayReason);
}

void UTargetSpawnerComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    CleanupResolvedTargets();

    if (bDemoMode)
    {
        if (bDemoEnded)
        {
            return;
        }
        DemoElapsed_SEC += DeltaTime;
        if (DemoElapsed_SEC >= DemoMode_Duration_SEC)
        {
            bDemoEnded = true;
            DestroyAllActiveTargetsAsVoid();
            UE_LOG(LogTargetSpawnerComponent, Warning,
                TEXT("[DemoMode] Session ended | Elapsed=%.1f s"),
                DemoElapsed_SEC);
            return;
        }
    }

    if (!CanSpawnTargetsNow())
    {
        if (IsValid(SessionManager.Get()))
        {
            const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

            if (CurrentPhase == ETMSessionPhase::Phase_III_Cooldown ||
                CurrentPhase == ETMSessionPhase::Phase_Terminated)
            {
                DestroyAllActiveTargetsAsVoid();
            }
        }

        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    const float Now_SEC = World->GetTimeSeconds();

    if (LastSpawnTimestamp_SEC < 0.0f)
    {
        SpawnNextTarget();
        return;
    }

    const float ElapsedSinceLastSpawn_MS = (Now_SEC - LastSpawnTimestamp_SEC) * 1000.0f;

    const float EffectiveISI_MS = bDemoMode
        ? DemoMode_ISI_MS
        : (bDebugHardwareMode ? DebugHardware_ISI_MS : CurrentISI_MS);
    if (ElapsedSinceLastSpawn_MS >= EffectiveISI_MS)
    {
        SpawnNextTarget();
    }
}

void UTargetSpawnerComponent::ApplyStaircaseParams(float NewISI_MS, float NewNoiseAlpha)
{
    CurrentISI_MS = FMath::Clamp(
        NewISI_MS,
        TranquilMind::MIN_ISI_MS,
        TranquilMind::MAX_ISI_MS);

    CurrentNoiseAlpha = FMath::Clamp(
        NewNoiseAlpha,
        TranquilMind::MIN_NOISE_ALPHA,
        TranquilMind::MAX_NOISE_ALPHA);

    UE_LOG(
        LogTargetSpawnerComponent,
        Log,
        TEXT("[TargetSpawner] Staircase params applied | ISI=%.0f ms | NoiseAlpha=%.2f"),
        CurrentISI_MS,
        CurrentNoiseAlpha);
}

void UTargetSpawnerComponent::RestartTrial()
{
    DestroyAllActiveTargetsAsVoid();

    LastSpawnTimestamp_SEC = -1.0f;

    if (CanSpawnTargetsNow())
    {
        SpawnNextTarget();
    }
}

void UTargetSpawnerComponent::DestroyAllActiveTargetsAsVoid()
{
    for (int32 Index = ActiveTargets.Num() - 1; Index >= 0; --Index)
    {
        ATranquilMindTargetActor* Target = ActiveTargets[Index].Get();

        if (IsValid(Target) && !Target->IsResolved())
        {
            Target->ResolveAsVoid();
        }

        ActiveTargets.RemoveAtSwap(Index, 1, false);
    }

    ActiveTargets.Reset();
}

void UTargetSpawnerComponent::HandleTriggerPulled(FVector InSmoothedGazeDirection_World)
{
    UWorld* World = GetWorld();
    if (World == nullptr || !IsValid(SessionManager.Get()))
    {
        return;
    }

    const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

    if (CurrentPhase == ETMSessionPhase::Phase_1A_Desensitization ||
        CurrentPhase == ETMSessionPhase::Phase_III_Cooldown ||
        CurrentPhase == ETMSessionPhase::Phase_Terminated)
    {
        return;
    }

    if (CurrentPhase != ETMSessionPhase::Phase_1B_Baseline &&
        CurrentPhase != ETMSessionPhase::Phase_II_CoreTraining)
    {
        return;
    }

    const ETMInterruptType ActiveInterrupt = SessionManager->GetActiveInterrupt();

    if ((!bDebugHardwareMode && ActiveInterrupt == ETMInterruptType::HardGate_GazeLost) ||
        ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        if (bDebugHardwareMode && ActiveInterrupt == ETMInterruptType::SysAbort)
        {
            UE_LOG(LogTargetSpawnerComponent, Warning,
                TEXT("[HardwareDebug] Trigger ignored because SysAbort is active"));
        }
        return;
    }

    const float TriggerTimestamp_SEC = World->GetTimeSeconds();

    if (LastTriggerTimestamp_SEC >= 0.0f)
    {
        const float InterTrigger_MS =
            (TriggerTimestamp_SEC - LastTriggerTimestamp_SEC) * 1000.0f;

        LastTriggerTimestamp_SEC = TriggerTimestamp_SEC;

        if (InterTrigger_MS < TranquilMind::DEBOUNCE_MIN_MS)
        {
            SessionManager->ReportSpamInput();

            UE_LOG(
                LogTargetSpawnerComponent,
                Warning,
                TEXT("[TargetSpawner] Trigger rejected by debounce | Delta=%.2f ms"),
                InterTrigger_MS);

            return;
        }
    }
    else
    {
        LastTriggerTimestamp_SEC = TriggerTimestamp_SEC;
    }

    CleanupResolvedTargets();

    // ============================================================
    // TEMP DEBUG MODE
    //
    // 当前阶段先不用 Tie-Break，只验证：
    // Space / Trigger 是否能一键让场上目标立即消失。
    //
    // 正式版之后再恢复：
    // ATranquilMindTargetActor* BoundTarget =
    //     SelectTargetByTieBreak(InSmoothedGazeDirection_World);
    // BoundTarget->ResolveAsTriggered(TriggerTimestamp_SEC);
    // ============================================================

    if (ActiveTargets.Num() <= 0)
    {
        SessionManager->ReportSpamInput();

        UE_LOG(
            LogTargetSpawnerComponent,
            Warning,
            TEXT("[TargetSpawner] Trigger pulled but no active targets. Reported spam."));

        return;
    }

    UE_LOG(
        LogTargetSpawnerComponent,
        Warning,
        TEXT("[TargetSpawner] DEBUG trigger pulled. Resolving ALL active targets. Count=%d"),
        ActiveTargets.Num());

    for (int32 Index = ActiveTargets.Num() - 1; Index >= 0; --Index)
    {
        ATranquilMindTargetActor* Target = ActiveTargets[Index].Get();

        if (!IsValid(Target))
        {
            ActiveTargets.RemoveAtSwap(Index, 1, false);
            continue;
        }

        if (Target->IsResolved())
        {
            ActiveTargets.RemoveAtSwap(Index, 1, false);
            continue;
        }

        Target->ResolveAsTriggered(TriggerTimestamp_SEC);
        ActiveTargets.RemoveAtSwap(Index, 1, false);
    }

    ActiveTargets.Reset();

    // 防止刚销毁后同一帧或下一帧立刻生成，让肉眼能看到“确实消失了”。
    LastSpawnTimestamp_SEC = TriggerTimestamp_SEC;
}

void UTargetSpawnerComponent::RegisterSessionManager(ATranquilMindSessionManager* InSessionManager)
{
    if (IsValid(SessionManager.Get()))
    {
        SessionManager->OnStaircaseParamsUpdated.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::ApplyStaircaseParams);

        SessionManager->OnHardGateResumed.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleHardGateResumed);

        SessionManager->OnInterruptStateChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandleInterruptStateChanged);

        SessionManager->OnPhaseChanged.RemoveDynamic(
            this,
            &UTargetSpawnerComponent::HandlePhaseChanged);
    }

    SessionManager = InSessionManager;

    if (!IsValid(SessionManager.Get()))
    {
        return;
    }

    SessionManager->OnStaircaseParamsUpdated.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::ApplyStaircaseParams);

    SessionManager->OnHardGateResumed.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::HandleHardGateResumed);

    SessionManager->OnInterruptStateChanged.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::HandleInterruptStateChanged);

    SessionManager->OnPhaseChanged.AddUniqueDynamic(
        this,
        &UTargetSpawnerComponent::HandlePhaseChanged);

    if (bDemoMode)
    {
        SessionManager->bBypassHardGateSuspend = true;
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[DemoMode] HardGate suspend watchdog bypassed"));
    }

    const FSessionStats SessionStats = SessionManager->GetSessionStats();
    ApplyStaircaseParams(SessionStats.Current_ISI_MS, SessionStats.Current_NoiseAlpha);

    if (SessionManager->GetActiveInterrupt() == ETMInterruptType::HardGate_GazeLost)
    {
        DestroyAllActiveTargetsAsVoid();
    }

    UE_LOG(
        LogTargetSpawnerComponent,
        Log,
        TEXT("[TargetSpawner] Registered SessionManager"));
}

void UTargetSpawnerComponent::SpawnNextTarget()
{
    UWorld* World = GetWorld();
    if (World == nullptr || !CanSpawnTargetsNow())
    {
        return;
    }

    CleanupResolvedTargets();

    // ============================================================
    // TEMP DEBUG MODE
    //
    // 当前调试阶段场上只允许 1 个目标。
    // 避免多个目标重叠，导致你按 Space 后看起来像“没消失”。
    // ============================================================
    if (ActiveTargets.Num() > 0)
    {
        return;
    }

    const float SpawnTimestamp_SEC = World->GetTimeSeconds();

    if (TargetActorClass.Get() == nullptr)
    {
        LastSpawnTimestamp_SEC = SpawnTimestamp_SEC;

        UE_LOG(
            LogTargetSpawnerComponent,
            Warning,
            TEXT("[TargetSpawner] TargetActorClass is null. Spawn skipped."));

        return;
    }

    const ETMStimulusType NextStimulusType = ChooseNextStimulusType();

    // TEMP hardware placement test — spawn 200 cm in front of VRCamera.
    // Fall back to hardcoded world position if camera not found.
    FVector SpawnLocation(
        TranquilMindSpawner::DebugSpawnX_CM,
        TranquilMindSpawner::DebugSpawnY_CM,
        TranquilMindSpawner::DebugSpawnZ_CM);

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (APawn* ControlledPawn = PC->GetPawn())
        {
            UCameraComponent* Cam =
                ControlledPawn->FindComponentByClass<UCameraComponent>();
            if (IsValid(Cam))
            {
                const FVector CamLoc = Cam->GetComponentLocation();
                const FVector CamFwd = Cam->GetForwardVector();
                constexpr float SpawnDist_CM = 200.0f;
                SpawnLocation = CamLoc + CamFwd * SpawnDist_CM;

                UE_LOG(LogTargetSpawnerComponent, Warning,
                    TEXT("[TargetSpawner] CamLoc=(%.1f,%.1f,%.1f) Fwd=(%.2f,%.2f,%.2f)"
                         " SpawnAt=(%.1f,%.1f,%.1f) Dist=%.0fcm"),
                    CamLoc.X, CamLoc.Y, CamLoc.Z,
                    CamFwd.X, CamFwd.Y, CamFwd.Z,
                    SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z,
                    SpawnDist_CM);
            }
            else
            {
                UE_LOG(LogTargetSpawnerComponent, Warning,
                    TEXT("[TargetSpawner] No UCameraComponent on pawn — using hardcoded spawn."));
            }
        }
        else
        {
            UE_LOG(LogTargetSpawnerComponent, Warning,
                TEXT("[TargetSpawner] No pawn possessed — using hardcoded spawn."));
        }
    }
    else
    {
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[TargetSpawner] No PlayerController — using hardcoded spawn."));
    }

    const FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = GetOwner();
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ATranquilMindTargetActor* SpawnedTarget =
        World->SpawnActor<ATranquilMindTargetActor>(
            TargetActorClass.Get(),
            SpawnLocation,
            SpawnRotation,
            SpawnParameters);

    LastSpawnTimestamp_SEC = SpawnTimestamp_SEC;

    if (!IsValid(SpawnedTarget))
    {
        UE_LOG(
            LogTargetSpawnerComponent,
            Warning,
            TEXT("[TargetSpawner] SpawnActor failed."));

        return;
    }

    SpawnedTarget->InitializeTarget(
        NextStimulusType,
        SpawnTimestamp_SEC,
        SessionManager.Get());

    if (bDemoMode)
    {
        SpawnedTarget->ResponseWindow_MS = DemoMode_ResponseWindow_MS;
    }

    ActiveTargets.Add(SpawnedTarget);

    UE_LOG(
        LogTargetSpawnerComponent,
        Warning,
        TEXT("[TargetSpawner] Spawned ONE target | Type=%d | Active=%d | Location=(%.1f, %.1f, %.1f)"),
        static_cast<int32>(NextStimulusType),
        ActiveTargets.Num(),
        SpawnLocation.X,
        SpawnLocation.Y,
        SpawnLocation.Z);

    if (bDebugHardwareMode)
    {
        const float DisplayISI_MS = bDemoMode ? DemoMode_ISI_MS : DebugHardware_ISI_MS;
        UE_LOG(LogTargetSpawnerComponent, Warning,
            TEXT("[HardwareDebug] Spawned %s | Next in %.1f s"),
            (NextStimulusType == ETMStimulusType::Go) ? TEXT("GO") : TEXT("NOGO"),
            DisplayISI_MS / 1000.0f);
    }
}

void UTargetSpawnerComponent::CleanupResolvedTargets()
{
    for (int32 Index = ActiveTargets.Num() - 1; Index >= 0; --Index)
    {
        ATranquilMindTargetActor* Target = ActiveTargets[Index].Get();

        if (!IsValid(Target) || Target->IsResolved())
        {
            ActiveTargets.RemoveAtSwap(Index, 1, false);
        }
    }
}

void UTargetSpawnerComponent::HandleInterruptStateChanged(
    ETMInterruptType InterruptType,
    bool bActive)
{
    if (!bActive)
    {
        return;
    }

    if (InterruptType == ETMInterruptType::HardGate_GazeLost ||
        InterruptType == ETMInterruptType::SysAbort)
    {
        DestroyAllActiveTargetsAsVoid();
        LastSpawnTimestamp_SEC = -1.0f;
    }
}

void UTargetSpawnerComponent::HandlePhaseChanged(ETMSessionPhase NewPhase)
{
    if (NewPhase == ETMSessionPhase::Phase_1B_Baseline ||
        NewPhase == ETMSessionPhase::Phase_II_CoreTraining)
    {
        LastSpawnTimestamp_SEC = -1.0f;
        return;
    }

    if (NewPhase == ETMSessionPhase::Phase_1A_Desensitization ||
        NewPhase == ETMSessionPhase::Phase_III_Cooldown ||
        NewPhase == ETMSessionPhase::Phase_Terminated)
    {
        DestroyAllActiveTargetsAsVoid();
        LastSpawnTimestamp_SEC = -1.0f;
    }
}

void UTargetSpawnerComponent::HandleHardGateResumed()
{
    RestartTrial();
}

bool UTargetSpawnerComponent::CanSpawnTargetsNow() const
{
    if (!IsValid(SessionManager.Get()))
    {
        return false;
    }

    const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

    if (CurrentPhase != ETMSessionPhase::Phase_1B_Baseline &&
        CurrentPhase != ETMSessionPhase::Phase_II_CoreTraining)
    {
        return false;
    }

    const ETMInterruptType ActiveInterrupt = SessionManager->GetActiveInterrupt();

    if (ActiveInterrupt == ETMInterruptType::SysAbort)
    {
        return false;
    }

    if (!bDebugHardwareMode &&
        ActiveInterrupt == ETMInterruptType::HardGate_GazeLost)
    {
        return false;
    }

    return true;
}

ETMStimulusType UTargetSpawnerComponent::ChooseNextStimulusType()
{
    if (!IsValid(SessionManager.Get()))
    {
        return ETMStimulusType::Go;
    }

    if (bDebugHardwareMode)
    {
        return (DebugAlternateTypeCounter++ % 2 == 0)
            ? ETMStimulusType::Go
            : ETMStimulusType::NoGo;
    }

    const ETMSessionPhase CurrentPhase = SessionManager->GetCurrentPhase();

    if (CurrentPhase == ETMSessionPhase::Phase_1B_Baseline)
    {
        return ETMStimulusType::Go;
    }

    if (CurrentPhase == ETMSessionPhase::Phase_II_CoreTraining)
    {
        const float Sample = StimulusRandomStream.FRand();

        if (Sample < TranquilMindSpawner::GoProbability)
        {
            return ETMStimulusType::Go;
        }

        return ETMStimulusType::NoGo;
    }

    return ETMStimulusType::Go;
}

ATranquilMindTargetActor* UTargetSpawnerComponent::SelectTargetByTieBreak(
    const FVector& InSmoothedGazeDirection_World) const
{
    ATranquilMindTargetActor* BestTarget = nullptr;
    float BestDepth_CM = 0.0f;
    float BestGazeAngle_DEG = 0.0f;
    float BestSpawnTimestamp_SEC = 0.0f;

    for (int32 Index = 0; Index < ActiveTargets.Num(); ++Index)
    {
        ATranquilMindTargetActor* CandidateTarget = ActiveTargets[Index].Get();

        if (!IsValid(CandidateTarget) ||
            CandidateTarget->IsResolved() ||
            CandidateTarget->HasHadResponse())
        {
            continue;
        }

        const float CandidateDepth_CM = CandidateTarget->GetDepthCM();
        const float CandidateGazeAngle_DEG =
            ComputeGazeAngleToTargetDEG(CandidateTarget, InSmoothedGazeDirection_World);
        const float CandidateSpawnTimestamp_SEC = CandidateTarget->GetSpawnTimestampSEC();

        bool bCandidateWins = false;

        if (BestTarget == nullptr)
        {
            bCandidateWins = true;
        }
        else
        {
            const float DepthDelta_CM = FMath::Abs(CandidateDepth_CM - BestDepth_CM);

            if (DepthDelta_CM > TranquilMind::TIEBREAK_DEPTH_DELTA_CM)
            {
                bCandidateWins = CandidateDepth_CM < BestDepth_CM;
            }
            else
            {
                const float GazeAngleDelta_DEG =
                    FMath::Abs(CandidateGazeAngle_DEG - BestGazeAngle_DEG);

                if (GazeAngleDelta_DEG > TranquilMind::TIEBREAK_GAZE_DELTA_DEG)
                {
                    bCandidateWins = CandidateGazeAngle_DEG < BestGazeAngle_DEG;
                }
                else
                {
                    bCandidateWins = CandidateSpawnTimestamp_SEC < BestSpawnTimestamp_SEC;
                }
            }
        }

        if (bCandidateWins)
        {
            BestTarget = CandidateTarget;
            BestDepth_CM = CandidateDepth_CM;
            BestGazeAngle_DEG = CandidateGazeAngle_DEG;
            BestSpawnTimestamp_SEC = CandidateSpawnTimestamp_SEC;
        }
    }

    return BestTarget;
}

float UTargetSpawnerComponent::ComputeGazeAngleToTargetDEG(
    const ATranquilMindTargetActor* Target,
    const FVector& InSmoothedGazeDirection_World) const
{
    if (!IsValid(Target))
    {
        return 180.0f;
    }

    const AActor* OwnerActor = GetOwner();

    const FVector GazeOrigin =
        OwnerActor != nullptr ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

    const FVector GazeDirection = InSmoothedGazeDirection_World.GetSafeNormal();

    if (GazeDirection.IsNearlyZero())
    {
        return 180.0f;
    }

    const FVector TargetDirection =
        (Target->GetTargetCenterWorld() - GazeOrigin).GetSafeNormal();

    if (TargetDirection.IsNearlyZero())
    {
        return 180.0f;
    }

    const float Dot =
        FMath::Clamp(FVector::DotProduct(GazeDirection, TargetDirection), -1.0f, 1.0f);

    return FMath::RadiansToDegrees(FMath::Acos(Dot));
}
