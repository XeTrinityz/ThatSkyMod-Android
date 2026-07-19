#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui {

class WindowManager {
public:
    static WindowManager& Get();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    void Initialize();

    bool BeginFrame(bool* menu_open, bool auto_scale, const ImVec4& accent_color);

    void EndFrame();

    void SyncToModState();

    int GetActiveTab() const { return active_tab_; }

    void SetActiveTab(int tab) { active_tab_ = tab; }

    bool IsCollapsed() const { return collapsed_; }

    void SetCollapsed(bool collapsed) { collapsed_ = collapsed; }

    void ToggleCollapsed() { collapsed_ = !collapsed_; }

    bool IsDockedRight() const { return dock_right_; }

    void SetDockedRight(bool right) { dock_right_ = right; }

    void ToggleDockedRight() { dock_right_ = !dock_right_; }

private:
    WindowManager() = default;
    ~WindowManager() = default;

    void RenderCollapsedHandle(const ImVec4& accent_color);
    void RenderCloseHandle();
    void UpdateSlideAnimation(float delta_time);
    void ApplyAutoScaling();

    ImVec2 win_size_{1008.0f, 910.0f};
    ImVec2 win_pos_{-1.0f, -1.0f};
    bool dragging_{false};
    ImVec2 drag_offset_{0, 0};
    bool collapsed_{true};
    bool request_close_{false};
    int active_tab_{0};
    bool first_frame_{true};

    float slide_t_{0.0f};

    bool dock_right_{true};
    float handle_y_norm_{0.5f};

    int pushed_colors_{0};
    int pushed_vars_{0};
};

}}
