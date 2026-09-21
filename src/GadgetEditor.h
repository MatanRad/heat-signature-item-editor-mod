#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ModInterface.h"

enum class GadgetRangeTier
{
    Standard,
    Long,
    Extreme
};

struct GadgetCapabilities
{
    bool supportsCapacityPreset = false;
    bool supportsRangePreset = false;
    bool supportsRangeAdvanced = false;
    bool supportsDuration = false;
    bool supportsBlastRadius = false;
    bool supportsVisitorReturnTime = false;
    double standardRange = 0.0;
    double longRange = 0.0;
    double extremeRange = 0.0;
};

struct GadgetParameters
{
    bool   highCapacity = false;
    bool   rechargeable = false;
    bool   selfCharging = false;
    GadgetRangeTier rangeTier = GadgetRangeTier::Standard;
    int    uses = 3;
    int    capacity = 3;
    double chargeRate = 0.0;
    double range = -1.0;
    double duration = -1.0;
    double secondsBetweenUses = 0.0;
    double blastRadius = 0.0;
    double visitorReturnTime = 0.0;
};

struct GadgetSnapshot
{
    int                       handle = 0;
    std::string               name;
    std::string               baseName;
    GadgetCapabilities        capabilities;
    GadgetParameters          parameters;
    std::vector<std::string>  traits;
};

class GadgetEditor
{
public:
    explicit GadgetEditor(const HS_ModApi* api) : m_api(api) {}

    std::optional<GadgetSnapshot> CaptureSnapshot(
        int handle,
        std::string& error) const;

    std::optional<GadgetSnapshot> ApplyParameters(
        int handle,
        const GadgetParameters& parameters,
        std::string& error) const;

private:
    const HS_ModApi* m_api;
};
