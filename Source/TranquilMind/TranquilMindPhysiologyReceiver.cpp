// Fill out your copyright notice in the Description page of Project Settings.

#include "TranquilMindPhysiologyReceiver.h"

#include "Common/UdpSocketBuilder.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTranquilMindPhysiology, Log, All);

static TAutoConsoleVariable<bool> CVarTranquilMindPhysiologyEnabled(
    TEXT("tranquilmind.physiology.enabled"),
    true,
    TEXT("Enable the TranquilMind physiology (GSR) UDP receiver subsystem."),
    ECVF_Default);

bool UTranquilMindPhysiologyReceiver::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    const UWorld* World = Cast<UWorld>(Outer);
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    return CVarTranquilMindPhysiologyEnabled.GetValueOnGameThread();
}

void UTranquilMindPhysiologyReceiver::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    UE_LOG(LogTranquilMindPhysiology, Warning, TEXT("[Physiology] Subsystem starting..."));
    StartListening();
}

void UTranquilMindPhysiologyReceiver::Deinitialize()
{
    StopListening();
    Super::Deinitialize();
}

void UTranquilMindPhysiologyReceiver::StartListening()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
    if (!SocketSubsystem)
    {
        UE_LOG(LogTranquilMindPhysiology, Warning,
            TEXT("[Physiology] No socket subsystem available; GSR receiver disabled."));
        return;
    }

    ListenSocket = FUdpSocketBuilder(TEXT("TranquilMindPhysiologyReceiver"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToAddress(FIPv4Address::Any)
        .BoundToPort(ListenPort)
        .WithReceiveBufferSize(2 * 1024 * 1024)
        .Build();

    if (!ListenSocket)
    {
        UE_LOG(LogTranquilMindPhysiology, Warning,
            TEXT("[Physiology] Failed to bind UDP socket on port %d; GSR receiver disabled."), ListenPort);
        return;
    }

    SocketReceiver = MakeUnique<FUdpSocketReceiver>(
        ListenSocket, FTimespan::FromMilliseconds(100), TEXT("TranquilMindPhysiologyReceiverThread"));
    SocketReceiver->OnDataReceived().BindUObject(this, &UTranquilMindPhysiologyReceiver::OnPacketReceived);
    SocketReceiver->Start();

    UE_LOG(LogTranquilMindPhysiology, Warning,
        TEXT("[Physiology] UDP socket bound OK — listening on 0.0.0.0:%d"), ListenPort);
}

void UTranquilMindPhysiologyReceiver::StopListening()
{
    if (SocketReceiver)
    {
        SocketReceiver->Stop();
        SocketReceiver.Reset();
    }

    if (ListenSocket)
    {
        if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get())
        {
            SocketSubsystem->DestroySocket(ListenSocket);
        }
        ListenSocket = nullptr;
    }
}

void UTranquilMindPhysiologyReceiver::OnPacketReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    // Runs on the socket receiver's background thread — keep this minimal and hop
    // to the game thread before touching any Blueprint-visible state.
    if (!Data.IsValid() || Data->Num() <= 0)
    {
        return;
    }

    TArray<uint8> Bytes(Data->GetData(), Data->Num());
    Bytes.Add(0); // null-terminate for safe FString conversion

    TWeakObjectPtr<UTranquilMindPhysiologyReceiver> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, Bytes = MoveTemp(Bytes)]()
    {
        if (UTranquilMindPhysiologyReceiver* Self = WeakThis.Get())
        {
            FString JsonPayload(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData())));
            Self->HandlePacketOnGameThread(JsonPayload);
        }
    });
}

void UTranquilMindPhysiologyReceiver::HandlePacketOnGameThread(FString JsonPayload)
{
    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonPayload);

    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTranquilMindPhysiology, Warning,
            TEXT("[Physiology] Failed to parse UDP payload: %s"), *JsonPayload);
        return;
    }

    double RawValue = 0.0;
    double SmoothedValue = 0.0;
    double QualityValue = 0.0;
    double TimestampValue = 0.0;

    JsonObject->TryGetNumberField(TEXT("raw"), RawValue);
    JsonObject->TryGetNumberField(TEXT("smoothed"), SmoothedValue);
    JsonObject->TryGetNumberField(TEXT("qual"), QualityValue);
    JsonObject->TryGetNumberField(TEXT("ts"), TimestampValue);

    LatestSample.Raw = static_cast<int32>(RawValue);
    LatestSample.Smoothed = static_cast<int32>(SmoothedValue);
    LatestSample.Quality = static_cast<int32>(QualityValue);
    LatestSample.TimestampMs = static_cast<int64>(TimestampValue);
    LatestSample.bIsValid = true;

    UE_LOG(LogTranquilMindPhysiology, Log,
        TEXT("[Physiology] GSR raw=%d smooth=%d qual=%d ts=%lld"),
        LatestSample.Raw, LatestSample.Smoothed, LatestSample.Quality, LatestSample.TimestampMs);
}
