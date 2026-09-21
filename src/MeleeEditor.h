#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ModInterface.h"

enum class MeleeDashRange
{
    Short,
    Long
};

enum class MeleeRecovery
{
    Quick,
    Slow
};

enum class MeleeStrikeNoise
{
    Quiet,
    Standard
};

enum class MeleeKnockback
{
    Light,
    Heavy,
    Extreme
};

struct MeleeParameters
{
    bool              concussive = false;
    bool              armourPiercing = false;
    MeleeDashRange    dashRange = MeleeDashRange::Long;
    bool              autoDash = false;
    MeleeRecovery     recovery = MeleeRecovery::Slow;
    MeleeStrikeNoise  strikeNoise = MeleeStrikeNoise::Standard;
    MeleeKnockback    knockback = MeleeKnockback::Light;
    double            aimRange = 256.0;
    double            secondsBetweenUses = 0.4;
    double            hitHumanSoundRadius = 150.0;
    double            knockbackSpeed = 100.0;
    double            dashSpeed = 3000.0;
};

struct MeleeSnapshot
{
    int                      handle = 0;
    std::string              name;
    MeleeParameters          parameters;
    std::vector<std::string> traits;
};

class MeleeEditor
{
public:
    explicit MeleeEditor(const HS_ModApi* api) : m_api(api) {}

    std::optional<MeleeSnapshot> CaptureSnapshot(
        int handle,
        std::string& error) const;

    std::optional<MeleeSnapshot> ApplyParameters(
        int handle,
        const MeleeParameters& parameters,
        std::string& error) const;

private:
    const HS_ModApi* m_api;
};
