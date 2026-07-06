// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindUISubsystem.h"

#include "TranquilMindHintPanel.h"
#include "TranquilMindGSRStatusPanel.h"
#include "TranquilMindDemoSummaryPanel.h"
#include "TranquilMindVoidVisualLayer.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindUISubsystem, Log, All);

bool UTranquilMindUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    UWorld* World = Cast<UWorld>(Outer);
    const bool bResult = World != nullptr && World->IsGameWorld();
    if (bResult)
    {
        UE_LOG(LogTranquilMindUISubsystem, Warning,
            TEXT("[UISubsystem] ShouldCreateSubsystem -> true"));
    }
    return bResult;
}

void UTranquilMindUISubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    UE_LOG(LogTranquilMindUISubsystem, Warning,
        TEXT("[UISubsystem] OnWorldBeginPlay: spawning ATranquilMindHintPanel"));

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(FVector(0.0f, 0.0f, -10000.0f));

    AActor* Spawned = InWorld.SpawnActor<ATranquilMindHintPanel>(
        ATranquilMindHintPanel::StaticClass(),
        SpawnTransform);

    UE_LOG(LogTranquilMindUISubsystem, Warning,
        TEXT("[UISubsystem] ATranquilMindHintPanel spawn result: %s"),
        Spawned ? TEXT("OK") : TEXT("FAILED - nullptr returned"));

    AActor* SpawnedGSR = InWorld.SpawnActor<ATranquilMindGSRStatusPanel>(
        ATranquilMindGSRStatusPanel::StaticClass(),
        SpawnTransform);

    UE_LOG(LogTranquilMindUISubsystem, Warning,
        TEXT("[UISubsystem] ATranquilMindGSRStatusPanel spawn result: %s"),
        SpawnedGSR ? TEXT("OK") : TEXT("FAILED - nullptr returned"));

    AActor* SpawnedSummary = InWorld.SpawnActor<ATranquilMindDemoSummaryPanel>(
        ATranquilMindDemoSummaryPanel::StaticClass(),
        SpawnTransform);

    UE_LOG(LogTranquilMindUISubsystem, Warning,
        TEXT("[UISubsystem] ATranquilMindDemoSummaryPanel spawn result: %s"),
        SpawnedSummary ? TEXT("OK") : TEXT("FAILED - nullptr returned"));

    // World-anchored ambient environment (identity transform, not camera-relative).
    AActor* SpawnedVisual = InWorld.SpawnActor<ATranquilMindVoidVisualLayer>(
        ATranquilMindVoidVisualLayer::StaticClass(),
        FTransform::Identity);

    UE_LOG(LogTranquilMindUISubsystem, Warning,
        TEXT("[UISubsystem] ATranquilMindVoidVisualLayer spawn result: %s"),
        SpawnedVisual ? TEXT("OK") : TEXT("FAILED - nullptr returned"));
}
