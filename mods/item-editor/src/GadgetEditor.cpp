#include "GadgetEditor.h"

#include "GameMakerPropertyAccess.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace
{
    constexpr double kSelfChargingRate = 0.1;
    constexpr int kHighCapacity = 5;
    constexpr int kStandardCapacity = 3;

    std::string ToLower(std::string value)
    {
        for (char& character : value)
        {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }
        return value;
    }

    bool IsGrenadeFamily(const std::string& baseName)
    {
        return ToLower(baseName).find("grenade") != std::string::npos;
    }

    std::vector<std::string> ReadTraitList(
        const GameMakerPropertyAccess& properties,
        int handle,
        int count,
        std::string& error)
    {
        std::vector<std::string> traits;
        traits.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            std::string trait;
            if (!properties.ReadString(handle, "Traits", trait, error, i))
                return {};
            traits.push_back(std::move(trait));
        }
        return traits;
    }

    GadgetCapabilities GetCapabilities(
        const std::string& baseName,
        double range,
        double duration)
    {
        GadgetCapabilities capabilities;
        capabilities.supportsCapacityPreset = true;
        capabilities.supportsRangeAdvanced = range >= 0.0;
        capabilities.supportsDuration = duration >= 0.0;
        capabilities.supportsBlastRadius =
            baseName == "Acid Trap" || baseName == "Crash Trap";
        capabilities.supportsVisitorReturnTime =
            baseName == "Sidewinder" || baseName == "Swapper" ||
            baseName == "Visitor";

        if (baseName == "Frazzler" || baseName == "Crashbeam" ||
            baseName == "Subverter")
        {
            capabilities.supportsRangePreset = true;
            capabilities.standardRange = 300.0;
            capabilities.longRange = 450.0;
            capabilities.extremeRange = 600.0;
        }
        else if (baseName == "Glitch Trap")
        {
            capabilities.supportsRangePreset = true;
            capabilities.standardRange = 450.0;
            capabilities.longRange = 675.0;
            capabilities.extremeRange = 900.0;
        }
        else if (baseName == "Key Cloner")
        {
            capabilities.supportsRangePreset = true;
            capabilities.standardRange = 400.0;
            capabilities.longRange = 600.0;
            capabilities.extremeRange = 800.0;
        }
        else if (baseName == "Sidewinder" || baseName == "Swapper" ||
                 baseName == "Visitor" || baseName == "Teleporter")
        {
            capabilities.supportsRangePreset = true;
            capabilities.standardRange = 350.0;
            capabilities.longRange = 525.0;
            capabilities.extremeRange = 700.0;
        }
        return capabilities;
    }

    std::string BuildGadgetDisplayName(
        const std::string& currentName,
        const GadgetParameters& parameters,
        const GadgetCapabilities& capabilities)
    {
        static const std::unordered_set<std::string> kControlledWords{
            "High", "Capacity", "Rechargeable", "Self-Charging",
            "Extreme", "Long", "Range"
        };

        std::istringstream input(currentName);
        std::ostringstream base;
        std::string word;
        while (input >> word)
        {
            if (kControlledWords.find(word) != kControlledWords.end())
                continue;
            if (base.tellp() > 0)
                base << ' ';
            base << word;
        }

        std::string name;
        if (parameters.selfCharging)
            name = "Self-Charging ";
        else if (parameters.rechargeable)
            name = "Rechargeable ";
        if (parameters.highCapacity)
            name += "High Capacity ";
        if (parameters.rangeTier == GadgetRangeTier::Long)
            name += "Long Range ";
        else if (parameters.rangeTier == GadgetRangeTier::Extreme)
            name += "Extreme Range ";
        return name + base.str();
    }
}

