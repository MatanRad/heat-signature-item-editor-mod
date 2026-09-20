#include "ItemEditorWindow.h"

#include "GunEditor.h"

#include <imgui.h>

#include <mutex>
#include <optional>
#include <string>
#include <utility>

class ItemEditorWindow::Impl
{
public:
    /// Creates a window controller backed by the loader API.
    explicit Impl(const HS_ModApi* api)
        : m_api(api), m_gunEditor(api)
    {
    }

    /// Captures a supported item and starts a fresh window session.
    OpenResult OpenForItem(int handle, std::string& error)
    {
        auto snapshot = m_gunEditor.CaptureSnapshot(handle, error);
        if (!snapshot)
        {
            return error == "Selected item is not a gun"
                ? OpenResult::UnsupportedItem
                : OpenResult::Failed;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.Open(std::move(*snapshot));
        return OpenResult::Opened;
    }

    /// Applies the latest requested parameters on the calling game thread.
    void ProcessPendingChanges()
    {
        int handle = 0;
        GunParameters parameters;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_state.TakePendingUpdate(handle, parameters))
                return;
        }

        std::string error;
        auto snapshot = m_gunEditor.ApplyParameters(
            handle,
            parameters,
            error);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.CompleteUpdate(handle, std::move(snapshot), error);
        if (!error.empty())
            m_api->Log("ItemEditor", error.c_str());
    }

    /// Renders one frame using a copy of state, then publishes UI changes.
    void Draw()
    {
        EditorState local;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            local = m_state;
        }
        if (!local.open || !local.selected)
            return;

        const float uiScale = CalculateUiScale();
        bool open = local.open;
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

        if (local.resetLayout)
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
                "Item Editor - Gun",
                &open,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetWindowFontScale(uiScale);
            DrawGunControls(local, changed);
        }
        ImGui::End();
        ImGui::PopStyleVar(4);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.CommitFrame(
            local.selected->handle,
            open,
            local.draft,
            changed);
    }

private:
    struct EditorState
    {
        bool open = false;
        bool resetLayout = false;
        std::optional<GunSnapshot> selected;
        GunParameters draft;
        std::optional<GunParameters> pendingUpdate;
        std::string status;

        /// Returns the state to its no-selection baseline.
        void Reset()
        {
            open = false;
            resetLayout = false;
            selected.reset();
            draft = {};
            pendingUpdate.reset();
            status.clear();
        }

        /// Opens a newly selected item and requests fresh window geometry.
        void Open(GunSnapshot snapshot)
        {
            Reset();
            open = true;
            resetLayout = true;
            draft = snapshot.parameters;
            selected = std::move(snapshot);
        }

        /// Hides the window while retaining its current editing session.
        void Close()
        {
            open = false;
            resetLayout = false;
        }

        /// Atomically consumes the one-slot render-to-game-thread mailbox.
        bool TakePendingUpdate(int& handle, GunParameters& parameters)
        {
            if (!selected || !pendingUpdate)
                return false;

            handle = selected->handle;
            parameters = *pendingUpdate;
            pendingUpdate.reset();
            return true;
        }

        /// Publishes an applied snapshot unless another item was selected meanwhile.
        void CompleteUpdate(
            int expectedHandle,
            std::optional<GunSnapshot> snapshot,
            const std::string& error)
        {
            if (!selected || selected->handle != expectedHandle)
                return;

            if (!snapshot)
            {
                status = error;
                return;
            }

            selected = std::move(snapshot);
            if (!pendingUpdate)
                draft = selected->parameters;
            status.clear();
        }

        /// Commits one render frame and replaces any older pending parameters.
        void CommitFrame(
            int expectedHandle,
            bool remainsOpen,
            const GunParameters& frameDraft,
            bool changed)
        {
            if (!selected || selected->handle != expectedHandle)
                return;

            if (!remainsOpen)
                Close();
            resetLayout = false;
            draft = frameDraft;
            if (changed)
            {
                pendingUpdate = frameDraft;
                status.clear();
            }
        }
    };

    /// Scales an ImGui spacing vector without changing the shared style.
    static ImVec2 Scale(ImVec2 value, float scale)
    {
        return ImVec2(value.x * scale, value.y * scale);
    }

    /// Calculates a per-window scale from a 1920x1080 baseline.
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

    /// Draws the gun-specific controls and updates the local draft.
    static void DrawGunControls(EditorState& state, bool& changed)
    {
        ImGui::TextUnformatted(state.selected->name.c_str());
        ImGui::Text("Instance: %d", state.selected->handle);
        ImGui::TextDisabled("Changes apply automatically");
        ImGui::Separator();

        int damageType = state.draft.concussive ? 1 : 0;
        ImGui::TextUnformatted("Damage");
        changed |= ImGui::RadioButton("Lethal", &damageType, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Concussive", &damageType, 1);
        state.draft.concussive = damageType == 1;

        int loudness = static_cast<int>(state.draft.loudness);
        ImGui::TextUnformatted("Sound");
        changed |= ImGui::RadioButton(
            "Loud",
            &loudness,
            static_cast<int>(GunLoudness::Loud));
        ImGui::SameLine();
        changed |= ImGui::RadioButton(
            "Quiet",
            &loudness,
            static_cast<int>(GunLoudness::Quiet));
        ImGui::SameLine();
        changed |= ImGui::RadioButton(
            "Silenced",
            &loudness,
            static_cast<int>(GunLoudness::Silenced));
        state.draft.loudness = static_cast<GunLoudness>(loudness);

        int fireMode = static_cast<int>(state.draft.fireMode);
        ImGui::TextUnformatted("Fire mode");
        changed |= ImGui::RadioButton(
            "Normal",
            &fireMode,
            static_cast<int>(GunFireMode::Normal));
        ImGui::SameLine();
        changed |= ImGui::RadioButton(
            "Quickfire",
            &fireMode,
            static_cast<int>(GunFireMode::Quickfire));
        ImGui::SameLine();
        changed |= ImGui::RadioButton(
            "Automatic",
            &fireMode,
            static_cast<int>(GunFireMode::Automatic));
        state.draft.fireMode = static_cast<GunFireMode>(fireMode);

        changed |= ImGui::Checkbox(
            "Armour-piercing",
            &state.draft.armourPiercing);
        ImGui::Separator();

        if (state.pendingUpdate)
            ImGui::TextUnformatted("Applying changes...");
        if (!state.status.empty())
            ImGui::TextWrapped("%s", state.status.c_str());
    }

    const HS_ModApi* m_api;
    GunEditor m_gunEditor;
    std::mutex m_mutex;
    EditorState m_state;
};

ItemEditorWindow::ItemEditorWindow(const HS_ModApi* api)
    : m_impl(std::make_unique<Impl>(api))
{
}

ItemEditorWindow::~ItemEditorWindow() = default;

/// Opens the editor for a live item selected on the game thread.
ItemEditorWindow::OpenResult ItemEditorWindow::OpenForItem(
    int handle,
    std::string& error)
{
    return m_impl->OpenForItem(handle, error);
}

/// Pumps the pending parameter mailbox from a game-thread hook.
void ItemEditorWindow::ProcessPendingChanges()
{
    m_impl->ProcessPendingChanges();
}

/// Draws the editor from the loader's ImGui render callback.
void ItemEditorWindow::Draw()
{
    m_impl->Draw();
}
