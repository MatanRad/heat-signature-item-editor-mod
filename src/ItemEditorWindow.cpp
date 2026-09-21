#include "ItemEditorWindow.h"

#include "GunEditorLayout.h"
#include "GadgetEditorLayout.h"
#include "ItemEditorLayout.h"
#include "MeleeEditorLayout.h"

#include <imgui.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class ItemEditorWindow::Impl
{
public:
    explicit Impl(const HS_ModApi* api)
        : m_api(api)
    {
        m_layouts.push_back(std::make_unique<GunEditorLayout>(api));
        m_layouts.push_back(std::make_unique<MeleeEditorLayout>(api));
        m_layouts.push_back(std::make_unique<GadgetEditorLayout>(api));
    }

    bool TakeCloseRequest()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const bool closeRequested = m_state.closeRequested;
        m_state.closeRequested = false;
        return closeRequested;
    }

    OpenResult OpenForItem(int handle, std::string& error)
    {
        error.clear();
        for (const auto& layout : m_layouts)
        {
            std::unique_ptr<ItemEditorLayout> snapshot;
            const ItemEditorLayout::CaptureResult result =
                layout->Capture(handle, snapshot, error);
            if (result == ItemEditorLayout::CaptureResult::Captured)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state.Open(std::move(snapshot));
                return OpenResult::Opened;
            }

            if (result == ItemEditorLayout::CaptureResult::Failed)
                return OpenResult::Failed;
        }

        return OpenResult::UnsupportedItem;
    }

    void ProcessPendingChanges()
    {
        std::unique_ptr<ItemEditorLayout> pending;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pending = m_state.TakePendingUpdate();
        }
        if (!pending)
            return;

        const int handle = pending->GetHandle();
        std::string error;
        const bool succeeded = pending->Apply(error);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.CompleteUpdate(handle, std::move(pending), succeeded, error);
        if (!succeeded && !error.empty())
            m_api->Log("ItemEditor", error.c_str());
    }

    void Draw()
    {
        std::unique_ptr<ItemEditorLayout> local;
        bool resetLayout = false;
        std::string status;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_state.open || !m_state.draft)
                return;
            local = m_state.draft->Clone();
            resetLayout = m_state.resetLayout;
            status = m_state.status;
        }

        const float uiScale = CalculateUiScale();
        bool open = true;
        bool changed = false;

        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            Scale(style.WindowPadding, uiScale));
        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            Scale(style.FramePadding, uiScale));
        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            Scale(style.ItemSpacing, uiScale));
        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemInnerSpacing,
            Scale(style.ItemInnerSpacing, uiScale));

        if (resetLayout)
        {
            const ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        }
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(360.0f * uiScale, 0.0f),
            ImVec2(650.0f * uiScale, 1000.0f * uiScale));

        if (ImGui::Begin(
                local->GetWindowTitle(),
                &open,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetWindowFontScale(uiScale);
            local->DrawControls(changed);
            if (!status.empty())
                ImGui::TextWrapped("%s", status.c_str());
        }
        ImGui::End();
        ImGui::PopStyleVar(4);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.CommitFrame(std::move(local), open, changed);
    }

private:
    struct EditorState
    {
        bool open = false;
        bool resetLayout = false;
        std::unique_ptr<ItemEditorLayout> selected;
        std::unique_ptr<ItemEditorLayout> draft;
        std::unique_ptr<ItemEditorLayout> pendingUpdate;
        std::string status;
        bool closeRequested = false;

        void Reset()
        {
            open = false;
            resetLayout = false;
            selected.reset();
            draft.reset();
            pendingUpdate.reset();
            status.clear();
        }

        void Open(std::unique_ptr<ItemEditorLayout> snapshot)
        {
            Reset();
            open = true;
            resetLayout = true;
            draft = snapshot->Clone();
            selected = std::move(snapshot);
        }

        void Close()
        {
            open = false;
            resetLayout = false;
            closeRequested = true;
        }

        std::unique_ptr<ItemEditorLayout> TakePendingUpdate()
        {
            return std::move(pendingUpdate);
        }

        void CompleteUpdate(
            int expectedHandle,
            std::unique_ptr<ItemEditorLayout> updated,
            bool succeeded,
            const std::string& error)
        {
            if (!selected || selected->GetHandle() != expectedHandle)
                return;

            if (!succeeded)
            {
                status = error;
                return;
            }

            selected = std::move(updated);
            if (!pendingUpdate)
                draft = selected->Clone();
            status.clear();
        }

        void CommitFrame(
            std::unique_ptr<ItemEditorLayout> frameDraft,
            bool remainsOpen,
            bool changed)
        {
            if (!selected ||
                selected->GetHandle() != frameDraft->GetHandle())
            {
                return;
            }

            if (!remainsOpen)
                Close();
            resetLayout = false;
            draft = frameDraft->Clone();
            if (changed)
            {
                pendingUpdate = std::move(frameDraft);
                status.clear();
            }
        }
    };

    static ImVec2 Scale(ImVec2 value, float scale)
    {
        return ImVec2(value.x * scale, value.y * scale);
    }

    static float CalculateUiScale()
    {
        const ImGuiIO& io = ImGui::GetIO();
        const float widthScale = io.DisplaySize.x / 1920.0f;
        const float heightScale = io.DisplaySize.y / 1080.0f;
        float scale = widthScale < heightScale ? widthScale : heightScale;
        if (scale < 1.0f)
            return 1.0f;
        if (scale > 2.5f)
            return 2.5f;
        return scale;
    }

    const HS_ModApi* m_api;
    std::vector<std::unique_ptr<ItemEditorLayout>> m_layouts;
    std::mutex m_mutex;
    EditorState m_state;
};

ItemEditorWindow::ItemEditorWindow(const HS_ModApi* api)
    : m_impl(std::make_unique<Impl>(api))
{
}

ItemEditorWindow::~ItemEditorWindow() = default;

ItemEditorWindow::OpenResult ItemEditorWindow::OpenForItem(
    int handle,
    std::string& error)
{
    return m_impl->OpenForItem(handle, error);
}

void ItemEditorWindow::ProcessPendingChanges()
{
    m_impl->ProcessPendingChanges();
}

void ItemEditorWindow::Draw()
{
    m_impl->Draw();
}

bool ItemEditorWindow::TakeCloseRequest()
{
    return m_impl->TakeCloseRequest();
}
