// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Common/UdpSocketReceiver.h"
#include "TranquilMindPhysiologyReceiver.generated.h"

class FSocket;

USTRUCT(BlueprintType)
struct FTMPhysiologySample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|Physiology")
    int32 Raw = 0;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|Physiology")
    int32 Smoothed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|Physiology")
    int32 Quality = 0;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|Physiology")
    int64 TimestampMs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "TranquilMind|Physiology")
    bool bIsValid = false;
};

// Listens for GSR UDP packets from the ESP32 (port 4210) and caches the latest sample.
// Disable at runtime via console variable: tranquilmind.physiology.enabled 0
UCLASS()
class TRANQUILMIND_API UTranquilMindPhysiologyReceiver : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Physiology")
    FTMPhysiologySample GetLatestSample() const { return LatestSample; }

    UFUNCTION(BlueprintPure, Category = "TranquilMind|Physiology")
    bool HasReceivedAnyPacket() const { return LatestSample.bIsValid; }

private:
    void StartListening();
    void StopListening();

    void OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
    void HandlePacketOnGameThread(FString JsonPayload);

    FSocket* ListenSocket = nullptr;
    TUniquePtr<FUdpSocketReceiver> SocketReceiver;

    FTMPhysiologySample LatestSample;

    static constexpr int32 ListenPort = 4210;
};
