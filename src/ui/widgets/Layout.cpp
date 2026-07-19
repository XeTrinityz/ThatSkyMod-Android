
#include <ui/widgets/Layout.h>
#include <ui/widgets/Buttons.h>
#include <ui/core/metrics.h>
#include <ui/core/ThemeManager.h>
#include <ui/helpers/Animations.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace tsm { namespace ui { namespace widgets {

static float s_cardPad = 12.0f;
static bool  s_cardOpen = false;

struct PannableStyleState { int varCount; int colCount; };
static std::vector<PannableStyleState> s_pannableStyleStack;
struct PannableInertia {
    float vx = 0.0f;
    float vy = 0.0f;
    bool active = false;
    bool isDragging = false;
    ImVec2 lastMousePos = ImVec2(0, 0);
};
static std::unordered_map<std::string, PannableInertia> s_pannableInertia;

struct ScrollIndicatorState {
    float shimmer_time = 0.0f;
    float pulse_time = 0.0f;
};
static std::unordered_map<std::string, ScrollIndicatorState> s_scrollIndicatorState;

bool BeginPannableChild(const char* id,
                        const ImVec2& size,
                        bool border,
                        ImGuiWindowFlags flags,
                        bool styleNoPadding,
                        bool transparentBg,
                        float minPanThreshold)
{
    int varCount = 0;
    int colCount = 0;
    if (styleNoPadding) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ++varCount; }
    if (transparentBg) { ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0)); ++colCount; }

    bool open = ImGui::BeginChild(id, size, border, flags);

    ImGuiIO& io = ImGui::GetIO();
    PannableInertia& inertia = s_pannableInertia[std::string(id)];
    const float dt = (io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f);

    ImVec2 winMin = ImGui::GetWindowPos();
    ImVec2 winMax = ImVec2(winMin.x + ImGui::GetWindowWidth(), winMin.y + ImGui::GetWindowHeight());
    ImVec2 mousePos = io.MousePos;
    const bool mouseInWindow = (mousePos.x >= winMin.x && mousePos.x <= winMax.x &&
                                mousePos.y >= winMin.y && mousePos.y <= winMax.y);

    const bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    if (mousePressed && mouseInWindow && !inertia.isDragging) {
        inertia.isDragging = true;
        inertia.lastMousePos = mousePos;
        inertia.vy = 0.0f;
        inertia.vx = 0.0f;
    }
    if (!mousePressed) {
        inertia.isDragging = false;
    }

    ImVec2 ourDelta = ImVec2(0, 0);
    if (inertia.isDragging && mousePressed) {
        ourDelta.x = mousePos.x - inertia.lastMousePos.x;
        ourDelta.y = mousePos.y - inertia.lastMousePos.y;
    }

    const bool dragging = inertia.isDragging && mousePressed && mouseInWindow;

    if (dragging) {
        if (fabsf(ourDelta.y) > 0.001f || fabsf(ourDelta.x) > 0.001f) {
            ImGui::SetScrollY(ImGui::GetScrollY() - ourDelta.y);
            if (io.KeyShift) ImGui::SetScrollX(ImGui::GetScrollX() - ourDelta.x);

            const float invDt = (dt > 1e-6f) ? (1.0f / dt) : 0.0f;
            const float sampleVy = ourDelta.y * invDt;
            const float sampleVx = ourDelta.x * invDt;
            const float timeConstant = 0.05f;
            const float alpha = 1.0f - expf(-dt / timeConstant);
            inertia.vy = inertia.vy * (1.0f - alpha) + sampleVy * alpha;
            inertia.vx = inertia.vx * (1.0f - alpha) + sampleVx * alpha;

            inertia.lastMousePos = mousePos;
        }
        inertia.active = true;
    } else if (inertia.active) {
        const float decel = 6000.0f;
        auto apply_decel = [&](float& v) {
            if (v > 0.0f) { v = std::max(0.0f, v - decel * dt); }
            else if (v < 0.0f) { v = std::min(0.0f, v + decel * dt); }
        };

        const float oldY = ImGui::GetScrollY();
        const float oldX = ImGui::GetScrollX();
        const float maxY = ImGui::GetScrollMaxY();
        const float maxX = ImGui::GetScrollMaxX();

        float newY = oldY - inertia.vy * dt;
        float newX = oldX - inertia.vx * dt;
        newY = (newY < 0.0f) ? 0.0f : (newY > maxY ? maxY : newY);
        newX = (newX < 0.0f) ? 0.0f : (newX > maxX ? maxX : newX);

        ImGui::SetScrollY(newY);
        ImGui::SetScrollX(newX);

        if ((newY <= 0.0f && inertia.vy > 0.0f) || (newY >= maxY && inertia.vy < 0.0f) || fabsf(newY - oldY) < 0.1f)
            inertia.vy = 0.0f;
        if ((newX <= 0.0f && inertia.vx > 0.0f) || (newX >= maxX && inertia.vx < 0.0f) || fabsf(newX - oldX) < 0.1f)
            inertia.vx = 0.0f;

        apply_decel(inertia.vy);
        apply_decel(inertia.vx);

        const float stop_eps = 15.0f;
        if (fabsf(inertia.vy) < stop_eps && fabsf(inertia.vx) < stop_eps)
            inertia.active = false;
    }

    s_pannableStyleStack.push_back({varCount, colCount});
    return open;
}

