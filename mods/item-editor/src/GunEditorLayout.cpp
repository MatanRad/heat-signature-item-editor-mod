#include "GunEditorLayout.h"

#include <imgui.h>

#include <utility>

GunEditorLayout::GunEditorLayout(const HS_ModApi* api)
    : m_editor(api)
{
}

ItemEditorLayout::CaptureResult GunEditorLayout::Capture(
    int handle,
    std::unique_ptr<ItemEditorLayout>& snapshot,
    std::string& error) const
{
    auto gunSnapshot = m_editor.CaptureSnapshot(handle, error);
    if (!gunSnapshot)
    {
        return error == "Selected item is not a gun"
            ? CaptureResult::Unsupported
            : CaptureResult::Failed;
    }

    auto layout = std::make_unique<GunEditorLayout>(*this);
    layout->m_snapshot = std::move(*gunSnapshot);
    snapshot = std::move(layout);
    return CaptureResult::Captured;
}

std::unique_ptr<ItemEditorLayout> GunEditorLayout::Clone() const
{
    return std::make_unique<GunEditorLayout>(*this);
}

int GunEditorLayout::GetHandle() const
{
    return m_snapshot ? m_snapshot->handle : 0;
}

const char* GunEditorLayout::GetWindowTitle() const
{
    return "Item Editor - Gun";
}

void GunEditorLayout::DrawControls(bool& changed)
{
    if (!m_snapshot)
        return;

    GunParameters& parameters = m_snapshot->parameters;
    ImGui::TextUnformatted(m_snapshot->name.c_str());
    ImGui::Text("Instance: %d", m_snapshot->handle);
    ImGui::TextDisabled("Changes apply automatically");
    ImGui::Separator();

    int damageType = parameters.concussive ? 1 : 0;
    ImGui::TextUnformatted("Damage");
    bool damageTypeChanged = ImGui::RadioButton("Lethal", &damageType, 0);
    ImGui::SameLine();
    damageTypeChanged |= ImGui::RadioButton("Concussive", &damageType, 1);
    changed |= damageTypeChanged;
    parameters.concussive = damageType == 1;
    if (damageTypeChanged && parameters.concussive)
    {
        parameters.uses = 16;
        parameters.capacity = 16;
    }

    int loudness = static_cast<int>(parameters.loudness);
    ImGui::TextUnformatted("Sound");
    bool loudnessChanged = ImGui::RadioButton(
        "Loud", &loudness, static_cast<int>(GunLoudness::Loud));
    ImGui::SameLine();
    loudnessChanged |= ImGui::RadioButton(
        "Quiet", &loudness, static_cast<int>(GunLoudness::Quiet));
    ImGui::SameLine();
    loudnessChanged |= ImGui::RadioButton(
        "Silenced", &loudness, static_cast<int>(GunLoudness::Silenced));
    parameters.loudness = static_cast<GunLoudness>(loudness);
    changed |= loudnessChanged;
    if (loudnessChanged)
    {
        parameters.noise =
            parameters.loudness == GunLoudness::Silenced ? 0.05 : 0.6;
        parameters.audibleThroughWalls =
            parameters.loudness == GunLoudness::Loud;
    }

    int fireMode = static_cast<int>(parameters.fireMode);
    ImGui::TextUnformatted("Fire mode");
    bool fireModeChanged = ImGui::RadioButton(
        "Normal", &fireMode, static_cast<int>(GunFireMode::Normal));
    ImGui::SameLine();
    fireModeChanged |= ImGui::RadioButton(
        "Quickfire", &fireMode, static_cast<int>(GunFireMode::Quickfire));
    ImGui::SameLine();
    fireModeChanged |= ImGui::RadioButton(
        "Automatic", &fireMode, static_cast<int>(GunFireMode::Automatic));
    parameters.fireMode = static_cast<GunFireMode>(fireMode);
    changed |= fireModeChanged;
    if (fireModeChanged)
    {
        parameters.secondsBetweenFire =
            parameters.fireMode == GunFireMode::Automatic ? 0.1 :
            parameters.fireMode == GunFireMode::Quickfire ? 0.3 :
            2.0 / 3.0;
    }

    changed |= ImGui::Checkbox(
        "Armour-piercing", &parameters.armourPiercing);
    ImGui::Separator();

    ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);
    if (ImGui::CollapsingHeader("Advanced options"))
    {
        float noise = static_cast<float>(parameters.noise);
        if (ImGui::SliderFloat("Noise", &noise, 0.0f, 1.0f))
        {
            parameters.noise = noise;
            changed = true;
        }
        changed |= ImGui::Checkbox(
            "Audible through walls", &parameters.audibleThroughWalls);
        changed |= ImGui::InputDouble(
            "Seconds between fire",
            &parameters.secondsBetweenFire,
            0.0,
            0.0,
            "%.3f");

        if (parameters.concussive)
        {
            changed |= ImGui::InputInt("Uses", &parameters.uses);
            changed |= ImGui::InputInt("Capacity", &parameters.capacity);
            if (parameters.uses < 0)
                parameters.uses = 0;
            if (parameters.capacity < 0)
                parameters.capacity = 0;
        }

        changed |= ImGui::Checkbox(
            "Infinite ammo", &parameters.infiniteAmmo);
    }
}

bool GunEditorLayout::Apply(std::string& error)
{
    if (!m_snapshot)
    {
        error = "Gun editor has no selected item";
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
