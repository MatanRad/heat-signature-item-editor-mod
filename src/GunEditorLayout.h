#pragma once

#include <memory>
#include <optional>
#include <string>

#include "GunEditor.h"
#include "ItemEditorLayout.h"

class GunEditorLayout final : public ItemEditorLayout
{
public:
    explicit GunEditorLayout(const HS_ModApi* api);

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
    GunEditor m_editor;
    std::optional<GunSnapshot> m_snapshot;
};