void EndPannableChild()
{
    const float scrollY = ImGui::GetScrollY();
    const float scrollMaxY = ImGui::GetScrollMaxY();
    const float indicatorHeight = DP(32.0f);

    if (scrollMaxY > 1.0f) {
        ImGuiIO& io = ImGui::GetIO();
        const float dt = (io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f);

        ImGuiID windowID = ImGui::GetCurrentWindow()->ID;
        std::string stateKey = std::to_string(windowID);
        ScrollIndicatorState& animState = s_scrollIndicatorState[stateKey];

        animState.shimmer_time += dt;
        animState.pulse_time += dt;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winMin = ImGui::GetWindowPos();
        ImVec2 winMax = ImVec2(winMin.x + ImGui::GetWindowWidth(), winMin.y + ImGui::GetWindowHeight());

        ImVec4 accentColor = ThemeManager::Get().GetAccentColor();

        float topIntensity = std::min(1.0f, scrollY / indicatorHeight);
        float bottomIntensity = std::min(1.0f, (scrollMaxY - scrollY) / indicatorHeight);

        auto DrawChevronIndicator = [&](bool isTop, float intensity) {
            if (intensity < 0.01f) return;

            using namespace helpers;

            float centerX = winMin.x + (winMax.x - winMin.x) * 0.5f;
            float centerY = isTop ? winMin.y + DP(12.0f) : winMax.y - DP(12.0f);

            float pulseTime = animState.pulse_time * 0.8f * 6.28318f;
            float pulseCurve = (sinf(pulseTime) + 1.0f) * 0.5f;
            float pulseScale = 1.0f + 0.12f * pulseCurve * intensity;
            float pulseAlpha = 0.5f + 0.5f * pulseCurve * intensity;

            float shimmerTime = animState.shimmer_time * 1.2f * 6.28318f;
            float shimmerAlpha = 0.25f * ((sinf(shimmerTime) + 1.0f) * 0.5f) * intensity;

            const int chevronCount = 1;
            const float chevronSpacing = DP(6.0f);
            const float chevronWidth = DP(16.0f);
            const float chevronThickness = DP(2.5f);
            const float chevronAngle = DP(4.0f);

            float waveTime = animState.pulse_time * 0.8f * 6.28318f;
            float waveOffset = sinf(waveTime) * DP(3.0f);

            for (int i = 0; i < chevronCount; ++i) {
                float chevronAlpha = intensity * pulseAlpha * (1.0f - i * 0.25f);
                if (chevronAlpha < 0.01f) continue;

                float yOffset = isTop ?
                    i * chevronSpacing + waveOffset :
                    -i * chevronSpacing - waveOffset;
                float chevronY = centerY + yOffset;

                ImVec2 p1 = ImVec2(centerX - chevronWidth * 0.5f, chevronY + (isTop ? chevronAngle : -chevronAngle));
                ImVec2 p2 = ImVec2(centerX, chevronY);
                ImVec2 p3 = ImVec2(centerX + chevronWidth * 0.5f, chevronY + (isTop ? chevronAngle : -chevronAngle));

                auto ScalePoint = [&](ImVec2 p) -> ImVec2 {
                    ImVec2 dir = ImVec2(p.x - centerX, p.y - chevronY);
                    return ImVec2(centerX + dir.x * pulseScale, chevronY + dir.y * pulseScale);
                };

                p1 = ScalePoint(p1);
                p2 = ScalePoint(p2);
                p3 = ScalePoint(p3);

                ImVec4 glowColor = accentColor;
                glowColor.w = (chevronAlpha + shimmerAlpha) * 0.3f;
                dl->AddPolyline((ImVec2[]){p1, p2, p3}, 3, ImGui::GetColorU32(glowColor), 0, chevronThickness + DP(3.0f));

                ImVec4 mainColor = accentColor;
                mainColor.w = chevronAlpha + shimmerAlpha;
                dl->AddPolyline((ImVec2[]){p1, p2, p3}, 3, ImGui::GetColorU32(mainColor), 0, chevronThickness);
            }

            float backdropAlpha = intensity * 0.08f;
            ImVec4 bgColor = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
            if (bgColor.w < 0.01f) {
                bgColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
            }

            const float backdropHeight = DP(16.0f);

            if (isTop) {
                ImU32 colorTop = ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, backdropAlpha));
                ImU32 colorBottom = ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 0.0f));
                dl->AddRectFilledMultiColor(
                    ImVec2(winMin.x, winMin.y),
                    ImVec2(winMax.x, winMin.y + backdropHeight),
                    colorTop, colorTop, colorBottom, colorBottom
                );
            } else {
                ImU32 colorTop = ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 0.0f));
                ImU32 colorBottom = ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, backdropAlpha));
                dl->AddRectFilledMultiColor(
                    ImVec2(winMin.x, winMax.y - backdropHeight),
                    ImVec2(winMax.x, winMax.y),
                    colorTop, colorTop, colorBottom, colorBottom
                );
            }
        };

        if (scrollY > 0.5f) {
            DrawChevronIndicator(true, topIntensity);
        }

        if (scrollY < scrollMaxY - 0.5f) {
            DrawChevronIndicator(false, bottomIntensity);
        }
    }

    ImGui::EndChild();
    if (!s_pannableStyleStack.empty()) {
        PannableStyleState st = s_pannableStyleStack.back();
        s_pannableStyleStack.pop_back();
        if (st.colCount > 0) ImGui::PopStyleColor(st.colCount);
        if (st.varCount > 0) ImGui::PopStyleVar(st.varCount);
    }
}

