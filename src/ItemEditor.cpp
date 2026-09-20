#include "ItemEditorWindow.h"
#include "ModInterface.h"

#include <imgui.h>
#include <windows.h>

#include <memory>
#include <string>

namespace
{
    constexpr SHORT kKeyCurrentlyDown = static_cast<SHORT>(0x8000);

    const HS_ModApi* g_api = nullptr;
    std::unique_ptr<ItemEditorWindow> g_editorWindow;

    bool IsControlDown()
    {
        return (GetAsyncKeyState(VK_CONTROL) & kKeyCurrentlyDown) != 0 ||
               (GetAsyncKeyState(VK_LCONTROL) & kKeyCurrentlyDown) != 0 ||
               (GetAsyncKeyState(VK_RCONTROL) & kKeyCurrentlyDown) != 0;
    }

    void Log(const std::string& message)
    {
        g_api->Log("ItemEditor", message.c_str());
    }

    void OnInventoryAssignment(
        const char*,
        CInstance*,
        CInstance*,
        RValue*,
        int argc,
        RValue** argv,
        void*)
    {
        if (!IsControlDown() || argc < 1 || !argv || !argv[0])
            return;

        const int handle =
            g_api->ResolveInstance(reinterpret_cast<uint32_t*>(argv[0]));
        std::string error;
        switch (g_editorWindow->OpenForItem(handle, error))
        {
        case ItemEditorWindow::OpenResult::Opened:
            g_api->SetImGuiVisible(1);
            g_api->RequestBypass();
            break;

        case ItemEditorWindow::OpenResult::UnsupportedItem:
            break;

        case ItemEditorWindow::OpenResult::Failed:
            Log(error);
            break;
        }
    }

    void OnInventoryItemStep(
        const char*,
        CInstance*,
        CInstance*,
        RValue*,
        int,
        RValue**,
        void*)
    {
        g_editorWindow->ProcessPendingChanges();
    }

    void OnImGuiDraw(void*)
    {
        g_editorWindow->Draw();
    }
}

HS_EXPORT_MOD_API_VERSION()

extern "C" __declspec(dllexport)
void ModInit(const HS_ModApi* api)
{
    g_api = api;
    g_editorWindow = std::make_unique<ItemEditorWindow>(api);

    void* alloc = nullptr;
    void* freeFn = nullptr;
    void* userData = nullptr;
    api->GetImGuiAllocators(&alloc, &freeFn, &userData);
    ImGui::SetAllocatorFunctions(
        reinterpret_cast<ImGuiMemAllocFunc>(alloc),
        reinterpret_cast<ImGuiMemFreeFunc>(freeFn),
        userData);
    ImGui::SetCurrentContext(
        reinterpret_cast<ImGuiContext*>(api->GetImGuiContext()));

    api->SubscribeHook(
        "gml_Script_AssignAsPrimaryItem",
        OnInventoryAssignment,
        nullptr);
    api->SubscribeHook(
        "gml_Script_AssignAsSecondaryItem",
        OnInventoryAssignment,
        nullptr);
    api->SubscribeHook(
        "gml_Script_InventoryItemStep",
        OnInventoryItemStep,
        nullptr);
    api->RegisterImGuiDraw(OnImGuiDraw, nullptr);

    Log("Loaded! Ctrl-click a gun in the inventory to edit it");
}
