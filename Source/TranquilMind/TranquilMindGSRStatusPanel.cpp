// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindGSRStatusPanel.h"

#include "TranquilMindPhysiologyReceiver.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindGSRStatusPanel, Log, All);

namespace TranquilMindGSRPanelStyle
{
    const FColor HeaderColor  (140, 135, 200);   // muted violet
    const FColor LiveColor    ( 90, 220, 255);   // calm cyan
    const FColor StaleColor   (255, 185,  90);   // soft amber
    const FColor WaitingColor (150, 150, 165);   // neutral gray
    const FColor DetailColor  (200, 205, 215);   // light gray
}

ATranquilMindGSRStatusPanel::ATranquilMindGSRStatusPanel()
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

    using namespace TranquilMindGSRPanelStyle;

    HeaderText = MakeLine(TEXT("HeaderText"), 2.2f, HeaderColor,   8.5f);
    StateText  = MakeLine(TEXT("StateText"),  5.0f, WaitingColor,  1.5f);
    DetailText = MakeLine(TEXT("DetailText"), 2.6f, DetailColor,  -4.5f);

    HeaderText->SetText(FText::FromString(TEXT("GSR MONITOR")));
    StateText->SetText(FText::FromString(TEXT("WAITING")));
    DetailText->SetText(FText::FromString(TEXT("awaiting signal")));
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

    using namespace TranquilMindGSRPanelStyle;

    if (!bHaveSample)
    {
        StateText->SetText(FText::FromString(TEXT("WAITING")));
        StateText->SetTextRenderColor(WaitingColor);
        DetailText->SetText(FText::FromString(TEXT("awaiting signal")));
        return;
    }

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

    StateText->SetText(FText::FromString(bIsLive ? TEXT("LIVE") : TEXT("STALE")));
    StateText->SetTextRenderColor(bIsLive ? LiveColor : StaleColor);

    DetailText->SetText(FText::FromString(FString::Printf(
        TEXT("Signal %d   Level %d"),
        Sample.Quality, Sample.Smoothed)));
}
