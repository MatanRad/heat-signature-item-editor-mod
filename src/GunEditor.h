#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ModInterface.h"

enum class GunLoudness
{
    Loud,
    Quiet,
    Silenced
};

enum class GunFireMode
{
    Normal,
    Quickfire,
    Automatic
};

struct GunParameters
{
    // These are the four independent choices exposed by the editor.
    // ApplyParameters() translates them into the correlated game fields and traits.
    bool         concussive = false;
    GunLoudness  loudness = GunLoudness::Loud;
    GunFireMode  fireMode = GunFireMode::Normal;
    bool         armourPiercing = false;
};

struct GunSnapshot
{
    int                      handle = 0;
    std::string              name;
    GunParameters            parameters;
    std::vector<std::string> traits;
};

class GunEditor
{
public:
    explicit GunEditor(const HS_ModApi* api) : m_api(api) {}

    // Reads a live gun into UI-owned state. Returns nullopt for stale handles
    // and non-gun items, with a user-facing explanation in error.
    std::optional<GunSnapshot> CaptureSnapshot(
        int handle,
        std::string& error) const;

    // Applies a complete parameter set, asks the game to rebuild gun visuals,
    // then returns a fresh snapshot of the resulting live object.
    std::optional<GunSnapshot> ApplyParameters(
        int handle,
        const GunParameters& parameters,
        std::string& error) const;

private:
    const HS_ModApi* m_api;
};
