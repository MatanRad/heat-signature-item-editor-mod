#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ItemEditorLayout.h"
#include "MeleeEditor.h"

class MeleeEditorLayout final : public ItemEditorLayout
{
public:
    explicit MeleeEditorLayout(const HS_ModApi* api);

    CaptureResult Capture(
        int handle,
        std::unique_ptr<ItemEditorLayout>& snapshot,
        std::string& error) const override;
    std::unique_ptr<ItemEditorLayout> Clone() const override;

    int GetHandle() const override;
    const char* GetWindowTitle() const override;
    void DrawControls(bool& changed) override;
    bool Apply(std::string& error) override;

private:
    MeleeEditor m_editor;
    std::optional<MeleeSnapshot> m_snapshot;
};
