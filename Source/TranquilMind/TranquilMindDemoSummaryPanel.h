// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TranquilMindDemoSummaryPanel.generated.h"

class UTextRenderComponent;
class UTargetSpawnerComponent;

// Portfolio demo ending panel. Hidden during the active task; reveals itself
// when the TargetSpawner's demo-mode timer ends. Read-only consumer of the
// spawner's demo state and the physiology receiver — never affects gameplay,
// spawning, scoring, HardGate, or session logic.
UCLASS()
class TRANQUILMIND_API ATranquilMindDemoSummaryPanel : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindDemoSummaryPanel();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    void RevealSummary();
    FString BuildSummaryBodyText() const;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> TitleText;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> BodyText;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> FooterText;

    UPROPERTY()
    TObjectPtr<UTargetSpawnerComponent> ObservedSpawner;

    // Same freshness bookkeeping as the GSR status panel: ESP32 ts is its own
    // boot-relative millis(), so age is measured from when WE last saw it change.
    int64  LastSeenTimestampMs     = -1;
    double LastSeenAtWorldTime_SEC = 0.0;

    bool bRevealed = false;

    static constexpr float StaleThreshold_SEC = 3.0f;
};
