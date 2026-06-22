// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TranquilMindUISubsystem.generated.h"

// Spawns ATranquilMindHintPanel once when a game world begins play.
// No level placement needed — UE auto-creates this subsystem per world.
UCLASS()
class TRANQUILMIND_API UTranquilMindUISubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
