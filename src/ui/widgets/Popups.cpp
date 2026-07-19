
#include <ui/widgets/Popups.h>
#include <ui/widgets/WidgetInternal.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <imgui/imgui.h>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace tsm { namespace ui { namespace widgets {

bool AccentButton(const char* label, const ImVec2& size);
bool SecondaryButton(const char* label, const ImVec2& size);
bool ToggleSwitch(const char* label, bool* v);


bool InputWithPlaceholder(const char* id, char* buf, size_t buf_size, const char* placeholder) {
    const char* ph = placeholder ? tsm::ui::i18n::Tr(placeholder) : "";
#if IMGUI_VERSION_NUM >= 18933
    ImVec4 bg = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(bg.x + 0.02f, bg.y + 0.02f, bg.z + 0.02f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(bg.x + 0.03f, bg.y + 0.03f, bg.z + 0.03f, 1.0f));
    bool changed = ImGui::InputTextWithHint(id, ph, buf, (int)buf_size);
    ImGui::PopStyleColor(3);
    return changed;
#else
    bool changed = ImGui::InputText(id, buf, (int)buf_size);
    if (!ImGui::IsItemActive() && (!buf || buf[0] == '\0')) {
        ImVec2 p = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(p.x + DP(6.0f), p.y + DP(2.0f)), ImGui::GetColorU32(kMutedText()), ph);
    }
    return changed;
#endif
}


bool ConfirmPopup(const char* popup_id,
                  const char* title,
                  const char* message,
                  const char* confirm_label,
                  const char* cancel_label) {
    bool confirmed = false;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(DP(280.0f), DP(0.0f)), ImGuiCond_Always);
    }
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
    if (ImGui::BeginPopupModal(popup_id, nullptr, flags)) {
        if (title && *title) {
            const char* localizedTitle = tsm::ui::i18n::Tr(title);
            float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            ImVec2 ts = ImGui::CalcTextSize(localizedTitle);
            float x = ImGui::GetWindowContentRegionMin().x + std::max(0.0f, (cont_w - ts.x) * 0.5f);
            ImGui::SetCursorPosX(x);
            ImGui::TextUnformatted(localizedTitle);
            ImGui::Separator();
        }
        if (message && *message) {
            const char* localizedMsg = tsm::ui::i18n::Tr(message);
            ImGui::TextWrapped("%s", localizedMsg);
        }
        ImGui::Dummy(ImVec2(0, DP(6.0f)));
        float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        float gap = DP(8.0f);
        float btn_w = (cont_w - gap) * 0.5f;
        bool showCancel = (cancel_label && *cancel_label);
        bool showConfirm = (confirm_label && *confirm_label);
        if (showCancel) {
            if (SecondaryButton(cancel_label, ImVec2(btn_w, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        if (showConfirm) {
            if (showCancel) ImGui::SameLine();
            if (AccentButton(confirm_label, ImVec2(btn_w, 0))) {
                confirmed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    return confirmed;
}


bool InputPopup(const char* popup_id,
                const char* title,
                const char* placeholder,
                char* buf,
                size_t buf_size,
                const char* confirm_label,
                const char* cancel_label) {
    bool confirmed = false;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(DP(280.0f), DP(0.0f)), ImGuiCond_Always);
    }
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
    if (ImGui::BeginPopupModal(popup_id, nullptr, flags)) {
        if (title && *title) {
            const char* localizedTitle = tsm::ui::i18n::Tr(title);
            float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            ImVec2 ts = ImGui::CalcTextSize(localizedTitle);
            float x = ImGui::GetWindowContentRegionMin().x + std::max(0.0f, (cont_w - ts.x) * 0.5f);
            ImGui::SetCursorPosX(x);
            ImGui::TextUnformatted(localizedTitle);
            ImGui::Separator();
        }
        float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        ImGui::PushItemWidth(cont_w);
        InputWithPlaceholder("##input", buf, buf_size, placeholder ? placeholder : "");
        ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0, DP(4.0f)));
        cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        float gap = DP(8.0f);
        float btn_w = (cont_w - gap) * 0.5f;
        bool showCancel = (cancel_label && *cancel_label);
        bool showConfirm = (confirm_label && *confirm_label);
        if (showCancel) {
            if (SecondaryButton(cancel_label, ImVec2(btn_w, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        if (showConfirm) {
            if (showCancel) ImGui::SameLine();
            if (AccentButton(confirm_label, ImVec2(btn_w, 0))) {
                confirmed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    return confirmed;
}


bool DetectOptionHold(const char* id, bool hovered, float seconds) {
    static std::unordered_map<std::string, float> t;
    ImGuiIO& io = ImGui::GetIO();
    bool alt = (io.KeyMods & ImGuiMod_Alt) != 0;
    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    float& acc = t[id ? id : ""];
    if (hovered && (mouseDown || alt)) {
        acc += io.DeltaTime;
        if (acc >= seconds) { acc = 0.0f; return true; }
    } else {
        acc = 0.0f;
    }
    return false;
}

}}}
