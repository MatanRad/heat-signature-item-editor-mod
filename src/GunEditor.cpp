#include "GunEditor.h"
#include "GameMakerPropertyAccess.h"

#include <algorithm>
#include <cmath>
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
            error))
    {
        return std::nullopt;
    }

    const auto mask = static_cast<uint64_t>(damageMask);
    snapshot.parameters.concussive = (mask & 2u) != 0;
    snapshot.parameters.armourPiercing = (mask & 4u) != 0;
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
    const double secondsBetweenUses =
        parameters.fireMode == GunFireMode::Automatic ? 0.1 :
        parameters.fireMode == GunFireMode::Quickfire ? 0.3 :
        2.0 / 3.0;

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
            secondsBetweenUses,
            error))
    {
        return std::nullopt;
    }

    if (parameters.loudness == GunLoudness::Silenced)
    {
        if (!properties.WriteReal(handle, "Noise", 0.05, error) ||
            !properties.WriteReal(
                handle,
                "AudibleThroughWalls",
                0.0,
                error))
        {
            return std::nullopt;
        }
    }
    else if (!properties.WriteReal(handle, "Noise", 0.6, error) ||
             !properties.WriteReal(
                 handle,
                 "AudibleThroughWalls",
                 parameters.loudness == GunLoudness::Loud ? 1.0 : 0.0,
                 error))
    {
        return std::nullopt;
    }

    double capacity = -1.0;
    if (!properties.ReadNumber(handle, "Capacity", capacity, error))
        return std::nullopt;
    if (parameters.concussive)
    {
        if (capacity < 0.0 &&
            (!properties.WriteReal(handle, "Capacity", 16.0, error) ||
             !properties.WriteReal(handle, "Uses", 16.0, error)))
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
