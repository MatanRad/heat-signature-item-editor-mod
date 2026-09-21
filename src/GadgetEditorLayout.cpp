#include "GadgetEditorLayout.h"

#include <imgui.h>

#include <utility>

namespace
{
    constexpr int kHighCapacity = 5;
    constexpr int kStandardCapacity = 3;
    constexpr double kSelfChargingRate = 0.1;
}

GadgetEditorLayout::GadgetEditorLayout(const HS_ModApi* api)
    : m_editor(api)
{
}

ItemEditorLayout::CaptureResult GadgetEditorLayout::Capture(
    int handle,
    std::unique_ptr<ItemEditorLayout>& snapshot,
    std::string& error) const
{
    auto gadgetSnapshot = m_editor.CaptureSnapshot(handle, error);
    if (!gadgetSnapshot)
    {
        return error == "Selected item is not a supported gadget"
            ? CaptureResult::Unsupported
            : CaptureResult::Failed;
    }

    auto layout = std::make_unique<GadgetEditorLayout>(*this);
    layout->m_snapshot = std::move(*gadgetSnapshot);
    snapshot = std::move(layout);
    return CaptureResult::Captured;
}

std::unique_ptr<ItemEditorLayout> GadgetEditorLayout::Clone() const
{
    return std::make_unique<GadgetEditorLayout>(*this);
}

int GadgetEditorLayout::GetHandle() const
{
    return m_snapshot ? m_snapshot->handle : 0;
}

const char* GadgetEditorLayout::GetWindowTitle() const
{
    return "Item Editor - Gadget";
}

void GadgetEditorLayout::DrawControls(bool& changed)
{
    if (!m_snapshot)
        return;

    GadgetParameters& parameters = m_snapshot->parameters;
    const GadgetCapabilities& capabilities = m_snapshot->capabilities;
    ImGui::TextUnformatted(m_snapshot->name.c_str());
    ImGui::Text("Instance: %d", m_snapshot->handle);
    ImGui::TextDisabled("Changes apply automatically");
    ImGui::Separator();

    if (capabilities.supportsCapacityPreset &&
        ImGui::Checkbox("High capacity", &parameters.highCapacity))
    {
        parameters.uses =
            parameters.highCapacity ? kHighCapacity : kStandardCapacity;
        parameters.capacity =
            parameters.highCapacity ? kHighCapacity : kStandardCapacity;
        changed = true;
    }

    changed |= ImGui::Checkbox("Rechargeable", &parameters.rechargeable);

    if (ImGui::Checkbox("Self-charging", &parameters.selfCharging))
    {
        parameters.chargeRate =
            parameters.selfCharging ? kSelfChargingRate : 0.0;
        if (parameters.selfCharging)
            parameters.rechargeable = true;
        changed = true;
    }

    if (capabilities.supportsRangePreset)
    {
        int rangeTier = static_cast<int>(parameters.rangeTier);
        ImGui::TextUnformatted("Range");
        bool rangeChanged = ImGui::RadioButton("Standard", &rangeTier, 0);
        ImGui::SameLine();
        rangeChanged |= ImGui::RadioButton("Long", &rangeTier, 1);
        ImGui::SameLine();
        rangeChanged |= ImGui::RadioButton("Extreme", &rangeTier, 2);
        parameters.rangeTier = static_cast<GadgetRangeTier>(rangeTier);
        changed |= rangeChanged;
        if (rangeChanged)
        {
            parameters.range =
                parameters.rangeTier == GadgetRangeTier::Long
                ? capabilities.longRange
                : parameters.rangeTier == GadgetRangeTier::Extreme
                ? capabilities.extremeRange
                : capabilities.standardRange;
        }
    }

    ImGui::Separator();
    ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);
    if (ImGui::CollapsingHeader("Advanced options"))
    {
        changed |= ImGui::InputInt("Uses", &parameters.uses);
        changed |= ImGui::InputInt("Capacity", &parameters.capacity);
        changed |= ImGui::InputDouble(
            "Charge rate", &parameters.chargeRate, 0.0, 0.0, "%.3f");
        changed |= ImGui::InputDouble(
            "Seconds between uses",
            &parameters.secondsBetweenUses,
            0.0,
            0.0,
            "%.3f");
        if (capabilities.supportsRangeAdvanced)
        {
            changed |= ImGui::InputDouble(
                "Range", &parameters.range, 0.0, 0.0, "%.1f");
        }
        if (capabilities.supportsDuration)
        {
            changed |= ImGui::InputDouble(
                "Duration", &parameters.duration, 0.0, 0.0, "%.1f");
        }
        if (capabilities.supportsBlastRadius)
        {
            changed |= ImGui::InputDouble(
                "Blast radius",
                &parameters.blastRadius,
                0.0,
                0.0,
                "%.1f");
        }
        if (capabilities.supportsVisitorReturnTime)
        {
            changed |= ImGui::InputDouble(
                "Visitor return time",
                &parameters.visitorReturnTime,
                0.0,
                0.0,
                "%.1f");
        }

        if (parameters.uses < 0)
            parameters.uses = 0;
        if (parameters.capacity < 0)
            parameters.capacity = 0;
    }
}

bool GadgetEditorLayout::Apply(std::string& error)
{
    if (!m_snapshot)
    {
        error = "Gadget editor has no selected item";
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
