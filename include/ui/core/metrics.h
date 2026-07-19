#ifndef TSM_V2_UI_METRICS_H
#define TSM_V2_UI_METRICS_H

namespace tsm { namespace ui { namespace metrics {

void  SetUiScale(float scale);
float GetUiScale();

void  SetScreenDensity(float density);
float GetScreenDensity();

float dp(float size);
float dp_raw(float size);

}}}

namespace modui {
    inline void set_screen_density(float density) { ::tsm::ui::metrics::SetScreenDensity(density); }
    inline void set_ui_scale(float scale)        { ::tsm::ui::metrics::SetUiScale(scale); }
    inline float dp(float x)                     { return ::tsm::ui::metrics::dp(x); }
    inline float dp_raw(float x)                 { return ::tsm::ui::metrics::dp_raw(x); }
}

#ifndef DP
#define DP(x) ::tsm::ui::metrics::dp((x))
#endif
#ifndef DP_RAW
#define DP_RAW(x) ::tsm::ui::metrics::dp_raw((x))
#endif

#endif
