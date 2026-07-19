#pragma once

#include <imgui/imgui.h>
#include <string>

namespace tsm { namespace ui { namespace widgets {


void Icon(const std::string& name,
          float size,
          const ImVec4& color = ImVec4(1,1,1,1),
          const char* fallbackAtlasName = "UiMiscQuestion",
          const char* fallbackImageName = nullptr);

bool IconButton(const std::string& name,
                float size,
                const ImVec4& color = ImVec4(1,1,1,1),
                const char* fallbackAtlasName = "UiMiscQuestion",
                const char* fallbackImageName = nullptr);

void IconRotated(const std::string& name,
                 float size,
                 float rotationDegrees,
                 const ImVec4& color = ImVec4(1,1,1,1),
                 const char* fallbackAtlasName = "UiMiscQuestion",
                 const char* fallbackImageName = nullptr);

bool IconButtonRotated(const std::string& name,
                       float size,
                       float rotationDegrees,
                       const ImVec4& color = ImVec4(1,1,1,1),
                       const char* fallbackAtlasName = "UiMiscQuestion",
                       const char* fallbackImageName = nullptr);

}}}
