#ifndef TSM_UI_CUSTOM_ICON_LOADER_H
#define TSM_UI_CUSTOM_ICON_LOADER_H

#include <string>
#include <imgui/imgui.h>
#include <unordered_map>

namespace tsm { namespace ui { namespace widgets {

class CustomIconLoader {
public:
    static CustomIconLoader& Get();

    ImTextureID GetIcon(const std::string& name);

    void RegisterIcon(const std::string& name, const unsigned char* data, size_t size);

private:
    CustomIconLoader() = default;
    ~CustomIconLoader();

    std::unordered_map<std::string, ImTextureID> icons_;

    ImTextureID CreateTextureFromMemory(const unsigned char* data, size_t size);
};

}}}

#endif
