
#ifndef TSM_V2_UI_WIDGETS_INTERNAL_H
#define TSM_V2_UI_WIDGETS_INTERNAL_H

#include <imgui/imgui.h>

namespace tsm { namespace ui { namespace widgets {

ImVec4 GetAccentColor();
inline ImVec4 kMutedText() { return ImVec4(0.604f, 0.627f, 0.627f, 1.0f); }
inline ImVec4 kInactive()  { return ImVec4(0.165f, 0.176f, 0.180f, 1.0f); }
inline ImVec4 kBorder()    { return ImVec4(0.133f, 0.149f, 0.153f, 1.0f); }

float GetCardPadding();

void TextEllipsisSingleLine(const char* text, float max_w);

bool DrawSwitch(bool* v, float height);

}}}

#endif
