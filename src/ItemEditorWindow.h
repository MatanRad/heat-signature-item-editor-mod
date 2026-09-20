#pragma once

#include <memory>
#include <string>

#include "ModInterface.h"

class ItemEditorWindow
{
public:
    enum class OpenResult
    {
        Opened,
        UnsupportedItem,
        Failed
    };

    explicit ItemEditorWindow(const HS_ModApi* api);
    ~ItemEditorWindow();

    ItemEditorWindow(const ItemEditorWindow&) = delete;
    ItemEditorWindow& operator=(const ItemEditorWindow&) = delete;

    /// Selects a live item and opens a fresh editing session for it.
    OpenResult OpenForItem(int handle, std::string& error);

    /// Applies the newest UI request from a GameMaker game-thread hook.
    void ProcessPendingChanges();

    /// Draws and updates the editor on the ImGui render thread.
    void Draw();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
