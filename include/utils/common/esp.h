#pragma once

#include <utils/common/vec2.h>
#include <utils/common/vec3.h>
#include <utils/common/vec4.h>
#include <imgui/imgui.h>
#include <string>

namespace tsm { namespace utils { namespace esp {

struct GameViewMatrix {
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
};

bool WorldToScreen(const vec3& worldPos, vec2& screenPos);

bool DrawTextAtWorldPos(const vec3& worldPos, const char* text, ImU32 color = IM_COL32(255, 255, 255, 255), float offsetY = 0.0f);

bool DrawIconAtWorldPos(const vec3& worldPos, const char* iconName, float size = 24.0f, ImVec4 tint = ImVec4(1, 1, 1, 1));

bool DrawIconTextAtWorldPos(const vec3& worldPos, const char* iconName, const char* text, float iconSize = 24.0f, ImU32 textColor = IM_COL32(255, 255, 255, 255));

GameViewMatrix* GetViewProjectionMatrix();

bool IsAvailable();

}}}
