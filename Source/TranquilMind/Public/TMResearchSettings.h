// TranquilMind project settings — operating mode + Research config.
//
// Config-backed (DefaultGame.ini). This is the single, explicit place where the
// operating mode is selected. Default is Demo, preserving verified production
// behavior; Research is opt-in.
//
// On-device override (no repackage): console variable
//   tranquilmind.OperatingMode   (-1 = use settings, 0 = Demo, 1 = Research)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "../TranquilMindTypes.h"
#include "TMResearchConfig.h"
#include "TMResearchSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TranquilMind"))
class TRANQUILMIND_API UTMResearchSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Operating mode selected for this build. Default Demo (safe / unchanged production). */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Mode")
    ETMOperatingMode OperatingMode = ETMOperatingMode::Demo;

    /** Fixed-parameter Research protocol config (Phase 1). */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "TranquilMind|Research")
    FTMResearchConfig ResearchConfig;

    /** Convenience accessor. */
    static const UTMResearchSettings* Get();

    /**
     * Resolves the effective operating mode, honoring the console-variable override
     * (tranquilmind.OperatingMode) if it is set to 0 or 1; otherwise returns the
     * configured OperatingMode. Always safe to call.
     */
    static ETMOperatingMode GetEffectiveOperatingMode();
};
