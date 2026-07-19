#pragma once

namespace tsm { namespace ui { namespace widgets {


bool ToggleSwitch(const char* label, bool* v);

bool LabeledSliderInt(const char* label, int* v, int v_min, int v_max);
bool LabeledSliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt = "%.1f");

void SectionHeader(const char* title);
void CenterSeparator(const char* text);

void CenteredText(const char* text);
void PaddedText(const char* text);
void PaddedTextDisabled(const char* text);

}}}
