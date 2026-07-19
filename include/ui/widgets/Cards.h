#pragma once

#include <cstddef>
#include <imgui/imgui.h>

namespace tsm { namespace ui { namespace widgets {


bool ToggleCard(const char* title, const char* description, bool* v);
bool ToggleCardIcon(const char* title, const char* description, const char* iconLabel, bool* v);

bool IntCard(const char* title, const char* description, int* v, int v_min, int v_max);
bool IntCardIcon(const char* title, const char* description, const char* iconLabel, int* v, int v_min, int v_max);

bool FloatCard(const char* title, const char* description, float* v, float v_min, float v_max, const char* fmt = "%.1f");
bool FloatCardIcon(const char* title, const char* description, const char* iconLabel, float* v, float v_min, float v_max, float v_default = 1.0f, const char* fmt = "%.1f");

bool InputCardIcon(const char* title, const char* description, const char* iconLabel, float* v, const char* fmt = "%.2f");

bool SelectCard(const char* title, const char* description, int* current_index, const char* const* options, int options_count);
bool SelectCardIcon(const char* title, const char* description, const char* iconLabel, int* current_index, const char* const* options, int options_count);
bool SelectCardIconList(const char* title, const char* description, int* current_index, const char* const* iconNames, int iconCount);
bool StringSliderCard(const char* title, const char* description, const char* iconLabel, const char* const* options, int options_count, int* selected_index);

bool ButtonCard(const char* title, const char* description, const char* iconLabel, const char* trailingText = nullptr, const ImVec4& iconColor = ImVec4(1,1,1,1));

bool ColorPickerCard(const char* title, const char* description, const char* iconLabel, ImVec4* color, float height = 64.0f);

struct EditableCardClick {
    bool main;
    bool edit;
};
EditableCardClick EditableCard(const char* title, const char* description, const char* iconLabel);
EditableCardClick EditableCard(const char* title, const char* description, const char* iconLabel, bool selected);

bool SearchCard(const char* placeholder, char* buf, size_t buf_size);

bool ToggleButton(const char* id, const char* iconName, bool* value, float size = 28.0f, const char* tooltip = nullptr);

}}}
