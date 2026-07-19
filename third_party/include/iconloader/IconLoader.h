#ifndef SKY_MODLOADER_ICONLOADER_H
#define SKY_MODLOADER_ICONLOADER_H
#include "../imgui/imgui.h"
#include "string"
#define IL_NO_TEXTURE (ImTextureID)-1

class SkyImage {
public:
    ImTextureID textureId;
    ImVec2 size;
};

class UIIcon {
public:
    ImTextureID textureId;
    ImVec2 uv0;
    ImVec2 uv1;
};

class IconLoader {
public:
    /**
    * Gets an image under the name "name" from Data/Images/bin/ETC2/<name>.ktx, ready to be drawn using ImGui::Image or ImGui::ImageButton if it was loaded successfully
    * Returns: a SkyImage object
    * If the image fails to load for some reason, the textureId would be equal to IL_NO_TEXTURE, and must not be drawn by your code.
    */
    static SkyImage& getImage(const std::string& name);
    /**
    * Draws an icon, loaded by its name from UiPackedAtlas.lua, with the specified name and size in pixels
    */
    static void icon(const std::string& name, const float& size, const ImVec4& color = ImVec4(1,1,1,1));
    /**
    * Creates a button with an icon, loaded by its name from UiPackedAtlas.lua, with the specified name and size in pixels
    */
    static bool iconButton(const std::string& name, const float& size, const ImVec4& color = ImVec4(1,1,1,1));
    /**
    * Gets the required parameters for an icon, loaded by its name from UiPackedAtlas.lua, with the specified name
    * If icon->textureId is IL_NO_TEXTURE, the icon data is not valid and it shouldn't be drawn.
    * Note: you should not store the result of this function outside of your draw function's scope
    */
    static void getUIIcon(const std::string &name, UIIcon* icon);
};


#endif //SKY_MODLOADER_ICONLOADER_H
