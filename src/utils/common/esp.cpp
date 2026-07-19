#include <utils/common/esp.h>
#include <game/memory/api.h>
#include <game/memory/offsets.h>
#include <iconloader/IconLoader.h>
#include <imgui/imgui.h>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define USE_NEON 1
#endif

namespace tsm { namespace utils { namespace esp {

GameViewMatrix* GetViewProjectionMatrix() {
    std::uintptr_t cameraSystem = tsm::game::api::CameraSystem();
    if (cameraSystem == 0) return nullptr;

    std::uintptr_t* intermediatePtr = reinterpret_cast<std::uintptr_t*>(cameraSystem + tsm::game::Offsets::kCameraIntermediate);
    if (!intermediatePtr || *intermediatePtr == 0) return nullptr;

    std::uintptr_t viewProjAddr = *intermediatePtr + tsm::game::Offsets::kViewProjectionMatrix;
    return reinterpret_cast<GameViewMatrix*>(viewProjAddr);
}

bool IsAvailable() {
    return GetViewProjectionMatrix() != nullptr;
}

bool WorldToScreen(const vec3& worldPos, vec2& screenPos) {
    auto viewProj = GetViewProjectionMatrix();
    if (!viewProj) return false;

#if USE_NEON
    float32x4_t pos = {worldPos.x, worldPos.y, worldPos.z, 1.0f};

    float32x4_t row1 = {viewProj->_11, viewProj->_12, viewProj->_13, viewProj->_14};
    float32x4_t row2 = {viewProj->_21, viewProj->_22, viewProj->_23, viewProj->_24};
    float32x4_t row3 = {viewProj->_31, viewProj->_32, viewProj->_33, viewProj->_34};
    float32x4_t row4 = {viewProj->_41, viewProj->_42, viewProj->_43, viewProj->_44};

    float32x4_t result;
    result = vmulq_n_f32(row1, vgetq_lane_f32(pos, 0));
    result = vmlaq_n_f32(result, row2, vgetq_lane_f32(pos, 1));
    result = vmlaq_n_f32(result, row3, vgetq_lane_f32(pos, 2));
    result = vmlaq_n_f32(result, row4, vgetq_lane_f32(pos, 3));

    float clipX = vgetq_lane_f32(result, 0);
    float clipY = vgetq_lane_f32(result, 1);
    float clipW = vgetq_lane_f32(result, 3);
#else
    float clipX = worldPos.x * viewProj->_11 + worldPos.y * viewProj->_21 + worldPos.z * viewProj->_31 + viewProj->_41;
    float clipY = worldPos.x * viewProj->_12 + worldPos.y * viewProj->_22 + worldPos.z * viewProj->_32 + viewProj->_42;
    float clipW = worldPos.x * viewProj->_14 + worldPos.y * viewProj->_24 + worldPos.z * viewProj->_34 + viewProj->_44;
#endif

    if (clipW < 0.1f) return false;

    float invW = 1.0f / clipW;
    float ndcX = clipX * invW;
    float ndcY = clipY * invW;

    std::uintptr_t cameraSystem = tsm::game::api::CameraSystem();
    if (cameraSystem != 0) {
        std::uintptr_t* intermediatePtr = reinterpret_cast<std::uintptr_t*>(cameraSystem + tsm::game::Offsets::kCameraIntermediate);
        if (intermediatePtr && *intermediatePtr) {
            float* jitter = reinterpret_cast<float*>(*intermediatePtr + tsm::game::Offsets::kJitterFullHalf);
            ndcX += jitter[0] * 2.0f;
            ndcY += jitter[1] * 2.0f;
        }
    }

    const ImVec2& displaySize = ImGui::GetIO().DisplaySize;

    screenPos.x = (ndcX * 0.5f + 0.5f) * displaySize.x;
    screenPos.y = (0.5f - ndcY * 0.5f) * displaySize.y;

    return screenPos.x >= 0.0f && screenPos.x <= displaySize.x &&
           screenPos.y >= 0.0f && screenPos.y <= displaySize.y;
}

bool DrawTextAtWorldPos(const vec3& worldPos, const char* text, ImU32 color, float offsetY) {
    if (!text) return false;

    vec2 screenPos;
    if (!WorldToScreen(worldPos, screenPos)) return false;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    const char* lineStart = text;
    float currentY = screenPos.y + offsetY;

    while (*lineStart) {
        const char* lineEnd = lineStart;
        while (*lineEnd && *lineEnd != '\n') ++lineEnd;

        ImVec2 lineSize = ImGui::CalcTextSize(lineStart, lineEnd);
        ImVec2 pos(screenPos.x - lineSize.x * 0.5f, currentY);

        drawList->AddText(ImVec2(pos.x - 1, pos.y), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
        drawList->AddText(ImVec2(pos.x + 1, pos.y), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
        drawList->AddText(ImVec2(pos.x, pos.y - 1), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
        drawList->AddText(ImVec2(pos.x, pos.y + 1), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
        drawList->AddText(pos, color, lineStart, lineEnd);

        currentY += lineSize.y;

        if (*lineEnd == '\n') {
            lineStart = lineEnd + 1;
        } else {
            break;
        }
    }

    return true;
}

bool DrawIconAtWorldPos(const vec3& worldPos, const char* iconName, float size, ImVec4 tint) {
    if (!iconName) return false;

    vec2 screenPos;
    if (!WorldToScreen(worldPos, screenPos)) return false;

    UIIcon icon{};
    IconLoader::getUIIcon(iconName, &icon);
    if (icon.textureId == IL_NO_TEXTURE) {
        return false;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    float shadowSize = size * 1.15f;
    ImVec2 shadowPos(screenPos.x - shadowSize * 0.5f, screenPos.y - shadowSize * 0.5f);
    drawList->AddImage(
        icon.textureId,
        shadowPos,
        ImVec2(shadowPos.x + shadowSize, shadowPos.y + shadowSize),
        icon.uv0,
        icon.uv1,
        IM_COL32(0, 0, 0, 200)
    );

    ImVec2 pos(screenPos.x - size * 0.5f, screenPos.y - size * 0.5f);
    drawList->AddImage(
        icon.textureId,
        pos,
        ImVec2(pos.x + size, pos.y + size),
        icon.uv0,
        icon.uv1,
        IM_COL32(255, 255, 255, 255)
    );

    return true;
}

bool DrawIconTextAtWorldPos(const vec3& worldPos, const char* iconName, const char* text, float iconSize, ImU32 textColor) {
    if (!iconName && !text) return false;

    vec2 screenPos;
    if (!WorldToScreen(worldPos, screenPos)) return false;

    bool drewIcon = false;
    float textOffsetY = 0.0f;

    if (iconName) {
        UIIcon icon{};
        IconLoader::getUIIcon(iconName, &icon);
        if (icon.textureId != IL_NO_TEXTURE) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();

            float shadowSize = iconSize * 1.15f;
            ImVec2 shadowPos(screenPos.x - shadowSize * 0.5f, screenPos.y - shadowSize * 0.5f);
            drawList->AddImage(
                icon.textureId,
                shadowPos,
                ImVec2(shadowPos.x + shadowSize, shadowPos.y + shadowSize),
                icon.uv0,
                icon.uv1,
                IM_COL32(0, 0, 0, 200)
            );

            ImVec2 iconPos(screenPos.x - iconSize * 0.5f, screenPos.y - iconSize * 0.5f);
            drawList->AddImage(
                icon.textureId,
                iconPos,
                ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                icon.uv0,
                icon.uv1,
                IM_COL32(255, 255, 255, 255)
            );
            drewIcon = true;
            textOffsetY = iconSize * 0.6f;
        }
    }

    if (text && *text) {
        vec3 textWorldPos = worldPos;
        vec2 textScreenPos;
        if (WorldToScreen(textWorldPos, textScreenPos)) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();

            const char* lineStart = text;
            float currentY = textScreenPos.y + textOffsetY;

            while (*lineStart) {
                const char* lineEnd = lineStart;
                while (*lineEnd && *lineEnd != '\n') ++lineEnd;

                ImVec2 lineSize = ImGui::CalcTextSize(lineStart, lineEnd);
                ImVec2 pos(textScreenPos.x - lineSize.x * 0.5f, currentY);

                drawList->AddText(ImVec2(pos.x - 1, pos.y), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
                drawList->AddText(ImVec2(pos.x + 1, pos.y), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
                drawList->AddText(ImVec2(pos.x, pos.y - 1), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
                drawList->AddText(ImVec2(pos.x, pos.y + 1), IM_COL32(0, 0, 0, 255), lineStart, lineEnd);
                drawList->AddText(pos, textColor, lineStart, lineEnd);

                currentY += lineSize.y;

                if (*lineEnd == '\n') {
                    lineStart = lineEnd + 1;
                } else {
                    break;
                }
            }
        }
        return true;
    }

    return drewIcon;
}

}}}