bool BeginCard(const char* id, float padding, bool withBorder) {
    s_cardPad = DP(padding);
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, withBorder ? style.ChildBorderSize : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(s_cardPad, s_cardPad));

    ImVec4 cardBg = style.Colors[ImGuiCol_WindowBg];
    cardBg.w = withBorder ? (cardBg.w * 0.92f) : 0.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBg);

    bool ok = ImGui::BeginChild(id, ImVec2(0, 0), withBorder, ImGuiWindowFlags_NoScrollbar);
    if (!ok) {
        ImGui::EndChild();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
        s_cardOpen = false;
        return false;
    }

    ImGui::PopStyleColor(1);
    ImGui::PushItemWidth(-1.0f);
    s_cardOpen = true;
    return true;
}

void EndCard() {
    if (!s_cardOpen) return;
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    s_cardOpen = false;
}

float GetCardPadding() {
    return s_cardPad;
}

void NumberedButtonGrid(int count, int selectedIndex,
                        const std::function<void(int)>& onButtonClick,
                        const std::function<ImVec4(int)>& colorGetter) {
    const float d = DP(36.0f);
    const float side_padding = DP(2.0f);
    float avail_w = ImGui::GetContentRegionAvail().x - (side_padding * 2.0f);
    float gap = ImGui::GetStyle().ItemSpacing.x;
    int cols = (int)std::max(1.0f, floorf((avail_w + gap) / (d + gap)));
    float row_w = cols * d + (cols - 1) * gap;
    float left_pad = side_padding + std::max(0.0f, (avail_w - row_w) * 0.5f);

    ImVec2 start = ImGui::GetCursorPos();
    int i = 0;
    while (i < count) {
        int col = i % cols;
        int row = i / cols;
        ImVec2 pos = ImVec2(start.x + left_pad + col * (d + gap), start.y + row * (d + gap));
        ImGui::SetCursorPos(pos);
        ImGui::PushID(i);

        ImVec4 color = colorGetter ? colorGetter(i) : ImVec4(0,0,0,0);

        bool clicked = tsm::ui::widgets::NumberCircleButton(i + 1, i == selectedIndex, 36.0f, color);
        if (clicked && onButtonClick) {
            onButtonClick(i);
        }

        ImGui::PopID();
        ++i;
    }

    float used_h = (float)((i + cols - 1) / cols) * (d + gap);
    ImGui::Dummy(ImVec2(0, std::max(0.0f, used_h - (ImGui::GetCursorPosY() - start.y))));
}

}}}
