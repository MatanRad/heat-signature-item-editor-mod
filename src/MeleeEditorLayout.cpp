#include "MeleeEditorLayout.h"

#include <imgui.h>

#include <utility>

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
}

MeleeEditorLayout::MeleeEditorLayout(const HS_ModApi* api)
    : m_editor(api)
{
}

ItemEditorLayout::CaptureResult MeleeEditorLayout::Capture(
    int handle,
    std::unique_ptr<ItemEditorLayout>& snapshot,
    std::string& error) const
{
    auto meleeSnapshot = m_editor.CaptureSnapshot(handle, error);
    if (!meleeSnapshot)
    {
        return error == "Selected item is not a melee weapon"
            ? CaptureResult::Unsupported
            : CaptureResult::Failed;
    }

    auto layout = std::make_unique<MeleeEditorLayout>(*this);
    layout->m_snapshot = std::move(*meleeSnapshot);
    snapshot = std::move(layout);
    return CaptureResult::Captured;
}

std::unique_ptr<ItemEditorLayout> MeleeEditorLayout::Clone() const
{
    return std::make_unique<MeleeEditorLayout>(*this);
}

int MeleeEditorLayout::GetHandle() const
{
    return m_snapshot ? m_snapshot->handle : 0;
}

const char* MeleeEditorLayout::GetWindowTitle() const
{
    return "Item Editor - Melee";
}

void MeleeEditorLayout::DrawControls(bool& changed)
{
    if (!m_snapshot)
        return;

    MeleeParameters& parameters = m_snapshot->parameters;
    ImGui::TextUnformatted(m_snapshot->name.c_str());
    ImGui::Text("Instance: %d", m_snapshot->handle);
    ImGui::TextDisabled("Changes apply automatically");
    ImGui::Separator();

    int damageType = parameters.concussive ? 1 : 0;
    ImGui::TextUnformatted("Damage");
    bool damageChanged = ImGui::RadioButton("Lethal", &damageType, 0);
    ImGui::SameLine();
    damageChanged |= ImGui::RadioButton("Concussive", &damageType, 1);
    parameters.concussive = damageType == 1;
    changed |= damageChanged;

    changed |= ImGui::Checkbox(
        "Armour-piercing", &parameters.armourPiercing);

    int dashRange = static_cast<int>(parameters.dashRange);
    ImGui::TextUnformatted("Dash range");
    bool dashRangeChanged = ImGui::RadioButton(
        "Short", &dashRange, static_cast<int>(MeleeDashRange::Short));
    ImGui::SameLine();
    dashRangeChanged |= ImGui::RadioButton(
        "Long", &dashRange, static_cast<int>(MeleeDashRange::Long));
    parameters.dashRange = static_cast<MeleeDashRange>(dashRange);
    changed |= dashRangeChanged;
    if (dashRangeChanged)
    {
        parameters.aimRange =
            parameters.dashRange == MeleeDashRange::Short
            ? kShortAimRange
            : kLongAimRange;
    }

    bool autoDash = parameters.autoDash;
    if (ImGui::Checkbox("Auto-strike", &autoDash))
    {
        parameters.autoDash = autoDash;
        changed = true;
    }

    int recovery = static_cast<int>(parameters.recovery);
    ImGui::TextUnformatted("Recovery");
    bool recoveryChanged = ImGui::RadioButton(
        "Quick", &recovery, static_cast<int>(MeleeRecovery::Quick));
    ImGui::SameLine();
    recoveryChanged |= ImGui::RadioButton(
        "Slow", &recovery, static_cast<int>(MeleeRecovery::Slow));
    parameters.recovery = static_cast<MeleeRecovery>(recovery);
    changed |= recoveryChanged;
    if (recoveryChanged)
    {
        parameters.secondsBetweenUses =
            parameters.recovery == MeleeRecovery::Quick
            ? kQuickRecovery
            : kSlowRecovery;
    }

    int strikeNoise = static_cast<int>(parameters.strikeNoise);
    ImGui::TextUnformatted("Strike noise");
    bool strikeNoiseChanged = ImGui::RadioButton(
        "Quiet", &strikeNoise, static_cast<int>(MeleeStrikeNoise::Quiet));
    ImGui::SameLine();
    strikeNoiseChanged |= ImGui::RadioButton(
        "Standard",
        &strikeNoise,
        static_cast<int>(MeleeStrikeNoise::Standard));
    parameters.strikeNoise = static_cast<MeleeStrikeNoise>(strikeNoise);
    changed |= strikeNoiseChanged;
    if (strikeNoiseChanged)
    {
        parameters.hitHumanSoundRadius =
            parameters.strikeNoise == MeleeStrikeNoise::Quiet
            ? kQuietStrikeRadius
            : kStandardStrikeRadius;
    }

    int knockback = static_cast<int>(parameters.knockback);
    ImGui::TextUnformatted("Knockback");
    bool knockbackChanged = ImGui::RadioButton(
        "Light", &knockback, static_cast<int>(MeleeKnockback::Light));
    ImGui::SameLine();
    knockbackChanged |= ImGui::RadioButton(
        "Heavy", &knockback, static_cast<int>(MeleeKnockback::Heavy));
    ImGui::SameLine();
    knockbackChanged |= ImGui::RadioButton(
        "Extreme", &knockback, static_cast<int>(MeleeKnockback::Extreme));
    parameters.knockback = static_cast<MeleeKnockback>(knockback);
    changed |= knockbackChanged;
    if (knockbackChanged)
    {
        parameters.knockbackSpeed =
            parameters.knockback == MeleeKnockback::Light
            ? kLightKnockback
            : parameters.knockback == MeleeKnockback::Heavy
            ? kHeavyKnockback
            : kExtremeKnockback;
    }

    ImGui::Separator();
    ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);
    if (ImGui::CollapsingHeader("Advanced options"))
    {
        changed |= ImGui::InputDouble(
            "Custom dash range", &parameters.aimRange, 0.0, 0.0, "%.1f");
        changed |= ImGui::InputDouble(
            "Seconds between strikes",
            &parameters.secondsBetweenUses,
            0.0,
            0.0,
            "%.3f");
        changed |= ImGui::InputDouble(
            "Hit sound radius",
            &parameters.hitHumanSoundRadius,
            0.0,
            0.0,
            "%.1f");
        changed |= ImGui::InputDouble(
            "Custom knockback speed",
            &parameters.knockbackSpeed,
            0.0,
            0.0,
            "%.1f");
        changed |= ImGui::InputDouble(
            "Dash speed", &parameters.dashSpeed, 0.0, 0.0, "%.1f");
    }
}

bool MeleeEditorLayout::Apply(std::string& error)
{
    if (!m_snapshot)
    {
        error = "Melee editor has no selected item";
        return false;
    }

    auto updated = m_editor.ApplyParameters(
        m_snapshot->handle,
        m_snapshot->parameters,
        error);
    if (!updated)
        return false;

    m_snapshot = std::move(*updated);
    return true;
}
