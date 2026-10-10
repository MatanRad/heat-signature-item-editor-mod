#include "MeleeEditor.h"

#include "GameMakerPropertyAccess.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace
{
    constexpr double kShortAimRange = 64.0;
    constexpr double kLongAimRange = 256.0;
    constexpr double kQuickRecovery = 0.1;
    constexpr double kSlowRecovery = 0.4;
    constexpr double kQuietStrikeRadius = 48.0;
    constexpr double kStandardStrikeRadius = 150.0;
    constexpr double kLightKnockback = 100.0;
    constexpr double kHeavyKnockback = 300.0;
    constexpr double kExtremeKnockback = 1000.0;

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

    template <typename T>
    T ClosestPreset(
        double value,
        T first,
        double firstValue,
        T second,
        double secondValue,
        T third,
        double thirdValue)
    {
        const double firstDistance = std::abs(value - firstValue);
        const double secondDistance = std::abs(value - secondValue);
        const double thirdDistance = std::abs(value - thirdValue);
        if (firstDistance <= secondDistance && firstDistance <= thirdDistance)
            return first;
        if (secondDistance <= thirdDistance)
            return second;
        return third;
    }

    std::string BuildMeleeDisplayName(
        const std::string& currentName,
        const MeleeParameters& parameters)
    {
        std::istringstream input(currentName);
        std::ostringstream base;
        std::string word;
        while (input >> word)
        {
            if (word == "Armour-Piercing" || word == "Armor-Piercing" ||
                word == "Concussion")
                continue;
            if (base.tellp() > 0)
                base << ' ';
            base << word;
        }

        std::string name;
        if (parameters.armourPiercing)
            name = "Armour-Piercing ";
        if (parameters.concussive)
            name += "Concussion ";
        return name + base.str();
    }
}

std::optional<MeleeSnapshot> MeleeEditor::CaptureSnapshot(
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
    std::string attachmentSlot;
    if (!properties.ReadString(handle, "AttachmentSlot", attachmentSlot, error))
        return std::nullopt;
    if (attachmentSlot != "Wrench")
    {
        error = "Selected item is not a melee weapon";
        return std::nullopt;
    }

    MeleeSnapshot snapshot;
    snapshot.handle = handle;
    if (!properties.ReadString(handle, "Name", snapshot.name, error))
        return std::nullopt;

    double traitCount = 0.0;
    double damageMask = 0.0;
    double autoDash = 0.0;
    if (!properties.ReadNumber(handle, "TraitCount", traitCount, error) ||
        !properties.ReadNumber(handle, "WeaponDamageMask", damageMask, error) ||
        !properties.ReadNumber(
            handle, "AimRange", snapshot.parameters.aimRange, error) ||
        !properties.ReadNumber(handle, "AutoDash", autoDash, error) ||
        !properties.ReadNumber(
            handle,
            "SecondsBetweenUses",
            snapshot.parameters.secondsBetweenUses,
            error) ||
        !properties.ReadNumber(
            handle,
            "HitHumanSoundRadius",
            snapshot.parameters.hitHumanSoundRadius,
            error) ||
        !properties.ReadNumber(
            handle,
            "KnockbackSpeed",
            snapshot.parameters.knockbackSpeed,
            error) ||
        !properties.ReadNumber(
            handle, "DashSpeed", snapshot.parameters.dashSpeed, error))
    {
        return std::nullopt;
    }

    const int count = std::clamp(static_cast<int>(traitCount), 0, 64);
    snapshot.traits = ReadTraitList(properties, handle, count, error);
    if (!error.empty())
        return std::nullopt;

    const uint64_t mask = static_cast<uint64_t>(damageMask);
    snapshot.parameters.concussive = (mask & 2u) != 0;
    snapshot.parameters.armourPiercing = (mask & 4u) != 0;
    snapshot.parameters.autoDash = autoDash > 0.5;
    snapshot.parameters.dashRange =
        std::abs(snapshot.parameters.aimRange - kShortAimRange) <
        std::abs(snapshot.parameters.aimRange - kLongAimRange)
        ? MeleeDashRange::Short
        : MeleeDashRange::Long;
    snapshot.parameters.recovery =
        std::abs(snapshot.parameters.secondsBetweenUses - kQuickRecovery) <
        std::abs(snapshot.parameters.secondsBetweenUses - kSlowRecovery)
        ? MeleeRecovery::Quick
        : MeleeRecovery::Slow;
    snapshot.parameters.strikeNoise =
        std::abs(
            snapshot.parameters.hitHumanSoundRadius - kQuietStrikeRadius) <
        std::abs(
            snapshot.parameters.hitHumanSoundRadius - kStandardStrikeRadius)
        ? MeleeStrikeNoise::Quiet
        : MeleeStrikeNoise::Standard;
    snapshot.parameters.knockback = ClosestPreset(
        snapshot.parameters.knockbackSpeed,
        MeleeKnockback::Light,
        kLightKnockback,
        MeleeKnockback::Heavy,
        kHeavyKnockback,
        MeleeKnockback::Extreme,
        kExtremeKnockback);
    return snapshot;
}

