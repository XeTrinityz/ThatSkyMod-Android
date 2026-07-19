#include <iconloader/CustomIconLoader.h>
#include <stb/stb_image.h>
#include <GLES3/gl3.h>
#include <vector>
#include <utils/logging/log.h>

#include <iconloader/GitHub_bmp.h>

namespace tsm { namespace ui { namespace widgets {

CustomIconLoader& CustomIconLoader::Get() {
    static CustomIconLoader instance;
    return instance;
}

CustomIconLoader::~CustomIconLoader() {
    for (auto& pair : icons_) {
        GLuint tex = (GLuint)(uintptr_t)pair.second;
        glDeleteTextures(1, &tex);
    }
    icons_.clear();
}

ImTextureID CustomIconLoader::GetIcon(const std::string& name) {
    auto it = icons_.find(name);
    if (it != icons_.end()) {
        return it->second;
    }

    if (name == "GitHub") {
        if (kGitHubBmpSize > 0) {
            ImTextureID tex = CreateTextureFromMemory(kGitHubBmpData, kGitHubBmpSize);
            if (tex != (ImTextureID)-1) {
                icons_["GitHub"] = tex;
                return tex;
            }
        }
    }

    return (ImTextureID)-1;
}

void CustomIconLoader::RegisterIcon(const std::string& name, const unsigned char* data, size_t size) {
    if (icons_.count(name)) return;
    ImTextureID tex = CreateTextureFromMemory(data, size);
    if (tex != (ImTextureID)-1) {
        icons_[name] = tex;
    }
}

ImTextureID CustomIconLoader::CreateTextureFromMemory(const unsigned char* data, size_t size) {
    int w, h, n;
    unsigned char* pixels = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!pixels) {
        tsm::log::e("CustomIconLoader: Failed to load image from memory");
        return (ImTextureID)-1;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    stbi_image_free(pixels);

    return (ImTextureID)(uintptr_t)texture;
}

}}}
