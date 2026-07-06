// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindGSRStatusPanel.h"

#include "TranquilMindPhysiologyReceiver.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindGSRStatusPanel, Log, All);

ATranquilMindGSRStatusPanel::ATranquilMindGSRStatusPanel()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    SetRootComponent(Root);

    StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
    StatusText->SetupAttachment(Root);
    StatusText->SetText(FText::FromString(TEXT("GSR: WAITING")));
    StatusText->SetWorldSize(4.0f);
    StatusText->SetTextRenderColor(FColor(180, 220, 255));
    StatusText->SetHorizontalAlignment(EHTA_Center);
    StatusText->SetVerticalAlignment(EVRTA_TextCenter);
}

void ATranquilMindGSRStatusPanel::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTranquilMindGSRStatusPanel, Warning,
        TEXT("[GSRStatus] BeginPlay: actor active"));
}

void ATranquilMindGSRStatusPanel::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (CamManager == nullptr)
    {
        return;
    }

    const FVector CamLoc = CamManager->GetCameraLocation();
    const FRotator CamRot = CamManager->GetCameraRotation();

    // Peripheral placement (up and to the right) so this never competes with
    // the central target-spawn area or the temporary HintPanel intro text.
    const FRotator YawRot(0.0f, CamRot.Yaw, 0.0f);
    const FVector Fwd   = YawRot.Vector();
    const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    const FVector PanelLoc = CamLoc + Fwd * 130.0f + Right * 55.0f + FVector(0.0f, 0.0f, 35.0f);
    const FRotator PanelRot(0.0f, CamRot.Yaw + 180.0f, 0.0f);

    SetActorLocationAndRotation(PanelLoc, PanelRot);

    // ── Read latest GSR sample (read-only; never affects gameplay) ──────
    FTMPhysiologySample Sample;
    bool bHaveSample = false;

    if (UWorld* World = GetWorld())
    {
        if (UTranquilMindPhysiologyReceiver* Receiver = World->GetSubsystem<UTranquilMindPhysiologyReceiver>())
        {
            Sample = Receiver->GetLatestSample();
            bHaveSample = Sample.bIsValid;
        }
    }

    FString DisplayText;

    if (!bHaveSample)
    {
        DisplayText = TEXT("GSR: WAITING");
    }
    else
    {
        if (Sample.TimestampMs != LastSeenTimestampMs)
        {
            LastSeenTimestampMs     = Sample.TimestampMs;
            LastSeenAtWorldTime_SEC = GetWorld()->GetTimeSeconds();
        }

        const float Age_SEC = GetWorld()->GetTimeSeconds() - LastSeenAtWorldTime_SEC;
        const bool  bIsLive = Age_SEC < StaleThreshold_SEC;

        if (bIsLive && !bHasLoggedFirstLive)
        {
            bHasLoggedFirstLive = true;
            UE_LOG(LogTranquilMindGSRStatusPanel, Warning,
                TEXT("[GSRStatus] First LIVE sample observed — raw=%d smooth=%d qual=%d"),
                Sample.Raw, Sample.Smoothed, Sample.Quality);
        }

        DisplayText = FString::Printf(
            TEXT("GSR: %s\nRaw: %d\nSmooth: %d\nQuality: %d\nAge: %.1fs"),
            bIsLive ? TEXT("LIVE") : TEXT("STALE"),
            Sample.Raw, Sample.Smoothed, Sample.Quality, Age_SEC);
    }

    StatusText->SetText(FText::FromString(DisplayText));
}
