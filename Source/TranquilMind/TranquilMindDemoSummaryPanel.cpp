// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindDemoSummaryPanel.h"

#include "TranquilMindPhysiologyReceiver.h"
#include "TargetSpawnerComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindDemoSummary, Log, All);

ATranquilMindDemoSummaryPanel::ATranquilMindDemoSummaryPanel()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    SetRootComponent(Root);

    auto MakeLine = [this, Root](
        const FName& Name,
        float Size,
        FColor Color,
        float ZOffset) -> UTextRenderComponent*
    {
        UTextRenderComponent* Comp = CreateDefaultSubobject<UTextRenderComponent>(Name);
        Comp->SetupAttachment(Root);
        Comp->SetWorldSize(Size);
        Comp->SetTextRenderColor(Color);
        Comp->SetHorizontalAlignment(EHTA_Center);
        Comp->SetVerticalAlignment(EVRTA_TextCenter);
        Comp->SetRelativeLocation(FVector(0.0f, 0.0f, ZOffset));
        return Comp;
    };

    TitleText  = MakeLine(TEXT("TitleText"),  7.0f, FColor(120, 220, 160),  22.0f);
    BodyText   = MakeLine(TEXT("BodyText"),   4.0f, FColor(225, 225, 225),  -4.0f);
    FooterText = MakeLine(TEXT("FooterText"), 3.0f, FColor(150, 170, 190), -28.0f);

    TitleText->SetText(FText::FromString(TEXT("TranquilMind Demo Complete")));
    FooterText->SetText(FText::FromString(TEXT("Real-time physiological sensing verified on Quest.")));
}

void ATranquilMindDemoSummaryPanel::BeginPlay()
{
    Super::BeginPlay();

    SetActorHiddenInGame(true);

    UE_LOG(LogTranquilMindDemoSummary, Warning,
        TEXT("[DemoSummary] BeginPlay: panel spawned hidden, waiting for demo end"));
}

void ATranquilMindDemoSummaryPanel::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    // Track GSR freshness continuously so the age is accurate at reveal time.
    if (UTranquilMindPhysiologyReceiver* Receiver = World->GetSubsystem<UTranquilMindPhysiologyReceiver>())
    {
        const FTMPhysiologySample Sample = Receiver->GetLatestSample();
        if (Sample.bIsValid && Sample.TimestampMs != LastSeenTimestampMs)
        {
            LastSeenTimestampMs     = Sample.TimestampMs;
            LastSeenAtWorldTime_SEC = World->GetTimeSeconds();
        }
    }

    if (!bRevealed)
    {
        // Lazy read-only lookup: the spawner component lives on a level actor.
        if (ObservedSpawner == nullptr)
        {
            for (TObjectIterator<UTargetSpawnerComponent> It; It; ++It)
            {
                if (It->GetWorld() == World)
                {
                    ObservedSpawner = *It;
                    UE_LOG(LogTranquilMindDemoSummary, Warning,
                        TEXT("[DemoSummary] Observing TargetSpawnerComponent on '%s'"),
                        *GetNameSafe(It->GetOwner()));
                    break;
                }
            }
        }

        if (ObservedSpawner != nullptr && ObservedSpawner->IsDemoEnded())
        {
            RevealSummary();
        }
        return;
    }

    // Revealed: keep the panel camera-anchored and its GSR line current.
    APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (CamManager == nullptr)
    {
        return;
    }

    const FVector CamLoc = CamManager->GetCameraLocation();
    const FRotator CamRot = CamManager->GetCameraRotation();

    const FVector YawFwd = FRotator(0.0f, CamRot.Yaw, 0.0f).Vector();
    const FVector PanelLoc = CamLoc + YawFwd * 170.0f + FVector(0.0f, 0.0f, 0.0f);
    const FRotator PanelRot(0.0f, CamRot.Yaw + 180.0f, 0.0f);

    SetActorLocationAndRotation(PanelLoc, PanelRot);

    BodyText->SetText(FText::FromString(BuildSummaryBodyText()));
}

void ATranquilMindDemoSummaryPanel::RevealSummary()
{
    bRevealed = true;

    const FString Body = BuildSummaryBodyText();
    BodyText->SetText(FText::FromString(Body));

    SetActorHiddenInGame(false);

    UE_LOG(LogTranquilMindDemoSummary, Warning,
        TEXT("[DemoSummary] Demo end detected — summary panel shown"));
    UE_LOG(LogTranquilMindDemoSummary, Warning,
        TEXT("[DemoSummary] Body:\n%s"), *Body);
}

FString ATranquilMindDemoSummaryPanel::BuildSummaryBodyText() const
{
    FString StreamLine = TEXT("Physiological stream: WAITING");
    FString QualityLine;

    if (const UWorld* World = GetWorld())
    {
        if (UTranquilMindPhysiologyReceiver* Receiver = World->GetSubsystem<UTranquilMindPhysiologyReceiver>())
        {
            const FTMPhysiologySample Sample = Receiver->GetLatestSample();
            if (Sample.bIsValid)
            {
                const float Age_SEC = World->GetTimeSeconds() - LastSeenAtWorldTime_SEC;
                if (Age_SEC < StaleThreshold_SEC)
                {
                    StreamLine  = TEXT("Physiological stream: GSR LIVE");
                    QualityLine = FString::Printf(TEXT("Signal quality: %d\n"), Sample.Quality);
                }
                else
                {
                    StreamLine = TEXT("Physiological stream: STALE");
                }
            }
        }
    }

    return FString::Printf(
        TEXT("Session Summary\n\n")
        TEXT("Task loop: GO/NOGO interaction completed\n")
        TEXT("%s\n")
        TEXT("%s")
        TEXT("Data path: ESP32 -> Wi-Fi UDP -> Quest -> Unreal\n")
        TEXT("Mode: exploratory prototype, not clinical assessment"),
        *StreamLine,
        *QualityLine);
}
