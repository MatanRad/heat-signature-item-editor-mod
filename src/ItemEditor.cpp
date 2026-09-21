#include "ItemEditorWindow.h"
#include "GameMakerPropertyAccess.h"
#include "GmArgs.h"
#include "ModInterface.h"

#include <imgui.h>
#include <windows.h>

#include <cmath>
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

    bool IsOwnedByDailyChallenger(int itemHandle, std::string& error)
    {
        const GameMakerPropertyAccess properties(g_api);
        double owner = 0.0;
        if (!properties.ReadNumber(itemHandle, "Owner", owner, error))
            return false;

        if (!std::isfinite(owner) || owner <= 0.0)
        {
            error = "Could not verify the selected item's owner";
            return false;
        }

        const int ownerHandle = static_cast<int>(std::llround(owner));
        double dailyChallenge = 0.0;
        if (!properties.ReadNumber(
                ownerHandle,
                "DailyChallenge",
                dailyChallenge,
                error))
        {
            return false;
        }

        return dailyChallenge > 0.0;
    }

    void ShowDailyChallengeBlockedMessage(
        CInstance* self,
        CInstance* other)
    {
        if (!self || !other)
            return;

        GmArgs args;
        args.AddStr(
            g_api,
            "Item Editor is disabled during Daily Challenges.");
        args.AddReal(3.0);

        RValue result{};
        g_api->CallScript(
            "gml_Script_ShowUpdate",
            self,
            other,
            &result,
            args.Count(),
            args.Build());
    }

    void OnInventoryAssignment(
        const char*,
        CInstance* self,
        CInstance* other,
        RValue*,
        int argc,
        RValue** argv,
        void*)
    {
        if (!IsControlDown() || argc < 1 || !argv || !argv[0])
            return;

        const int handle =
            g_api->ResolveInstance(reinterpret_cast<uint32_t*>(argv[0]));
        if (handle == 0)
            return;

        std::string error;
        if (IsOwnedByDailyChallenger(handle, error))
        {
            Log("Item editor is disabled for Daily Challenge characters");
            ShowDailyChallengeBlockedMessage(self, other);
            return;
        }
        if (!error.empty())
        {
            Log(std::string(
                "Item editor is disabled because Daily Challenge status "
                "could not be verified: ") + error);
            return;
        }

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
        if (g_editorWindow->TakeCloseRequest())
            g_api->SetImGuiVisible(0);
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
