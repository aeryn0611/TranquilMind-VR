// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TranquilMindHintPanel.generated.h"

class UTextRenderComponent;

UCLASS()
class TRANQUILMIND_API ATranquilMindHintPanel : public AActor
{
    GENERATED_BODY()

public:
    ATranquilMindHintPanel();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Text_Line1;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Text_Line2;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Text_Line3;

    float IntroElapsed_SEC = 0.0f;
    bool  bIntroComplete   = false;

    static constexpr float IntroDuration_SEC = 5.0f;
};