std::optional<GadgetSnapshot> GadgetEditor::CaptureSnapshot(
    int handle,
    std::string& error) const
{
    error.clear();
    if (!m_api || !m_api->ResolveCInstance(handle))
    {
        error = "Item is no longer a live instance";
        return std::nullopt;
    }

    const GameMakerPropertyAccess properties(m_api);
    std::string verb;
    std::string baseName;
    if (!properties.ReadString(handle, "Verb", verb, error) ||
        !properties.ReadString(handle, "BaseName", baseName, error))
    {
        return std::nullopt;
    }
    if (verb != "Use" || IsGrenadeFamily(baseName))
    {
        error = "Selected item is not a supported gadget";
        return std::nullopt;
    }

    GadgetSnapshot snapshot;
    snapshot.handle = handle;
    snapshot.baseName = std::move(baseName);
    if (!properties.ReadString(handle, "Name", snapshot.name, error))
        return std::nullopt;

    double traitCount = 0.0;
    double uses = 0.0;
    double capacity = 0.0;
    double rechargeable = 0.0;
    double chargeRate = 0.0;
    double range = 0.0;
    double duration = 0.0;
    double secondsBetweenUses = 0.0;
    if (!properties.ReadNumber(handle, "TraitCount", traitCount, error) ||
        !properties.ReadNumber(handle, "Uses", uses, error) ||
        !properties.ReadNumber(handle, "Capacity", capacity, error) ||
        !properties.ReadNumber(
            handle, "Rechargeable", rechargeable, error) ||
        !properties.ReadNumber(handle, "ChargeRate", chargeRate, error) ||
        !properties.ReadNumber(handle, "Range", range, error) ||
        !properties.ReadNumber(handle, "Duration", duration, error) ||
        !properties.ReadNumber(
            handle, "SecondsBetweenUses", secondsBetweenUses,
            error))
    {
        return std::nullopt;
    }

    snapshot.parameters.uses = static_cast<int>(uses);
    snapshot.parameters.capacity = static_cast<int>(capacity);
    snapshot.parameters.rechargeable = rechargeable > 0.5;
    snapshot.parameters.chargeRate = chargeRate;
    snapshot.parameters.range = range;
    snapshot.parameters.duration = duration;
    snapshot.parameters.secondsBetweenUses = secondsBetweenUses;
    snapshot.capabilities = GetCapabilities(
        snapshot.baseName,
        snapshot.parameters.range,
        snapshot.parameters.duration);

    if (snapshot.capabilities.supportsBlastRadius &&
        !properties.ReadNumber(
            handle, "BlastRadius", snapshot.parameters.blastRadius, error))
    {
        return std::nullopt;
    }
    if (snapshot.capabilities.supportsVisitorReturnTime &&
        !properties.ReadNumber(
            handle,
            "VisitorReturnTime",
            snapshot.parameters.visitorReturnTime,
            error))
    {
        return std::nullopt;
    }

    const int count = std::clamp(static_cast<int>(traitCount), 0, 64);
    snapshot.traits = ReadTraitList(properties, handle, count, error);
    if (!error.empty())
        return std::nullopt;

    snapshot.parameters.highCapacity =
        snapshot.parameters.capacity >= kHighCapacity;
    snapshot.parameters.selfCharging =
        snapshot.parameters.chargeRate > 0.0;
    if (snapshot.capabilities.supportsRangePreset)
    {
        const double standardDistance = std::abs(
            snapshot.parameters.range - snapshot.capabilities.standardRange);
        const double longDistance = std::abs(
            snapshot.parameters.range - snapshot.capabilities.longRange);
        const double extremeDistance = std::abs(
            snapshot.parameters.range - snapshot.capabilities.extremeRange);
        snapshot.parameters.rangeTier =
            longDistance < standardDistance &&
            longDistance <= extremeDistance
            ? GadgetRangeTier::Long
            : extremeDistance < standardDistance
            ? GadgetRangeTier::Extreme
            : GadgetRangeTier::Standard;
    }
    return snapshot;
}

