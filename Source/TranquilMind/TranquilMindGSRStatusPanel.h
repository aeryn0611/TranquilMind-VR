// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TranquilMindGSRStatusPanel.generated.h"

class UTextRenderComponent;

// Minimal, always-visible VR text readout of the latest GSR sample.
// Read-only consumer of UTranquilMindPhysiologyReceiver — never affects
// gameplay, target spawning, ISI, scoring, HardGate, or session logic.
UCLASS()
class TRANQUILMIND_API ATranquilMindGSRStatusPanel : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindGSRStatusPanel();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> StatusText;

    // ESP32 ts (millis() since ESP32 boot) can't be compared to Unreal's world
    // clock directly, so "Age" is measured from when THIS actor last noticed
    // the timestamp change, not from the ESP32 timestamp itself.
    int64  LastSeenTimestampMs     = -1;
    double LastSeenAtWorldTime_SEC = 0.0;
    bool   bHasLoggedFirstLive     = false;

    static constexpr float StaleThreshold_SEC = 3.0f;
};
