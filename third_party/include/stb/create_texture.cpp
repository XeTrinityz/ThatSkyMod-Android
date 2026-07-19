#include "create_texture.h"

#define TAG_TEXTURE "Texture"

TextureInfo createTexture(ImgBase *img)
{
    TextureInfo textureInfo;
    int w, h, n;
    stbi_uc *data = stbi_load_from_memory(img->buff, img->size, &w, &h, &n, 0);

    GLuint texture;
    glGenTextures(1, &texture);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (n == 3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    // Correct assignment for ImGui's ImTextureID
    textureInfo.textureId = (ImTextureID)(uintptr_t)texture;
    textureInfo.size = ImVec2((float)w, (float)h);
    textureInfo.buff = img->buff;
    textureInfo.len = img->size;

    return textureInfo;
}


ImTextureID getTextureID(ImgBase *img)
{
	static std::vector<TextureInfo> textureInfoCache;
	for(auto & textureInfo : textureInfoCache)
		if(textureInfo.buff == img->buff)
			return textureInfo.textureId;
	TextureInfo textureInfo1 = createTexture(img);
	textureInfoCache.push_back(textureInfo1);
	return textureInfo1.textureId;
}