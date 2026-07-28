#include "TMResearchSettings.h"

#include "HAL/IConsoleManager.h"

// -1 = use settings value; 0 = force Demo; 1 = force Research.
// Lets a tester flip modes on-device via the console without repackaging.
static TAutoConsoleVariable<int32> CVarOperatingModeOverride(
    TEXT("tranquilmind.OperatingMode"),
    -1,
    TEXT("Override TranquilMind operating mode. -1=use settings, 0=Demo, 1=Research."),
    ECVF_Default);

const UTMResearchSettings* UTMResearchSettings::Get()
{
    return GetDefault<UTMResearchSettings>();
}

ETMOperatingMode UTMResearchSettings::GetEffectiveOperatingMode()
{
    const int32 Override = CVarOperatingModeOverride.GetValueOnAnyThread();
    if (Override == 0)
    {
        return ETMOperatingMode::Demo;
    }
    if (Override == 1)
    {
        return ETMOperatingMode::Research;
    }

    const UTMResearchSettings* Settings = Get();
    return Settings != nullptr ? Settings->OperatingMode : ETMOperatingMode::Demo;
}