std::optional<MeleeSnapshot> MeleeEditor::ApplyParameters(
    int handle,
    const MeleeParameters& parameters,
    std::string& error) const
{
    auto before = CaptureSnapshot(handle, error);
    if (!before)
        return std::nullopt;

    const double values[] = {
        parameters.aimRange,
        parameters.secondsBetweenUses,
        parameters.hitHumanSoundRadius,
        parameters.knockbackSpeed,
        parameters.dashSpeed
    };
    for (double value : values)
    {
        if (!std::isfinite(value) || value < 0.0)
        {
            error = "Melee advanced values must be finite and non-negative";
            return std::nullopt;
        }
    }

    const GameMakerPropertyAccess properties(m_api);
    const int64_t damageMask =
        (parameters.concussive ? 2u : 1u) |
        (parameters.armourPiercing ? 4u : 0u);
    if (!properties.WriteInt64(
            handle, "WeaponDamageMask", damageMask, error) ||
        !properties.WriteReal(handle, "AimRange", parameters.aimRange, error) ||
        !properties.WriteReal(
            handle, "AutoDash", parameters.autoDash ? 1.0 : 0.0, error) ||
        !properties.WriteReal(
            handle,
            "SecondsBetweenUses",
            parameters.secondsBetweenUses,
            error) ||
        !properties.WriteReal(
            handle,
            "HitHumanSoundRadius",
            parameters.hitHumanSoundRadius,
            error) ||
        !properties.WriteReal(
            handle,
            "KnockbackSpeed",
            parameters.knockbackSpeed,
            error) ||
        !properties.WriteReal(
            handle, "DashSpeed", parameters.dashSpeed, error))
    {
        return std::nullopt;
    }

    static const std::unordered_set<std::string> kControlledTraits{
        "Lethal", "Concussive", "Ignores Armour", "Short Dash Range",
        "Long Dash Range", "Quick Recovery", "Slow Recovery", "Quiet Strike"
    };
    std::vector<std::string> traits;
    for (const std::string& trait : before->traits)
    {
        if (kControlledTraits.find(trait) == kControlledTraits.end())
            traits.push_back(trait);
    }
    traits.emplace_back(parameters.concussive ? "Concussive" : "Lethal");
    if (parameters.armourPiercing)
        traits.emplace_back("Ignores Armour");
    traits.emplace_back(
        parameters.dashRange == MeleeDashRange::Short
        ? "Short Dash Range"
        : "Long Dash Range");
    traits.emplace_back(
        parameters.recovery == MeleeRecovery::Quick
        ? "Quick Recovery"
        : "Slow Recovery");
    if (parameters.strikeNoise == MeleeStrikeNoise::Quiet)
        traits.emplace_back("Quiet Strike");

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

    const std::string name = BuildMeleeDisplayName(before->name, parameters);
    if (!properties.WriteString(handle, "Name", name, error))
        return std::nullopt;

    return CaptureSnapshot(handle, error);
}
