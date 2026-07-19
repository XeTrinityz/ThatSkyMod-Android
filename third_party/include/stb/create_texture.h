#pragma once

/* To include an image in your library, you will need to follow these steps:

	1. Add the image to the resources folder, located in the same directory as the main.cpp file.
	2. Include the image in the CMakeLists.txt file by adding it to the list of resources to be compiled, like so:
		set(INC_RES_LIST
		image_1_name
		image_2_name
		image_3_name
		)

	3. Include the image in your cpp file by using the INCLUDE_IMG macro, including the image extension:
	INCLUDE_IMG(image_1_name_png)
	4. To pass the image to ImGui to render it, you will need to pass the ID of the texture.
 	5. You can do this by calling the getTextureID() function, like so: getTextureID(&image_1_name_png)
	6. Alternatively, you can use the provided macro TEXTURE_ID(image_1_name_png) for a shorter syntax.

	* Example:
		ImGui::Image(ImTextureID(getTextureID(&image_1_png)), ImVec2(256, 256));
	or
		ImGui::Image(TEXTURE_ID(image_1_png), ImVec2(256, 256));
*/

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <vector>
#include "stb_image.h"
#include "imgui.h"
//#include "../misc/Logger.h"

#define INCLUDE_RESOURCE(resname) \
    extern char _binary_##resname##_start[]; \
    extern char _binary_##resname##_size[]; \
    extern char _binary_##resname##_end[];

// macro to include images
#define INCLUDE_IMG(imgName) \
   	ImgBase imgName{};          \
    do{ \
        INCLUDE_RESOURCE(imgName) \
        imgName.buff = (uint8_t*)_binary_##imgName##_start; \
        imgName.size = (uint32_t)(_binary_##imgName##_end - _binary_##imgName##_start); \
    } while(false);

// macro to get the id of the texture
#define TEXTURE_ID(texture) getTextureID(&texture)

typedef struct {
	uint8_t* buff;
	int len;
	ImTextureID textureId;
	ImVec2 size;
}TextureInfo;

typedef struct{
	uint8_t* buff;
	uint32_t size;
}ImgBase;

extern TextureInfo createTexture(ImgBase *img);

extern ImTextureID getTextureID(ImgBase *img);