std::optional<GadgetSnapshot> GadgetEditor::ApplyParameters(
    int handle,
    const GadgetParameters& parameters,
    std::string& error) const
{
    auto before = CaptureSnapshot(handle, error);
    if (!before)
        return std::nullopt;

    const GadgetCapabilities& capabilities = before->capabilities;
    if (parameters.uses < 0 || parameters.capacity < 0 ||
        !std::isfinite(parameters.chargeRate) ||
        parameters.chargeRate < 0.0 ||
        !std::isfinite(parameters.secondsBetweenUses) ||
        parameters.secondsBetweenUses < 0.0 ||
        (capabilities.supportsRangeAdvanced &&
         (!std::isfinite(parameters.range) || parameters.range < 0.0)) ||
        (capabilities.supportsDuration &&
         (!std::isfinite(parameters.duration) || parameters.duration < 0.0)) ||
        (capabilities.supportsBlastRadius &&
         (!std::isfinite(parameters.blastRadius) ||
          parameters.blastRadius < 0.0)) ||
        (capabilities.supportsVisitorReturnTime &&
         (!std::isfinite(parameters.visitorReturnTime) ||
          parameters.visitorReturnTime < 0.0)))
    {
        error = "Gadget advanced values must be finite and non-negative";
        return std::nullopt;
    }

    const GameMakerPropertyAccess properties(m_api);
    if (!properties.WriteReal(
            handle, "Uses", static_cast<double>(parameters.uses), error) ||
        !properties.WriteReal(
            handle,
            "Capacity",
            static_cast<double>(parameters.capacity),
            error) ||
        !properties.WriteReal(
            handle,
            "Rechargeable",
            parameters.rechargeable ? 1.0 : 0.0,
            error) ||
        !properties.WriteReal(
            handle, "ChargeRate", parameters.chargeRate, error) ||
        !properties.WriteReal(
            handle,
            "SecondsBetweenUses",
            parameters.secondsBetweenUses,
            error))
    {
        return std::nullopt;
    }
    if (capabilities.supportsRangeAdvanced &&
        !properties.WriteReal(handle, "Range", parameters.range, error))
    {
        return std::nullopt;
    }
    if (capabilities.supportsDuration &&
        !properties.WriteReal(handle, "Duration", parameters.duration, error))
    {
        return std::nullopt;
    }
    if (capabilities.supportsBlastRadius &&
        !properties.WriteReal(
            handle, "BlastRadius", parameters.blastRadius, error))
    {
        return std::nullopt;
    }
    if (capabilities.supportsVisitorReturnTime &&
        !properties.WriteReal(
            handle,
            "VisitorReturnTime",
            parameters.visitorReturnTime,
            error))
    {
        return std::nullopt;
    }

    static const std::unordered_set<std::string> kControlledTraits{
        "High Capacity", "Rechargeable", "Self-Charging", "Extreme Range",
        "Long Range"
    };
    std::vector<std::string> traits;
    for (const std::string& trait : before->traits)
    {
        if (kControlledTraits.find(trait) == kControlledTraits.end())
            traits.push_back(trait);
    }
    if (parameters.highCapacity)
        traits.emplace_back("High Capacity");
    if (parameters.selfCharging)
        traits.emplace_back("Self-Charging");
    else if (parameters.rechargeable)
        traits.emplace_back("Rechargeable");
    if (parameters.rangeTier == GadgetRangeTier::Long &&
        capabilities.supportsRangePreset)
    {
        traits.emplace_back("Long Range");
    }
    else if (parameters.rangeTier == GadgetRangeTier::Extreme &&
             capabilities.supportsRangePreset)
    {
        traits.emplace_back("Extreme Range");
    }

    for (size_t i = 0; i < traits.size(); ++i)
    {
        if (!properties.WriteString(
                handle,
                "Traits",
                traits[i],
                error,
                static_cast<int>(i)))
        {
            return std::nullopt;
        }
    }
    if (!properties.WriteReal(
            handle, "TraitCount", static_cast<double>(traits.size()), error))
    {
        return std::nullopt;
    }

    const std::string name = BuildGadgetDisplayName(
        before->name,
        parameters,
        capabilities);
    if (!properties.WriteString(handle, "Name", name, error))
        return std::nullopt;

    return CaptureSnapshot(handle, error);
}
