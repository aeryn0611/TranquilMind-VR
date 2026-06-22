// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindHintPanel.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindHintPanel, Log, All);

ATranquilMindHintPanel::ATranquilMindHintPanel()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    PrimaryActorTick.bStartWithTickEnabled = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    SetRootComponent(Root);

    auto MakeLine = [this, Root](
        const FName& Name,
        const FString& Str,
        float Size,
        FColor Color,
        float ZOffset) -> UTextRenderComponent*
    {
        UTextRenderComponent* Comp = CreateDefaultSubobject<UTextRenderComponent>(Name);
        Comp->SetupAttachment(Root);
        Comp->SetText(FText::FromString(Str));
        Comp->SetWorldSize(Size);
        Comp->SetTextRenderColor(Color);
        Comp->SetHorizontalAlignment(EHTA_Center);
        Comp->SetVerticalAlignment(EVRTA_TextCenter);
        Comp->SetRelativeLocation(FVector(0.0f, 0.0f, ZOffset));
        return Comp;
    };

    Text_Line1 = MakeLine(
        TEXT("Text_Line1"),
        TEXT("GREEN = Press Trigger"),
        7.0f,
        FColor(80, 220, 80),
        12.0f);

    Text_Line2 = MakeLine(
        TEXT("Text_Line2"),
        TEXT("RED = Do Not Press"),
        7.0f,
        FColor(255, 80, 80),
        0.0f);

    Text_Line3 = MakeLine(
        TEXT("Text_Line3"),
        TEXT("Session ends after 120s"),
        5.0f,
        FColor(200, 200, 200),
        -10.0f);
}

void ATranquilMindHintPanel::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTranquilMindHintPanel, Warning,
        TEXT("[HintPanel] BeginPlay: actor active, intro timer starting"));
}

void ATranquilMindHintPanel::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIntroComplete)
    {
        return;
    }

    APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (CamManager == nullptr)
    {
        return;
    }

    const FVector CamLoc = CamManager->GetCameraLocation();
    const FRotator CamRot = CamManager->GetCameraRotation();

    // Yaw-only forward: panel stays stable regardless of head pitch/roll
    const FVector YawFwd = FRotator(0.0f, CamRot.Yaw, 0.0f).Vector();

    // 170 cm in front, 55 cm below camera center — lower to reduce visual dominance
    const FVector PanelLoc = CamLoc + YawFwd * 170.0f + FVector(0.0f, 0.0f, -55.0f);

    // Face back toward camera so text is readable from the player's side
    const FRotator PanelRot(0.0f, CamRot.Yaw + 180.0f, 0.0f);

    SetActorLocationAndRotation(PanelLoc, PanelRot);

    IntroElapsed_SEC += DeltaTime;
    if (IntroElapsed_SEC >= IntroDuration_SEC)
    {
        bIntroComplete = true;
        SetActorHiddenInGame(true);
        SetActorTickEnabled(false);
        UE_LOG(LogTranquilMindHintPanel, Warning,
            TEXT("[HintPanel] Hidden after intro duration"));
    }
}
