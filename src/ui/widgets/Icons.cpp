
#include <ui/widgets/Icons.h>
#include <iconloader/IconLoader.h>
#include <iconloader/CustomIconLoader.h>
#include <imgui/imgui.h>
#include <string>
#include <cmath>

namespace tsm { namespace ui { namespace widgets {


static void DrawRotatedIcon(UIIcon& uiIcon, float size, float rotationDeg, const ImVec4& color) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float angle = rotationDeg * (3.14159265f / 180.0f);
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);

    ImVec2 center = ImVec2(p.x + size * 0.5f, p.y + size * 0.5f);
    float half = size * 0.5f;

    auto rotate_point = [&](float x, float y) -> ImVec2 {
        float rx = x * cos_a - y * sin_a;
        float ry = x * sin_a + y * cos_a;
        return ImVec2(center.x + rx, center.y + ry);
    };

    ImVec2 p1 = rotate_point(-half, -half);
    ImVec2 p2 = rotate_point(half, -half);
    ImVec2 p3 = rotate_point(half, half);
    ImVec2 p4 = rotate_point(-half, half);

    ImVec2 uv_min = uiIcon.uv0;
    ImVec2 uv_max = uiIcon.uv1;
    ImVec2 uv0 = ImVec2(uv_min.x, uv_min.y);
    ImVec2 uv1 = ImVec2(uv_max.x, uv_min.y);
    ImVec2 uv2 = ImVec2(uv_max.x, uv_max.y);
    ImVec2 uv3 = ImVec2(uv_min.x, uv_max.y);

    ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
    dl->AddImageQuad(uiIcon.textureId, p1, p2, p3, p4, uv0, uv1, uv2, uv3, col);

    ImGui::Dummy(ImVec2(size, size));
}


void Icon(const std::string& name,
          float size,
          const ImVec4& color,
          const char* fallbackAtlasName,
          const char* fallbackImageName) {
    bool usedAtlas = false;
    if (!name.empty()) {
        ImTextureID customTex = CustomIconLoader::Get().GetIcon(name);
        if (customTex != (ImTextureID)-1) {
            ImGui::Image(customTex, ImVec2(size, size), ImVec2(0,0), ImVec2(1,1), color);
            return;
        }

        UIIcon uiIcon{};
        IconLoader::getUIIcon(name.c_str(), &uiIcon);
        if (uiIcon.textureId != IL_NO_TEXTURE) {
            usedAtlas = true;
            IconLoader::icon(name.c_str(), size, color);
        }
    }
    if (!usedAtlas) {
        const std::string imageName = !name.empty() ? name : (fallbackImageName ? std::string(fallbackImageName) : std::string());
        if (!imageName.empty()) {
            SkyImage& img = IconLoader::getImage(imageName);
            if (img.textureId != IL_NO_TEXTURE) {
                ImGui::Image(img.textureId, ImVec2(size, size));
                return;
            }
        }
        IconLoader::icon(fallbackAtlasName ? fallbackAtlasName : "UiMiscQuestion", size, color);
    }
}

bool IconButton(const std::string& name,
                float size,
                const ImVec4& color,
                const char* fallbackAtlasName,
                const char* fallbackImageName) {
    bool usedAtlas = false;
    bool clicked = false;
    if (!name.empty()) {
        ImTextureID customTex = CustomIconLoader::Get().GetIcon(name);
        if (customTex != (ImTextureID)-1) {
            ImGui::PushID(name.c_str());
            clicked = ImGui::ImageButton(customTex, ImVec2(size, size), ImVec2(0,0), ImVec2(1,1), -1, ImVec4(0,0,0,0), color);
            ImGui::PopID();
            return clicked;
        }

        UIIcon uiIcon{};
        IconLoader::getUIIcon(name.c_str(), &uiIcon);
        if (uiIcon.textureId != IL_NO_TEXTURE) {
            usedAtlas = true;
            clicked = IconLoader::iconButton(name.c_str(), size, color);
        }
    }
    if (!usedAtlas) {
        const std::string imageName = !name.empty() ? name : (fallbackImageName ? std::string(fallbackImageName) : std::string());
        if (!imageName.empty()) {
            SkyImage& img = IconLoader::getImage(imageName);
            if (img.textureId != IL_NO_TEXTURE) {
                clicked = ImGui::ImageButton(img.textureId, ImVec2(size, size));
                return clicked;
            }
        }
        clicked = IconLoader::iconButton(fallbackAtlasName ? fallbackAtlasName : "UiMiscQuestion", size, color);
    }
    return clicked;
}


void IconRotated(const std::string& name,
                 float size,
                 float rotationDegrees,
                 const ImVec4& color,
                 const char* fallbackAtlasName,
                 const char* fallbackImageName) {
    bool usedAtlas = false;
    if (!name.empty()) {
        UIIcon uiIcon{};
        IconLoader::getUIIcon(name.c_str(), &uiIcon);
        if (uiIcon.textureId != IL_NO_TEXTURE) {
            usedAtlas = true;
            DrawRotatedIcon(uiIcon, size, rotationDegrees, color);
        }
    }
    if (!usedAtlas) {
        if (fallbackAtlasName) {
            UIIcon uiIcon{};
            IconLoader::getUIIcon(fallbackAtlasName, &uiIcon);
            if (uiIcon.textureId != IL_NO_TEXTURE) {
                DrawRotatedIcon(uiIcon, size, rotationDegrees, color);
                return;
            }
        }
        Icon(name, size, color, fallbackAtlasName, fallbackImageName);
    }
}

bool IconButtonRotated(const std::string& name,
                       float size,
                       float rotationDegrees,
                       const ImVec4& color,
                       const char* fallbackAtlasName,
                       const char* fallbackImageName) {
    bool usedAtlas = false;
    bool clicked = false;
    if (!name.empty()) {
        UIIcon uiIcon{};
        IconLoader::getUIIcon(name.c_str(), &uiIcon);
        if (uiIcon.textureId != IL_NO_TEXTURE) {
            usedAtlas = true;
            ImVec2 p = ImGui::GetCursorScreenPos();
            DrawRotatedIcon(uiIcon, size, rotationDegrees, color);
            ImGui::SetCursorScreenPos(p);
            clicked = ImGui::InvisibleButton("##rotated_icon", ImVec2(size, size));
        }
    }
    if (!usedAtlas) {
        if (fallbackAtlasName) {
            UIIcon uiIcon{};
            IconLoader::getUIIcon(fallbackAtlasName, &uiIcon);
            if (uiIcon.textureId != IL_NO_TEXTURE) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                DrawRotatedIcon(uiIcon, size, rotationDegrees, color);
                ImGui::SetCursorScreenPos(p);
                clicked = ImGui::InvisibleButton("##rotated_icon", ImVec2(size, size));
                return clicked;
            }
        }
        clicked = IconButton(name, size, color, fallbackAtlasName, fallbackImageName);
    }
    return clicked;
}

}}}
