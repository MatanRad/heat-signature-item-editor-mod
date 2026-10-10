#include "GunEditor.h"
#include "GameMakerPropertyAccess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace
{
    /// Reads the logical prefix of the Traits array described by TraitCount.
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

    /// Tests whether a captured trait list contains an exact trait name.
    bool HasTrait(
        const std::vector<std::string>& traits,
        const char* expected)
    {
        return std::find(traits.begin(), traits.end(), expected) != traits.end();
    }

    /// Returns the GameMaker trait and display prefix for a loudness choice.
    std::string GetLoudnessTrait(GunLoudness loudness)
    {
        switch (loudness)
        {
        case GunLoudness::Loud:     return "Loud";
        case GunLoudness::Quiet:    return "Quiet";
        case GunLoudness::Silenced: return "Silenced";
        }
        return "Loud";
    }

    /// Converts an in-game number to the non-negative integer shown in the UI.
    int ToNonNegativeInteger(double value)
    {
        if (!std::isfinite(value) || value <= 0.0)
            return 0;

        const double maximum =
            static_cast<double>((std::numeric_limits<int>::max)());
        return static_cast<int>((std::min)(std::round(value), maximum));
    }

    /// Rebuilds a gun name while preserving adjectives not owned by the editor.
    std::string BuildGunDisplayName(
        const std::string& currentName,
        const std::string& fallbackBaseName,
        const GunParameters& parameters)
    {
        // Remove only prefixes controlled by this editor. Other generated
        // adjectives remain part of the weapon's name.
        static const std::unordered_set<std::string> kControlledWords{
            "Loud", "Quiet", "Silenced", "Quickfire", "Automatic",
            "Concussive", "Armour-Piercing", "Armor-Piercing"
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

        std::string baseName = base.str();
        if (baseName.empty())
            baseName = fallbackBaseName;

        std::ostringstream result;
        result << GetLoudnessTrait(parameters.loudness) << ' ';
        if (parameters.fireMode == GunFireMode::Quickfire)
            result << "Quickfire ";
        else if (parameters.fireMode == GunFireMode::Automatic)
            result << "Automatic ";
        if (parameters.concussive)
            result << "Concussive ";
        if (parameters.armourPiercing)
            result << "Armour-Piercing ";
        result << baseName;
        return result.str();
    }
}

/// Captures the editable state of a live gun instance.
///
/// The function rejects stale handles and non-gun items. On failure it returns
/// std::nullopt and describes the failed validation or property read in error.
std::optional<GunSnapshot> GunEditor::CaptureSnapshot(
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
    std::string type;
    if (!properties.ReadString(handle, "Type", type, error))
        return std::nullopt;
    if (type != "Gun")
    {
        error = "Selected item is not a gun";
        return std::nullopt;
    }

    GunSnapshot snapshot;
    snapshot.handle = handle;
    if (!properties.ReadString(handle, "Name", snapshot.name, error))
        return std::nullopt;

    double traitCount = 0.0;
    if (!properties.ReadNumber(handle, "TraitCount", traitCount, error))
        return std::nullopt;
    const int count = std::clamp(static_cast<int>(traitCount), 0, 64);
    snapshot.traits = ReadTraitList(properties, handle, count, error);
    if (!error.empty())
        return std::nullopt;

    double damageMask = 0.0;
    double noise = 0.0;
    double audibleThroughWalls = 0.0;
    double secondsBetweenUses = 0.0;
    double uses = 0.0;
    double capacity = 0.0;
    double chargeRate = 0.0;
    if (!properties.ReadNumber(
            handle,
            "WeaponDamageMask",
            damageMask,
            error) ||
        !properties.ReadNumber(handle, "Noise", noise, error) ||
        !properties.ReadNumber(
            handle,
            "AudibleThroughWalls",
            audibleThroughWalls,
            error) ||
        !properties.ReadNumber(
            handle,
            "SecondsBetweenUses",
            secondsBetweenUses,
            error) ||
        !properties.ReadNumber(handle, "ChargeRate", chargeRate, error))
    {
        return std::nullopt;
    }

    const auto mask = static_cast<uint64_t>(damageMask);
    snapshot.parameters.concussive = (mask & 2u) != 0;
    snapshot.parameters.armourPiercing = (mask & 4u) != 0;
    if (snapshot.parameters.concussive &&
        (!properties.ReadNumber(handle, "Uses", uses, error) ||
         !properties.ReadNumber(handle, "Capacity", capacity, error)))
    {
        return std::nullopt;
    }
    snapshot.parameters.noise = noise;
    snapshot.parameters.audibleThroughWalls = audibleThroughWalls > 0.5;
    snapshot.parameters.secondsBetweenFire = secondsBetweenUses;
    if (snapshot.parameters.concussive)
    {
        snapshot.parameters.uses = ToNonNegativeInteger(uses);
        snapshot.parameters.capacity = capacity < 0.0
            ? 16
            : ToNonNegativeInteger(capacity);
    }
    snapshot.parameters.infiniteAmmo = chargeRate >= 99.999;
    if (noise <= 0.05 + 0.0001)
        snapshot.parameters.loudness = GunLoudness::Silenced;
    else if (audibleThroughWalls > 0.5)
        snapshot.parameters.loudness = GunLoudness::Loud;
    else
        snapshot.parameters.loudness = GunLoudness::Quiet;

    if (std::abs(secondsBetweenUses - 0.1) < 0.01 ||
        HasTrait(snapshot.traits, "Extreme Rapid Fire"))
    {
        snapshot.parameters.fireMode = GunFireMode::Automatic;
    }
    else if (std::abs(secondsBetweenUses - 0.3) < 0.01 ||
             HasTrait(snapshot.traits, "Rapid Fire"))
    {
        snapshot.parameters.fireMode = GunFireMode::Quickfire;
    }
    else
    {
        snapshot.parameters.fireMode = GunFireMode::Normal;
    }

    return snapshot;
}

/// Applies one complete set of gun parameters to a live instance.
///
/// Related runtime fields and controlled traits are updated together, after
/// which SetGunSprites recalculates visual state and the display name is
/// rebuilt. The returned snapshot reflects the final game state.
std::optional<GunSnapshot> GunEditor::ApplyParameters(
    int handle,
    const GunParameters& parameters,
    std::string& error) const
{
    auto before = CaptureSnapshot(handle, error);
    if (!before)
        return std::nullopt;

    const GameMakerPropertyAccess properties(m_api);
    const int64_t damageMask =
        (parameters.concussive ? 2u : 1u) |
        (parameters.armourPiercing ? 4u : 0u);
    if (!std::isfinite(parameters.noise) ||
        parameters.noise < 0.0 ||
        parameters.noise > 1.0)
    {
        error = "Noise must be between 0 and 1";
        return std::nullopt;
    }
    if (!std::isfinite(parameters.secondsBetweenFire) ||
        parameters.secondsBetweenFire < 0.0)
    {
        error = "Seconds between fire must not be negative";
        return std::nullopt;
    }
    if (parameters.concussive &&
        (parameters.uses < 0 || parameters.capacity < 0))
    {
        error = "Ammo, uses, and capacity must not be negative";
        return std::nullopt;
    }

    if (!properties.WriteInt64(
            handle,
            "WeaponDamageMask",
            damageMask,
            error) ||
        !properties.WriteReal(
            handle,
            "AmmoType",
            parameters.armourPiercing ? 1.0 : 0.0,
            error) ||
        !properties.WriteReal(
            handle,
            "SecondsBetweenUses",
            parameters.secondsBetweenFire,
            error) ||
        !properties.WriteReal(handle, "Noise", parameters.noise, error) ||
        !properties.WriteReal(
            handle,
            "AudibleThroughWalls",
            parameters.audibleThroughWalls ? 1.0 : 0.0,
            error) ||
        !properties.WriteReal(
            handle,
            "ChargeRate",
            parameters.infiniteAmmo ? 100.0 : 0.0,
            error))
    {
        return std::nullopt;
    }

    if (parameters.concussive)
    {
        if (!properties.WriteReal(
                handle,
                "Capacity",
                static_cast<double>(parameters.capacity),
                error) ||
            !properties.WriteReal(
                handle,
                "Uses",
                static_cast<double>(parameters.uses),
                error))
        {
            return std::nullopt;
        }
        if (!properties.WriteReal(handle, "Rechargeable", 1.0, error))
            return std::nullopt;
    }
    else if (!properties.WriteReal(handle, "Capacity", -1.0, error) ||
             !properties.WriteReal(handle, "Rechargeable", 0.0, error))
    {
        return std::nullopt;
    }

    // Preserve unrelated generation traits while replacing the four groups
    // represented by GunParameters.
    static const std::unordered_set<std::string> kControlledTraits{
        "Lethal", "Concussive", "Loud", "Quiet", "Silenced",
        "Rapid Fire", "Extreme Rapid Fire", "Ignores Armour"
    };
    std::vector<std::string> traits;
    for (const std::string& trait : before->traits)
    {
        if (kControlledTraits.find(trait) == kControlledTraits.end())
            traits.push_back(trait);
    }
    traits.push_back(parameters.concussive ? "Concussive" : "Lethal");
    traits.push_back(GetLoudnessTrait(parameters.loudness));
    if (parameters.fireMode == GunFireMode::Quickfire)
        traits.emplace_back("Rapid Fire");
    else if (parameters.fireMode == GunFireMode::Automatic)
        traits.emplace_back("Extreme Rapid Fire");
    if (parameters.armourPiercing)
        traits.emplace_back("Ignores Armour");

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
            handle,
            "TraitCount",
            static_cast<double>(traits.size()),
            error))
    {
        return std::nullopt;
    }

    // SetGunSprites reads the normalized trait array and updates sprite_index,
    // BaseName, held attachment, and animation fields. The display name is
    // rebuilt afterwards so the script cannot overwrite the editor's prefixes.
    CInstance* instance = m_api->ResolveCInstance(handle);
    if (!instance)
    {
        error = "Gun was destroyed while applying changes";
        return std::nullopt;
    }
    RValue scriptResult{};
    m_api->CallScript(
        "gml_Script_SetGunSprites",
        instance,
        instance,
        &scriptResult,
        0,
        nullptr);

    std::string baseName;
    if (!properties.ReadString(handle, "BaseName", baseName, error))
        return std::nullopt;
    const std::string name =
        BuildGunDisplayName(before->name, baseName, parameters);
    if (!properties.WriteString(handle, "Name", name, error))
        return std::nullopt;

    return CaptureSnapshot(handle, error);
}
