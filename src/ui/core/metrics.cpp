#include <ui/core/metrics.h>

namespace tsm { namespace ui { namespace metrics {

static float s_ui_scale = 1.0f;
static float s_screen_density = 1.6667f;

void SetUiScale(float scale) {
    if (scale > 0.0f)
        s_ui_scale = scale;
}

float GetUiScale() {
    return s_ui_scale;
}

void SetScreenDensity(float density) {
    if (density > 0.0f)
        s_screen_density = density;
}

float GetScreenDensity() {
    return s_screen_density;
}

float dp(float size) {
    return size * s_screen_density * s_ui_scale;
}

float dp_raw(float size) {
    return size * s_screen_density;
}

}}}
