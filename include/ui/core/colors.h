#ifndef TSM_V2_UI_COLORS_H
#define TSM_V2_UI_COLORS_H

#include <cstdint>

namespace modui {

using Col32 = std::uint32_t;

#ifndef MODUI_COLOR_RGBA
#define MODUI_COLOR_RGBA(r, g, b, a) \
    (static_cast<modui::Col32>(((static_cast<modui::Col32>(r) & 0xFF) << 24) | \
                               ((static_cast<modui::Col32>(g) & 0xFF) << 16) | \
                               ((static_cast<modui::Col32>(b) & 0xFF) << 8)  | \
                               ((static_cast<modui::Col32>(a) & 0xFF))))
#endif

#ifndef MODUI_COLOR_HEX
#define MODUI_COLOR_HEX(argb) (modui::Color::from_argb(static_cast<modui::Col32>(argb)).get())
#endif

class Color {
public:
    explicit Color(Col32 color = 0) : _color{color} {}

    static inline Color from_argb(Col32 argb) {
        const Col32 a = (argb >> 24) & 0xFF;
        const Col32 r = (argb >> 16) & 0xFF;
        const Col32 g = (argb >> 8)  & 0xFF;
        const Col32 b = (argb >> 0)  & 0xFF;
        return Color(MODUI_COLOR_RGBA(r,g,b,a));
    }

    static inline Color from_rgba(Col32 rgba) { return Color(rgba); }

    inline Col32 get() const { return _color; }

    inline Col32 get_alpha_applied(float alpha) const {
        const Col32 a = static_cast<Col32>(((get() & 0xFF) * alpha));
        const Col32 rgb = (get() >> 8) & 0xFFFFFF;
        return (rgb << 8) | (a & 0xFF);
    }

    inline void set(Col32 color) { _color = color; }

    inline operator Col32() const { return _color; }

    inline Color& operator=(Col32 c) {
        _color = c;
        return *this;
    }

private:
    Col32 _color;
};

}

#endif
