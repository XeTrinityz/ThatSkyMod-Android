#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui { namespace helpers {

struct StyleColorScope {
    int count;
    explicit StyleColorScope(int n = 0) : count(n) {}
    ~StyleColorScope() { if (count > 0) ImGui::PopStyleColor(count); }
    StyleColorScope(const StyleColorScope&) = delete;
    StyleColorScope& operator=(const StyleColorScope&) = delete;
};

struct StyleVarScope {
    int count;
    explicit StyleVarScope(int n = 0) : count(n) {}
    ~StyleVarScope() { if (count > 0) ImGui::PopStyleVar(count); }
    StyleVarScope(const StyleVarScope&) = delete;
    StyleVarScope& operator=(const StyleVarScope&) = delete;
};

struct IDScope {
    IDScope(int id) { ImGui::PushID(id); }
    IDScope(const char* id) { ImGui::PushID(id); }
    IDScope(const void* id) { ImGui::PushID(id); }
    ~IDScope() { ImGui::PopID(); }
    IDScope(const IDScope&) = delete;
    IDScope& operator=(const IDScope&) = delete;
};

}}}
