#pragma once

#include <memory>
#include <string>

class ItemEditorLayout
{
public:
    enum class CaptureResult
    {
        Captured,
        Unsupported,
        Failed
    };

    virtual ~ItemEditorLayout() = default;

    // Captures a supported live item into an independent UI-owned layout.
    // Unsupported types allow the controller to try the next layout.
    virtual CaptureResult Capture(
        int handle,
        std::unique_ptr<ItemEditorLayout>& snapshot,
        std::string& error) const = 0;

    // Returns an independent copy for the render-thread and game-thread
    // mailboxes. Layout state must not reference live game data.
    virtual std::unique_ptr<ItemEditorLayout> Clone() const = 0;

    virtual int GetHandle() const = 0;
    virtual const char* GetWindowTitle() const = 0;
    virtual void DrawControls(bool& changed) = 0;

    // Applies this layout's complete draft on the game thread.
    virtual bool Apply(std::string& error) = 0;
};
