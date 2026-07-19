#pragma once

#include <imgui/imgui.h>
#include <unordered_map>
#include <cmath>

namespace tsm { namespace ui { namespace helpers {



enum class EaseType {
    Linear,
    InQuad, OutQuad, InOutQuad,
    InCubic, OutCubic, InOutCubic,
    InQuart, OutQuart, InOutQuart,
    InBack, OutBack, InOutBack,
    InElastic, OutElastic, InOutElastic,
    InBounce, OutBounce, InOutBounce
};

float Ease(float t, EaseType type);


struct SpringConfig {
    float stiffness = 300.0f;
    float damping = 25.0f;
    float mass = 1.0f;
    float epsilon = 0.001f;

    static SpringConfig Smooth()   { return {180.0f, 20.0f, 1.0f, 0.001f}; }
    static SpringConfig Snappy()   { return {400.0f, 28.0f, 1.0f, 0.001f}; }
    static SpringConfig Bouncy()   { return {300.0f, 15.0f, 1.0f, 0.001f}; }
    static SpringConfig Gentle()   { return {120.0f, 18.0f, 1.0f, 0.001f}; }
};

class Spring {
public:
    Spring(float initial = 0.0f, const SpringConfig& config = SpringConfig::Smooth());

    void SetTarget(float target);
    void SetValue(float value);
    void SetConfig(const SpringConfig& config);

    float Update(float dt);
    float GetValue() const { return value_; }
    float GetTarget() const { return target_; }
    bool IsSettled() const;

private:
    float value_;
    float velocity_;
    float target_;
    SpringConfig config_;
};


struct TweenConfig {
    float duration = 0.3f;
    EaseType easing = EaseType::OutCubic;
    float delay = 0.0f;
};

class Tween {
public:
    Tween(float start, float end, const TweenConfig& config = TweenConfig());

    void Start();
    void Reset();
    float Update(float dt);
    float GetValue() const { return current_value_; }
    bool IsComplete() const { return complete_; }

private:
    float start_value_;
    float end_value_;
    float current_value_;
    float elapsed_;
    TweenConfig config_;
    bool started_;
    bool complete_;
};


class AnimatedFloat {
public:
    AnimatedFloat(float initial = 0.0f);

    void SetTarget(float target);
    void SetImmediate(float value);
    void SetSpringConfig(const SpringConfig& config);

    float Update(float dt);
    float Get() const { return spring_.GetValue(); }
    bool IsAnimating() const { return !spring_.IsSettled(); }

private:
    Spring spring_;
};

AnimatedFloat& GetAnimatedFloat(const void* id, float initial_value = 0.0f);


class AnimatedColor {
public:
    AnimatedColor(const ImVec4& initial = ImVec4(1,1,1,1));

    void SetTarget(const ImVec4& target);
    void SetImmediate(const ImVec4& color);
    void SetSpringConfig(const SpringConfig& config);

    ImVec4 Update(float dt);
    ImVec4 Get() const;
    bool IsAnimating() const;

private:
    Spring r_, g_, b_, a_;
};

AnimatedColor& GetAnimatedColor(const void* id, const ImVec4& initial = ImVec4(1,1,1,1));


struct Transform2D {
    ImVec2 position = ImVec2(0, 0);
    ImVec2 scale = ImVec2(1, 1);
    float rotation = 0.0f;
    float alpha = 1.0f;
};

class AnimatedTransform {
public:
    AnimatedTransform();

    void SetTarget(const Transform2D& target);
    void SetImmediate(const Transform2D& transform);
    void SetSpringConfig(const SpringConfig& config);

    Transform2D Update(float dt);
    Transform2D Get() const;
    bool IsAnimating() const;

private:
    Spring pos_x_, pos_y_;
    Spring scale_x_, scale_y_;
    Spring rotation_;
    Spring alpha_;
};


struct RippleState {
    ImVec2 center;
    float radius;
    float max_radius;
    float alpha;
    float time;
    bool active;
};

void StartRipple(const void* id, const ImVec2& position);

void UpdateAndDrawRipples(const void* id, ImDrawList* draw_list, const ImVec2& rect_min,
                          const ImVec2& rect_max, const ImVec4& color, float corner_radius = 0.0f);


void SkeletonRect(const ImVec2& size, float corner_radius = 6.0f);

void SkeletonCircle(float radius);


void ShimmerRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                 const ImVec4& base_color, float corner_radius = 0.0f);

}}}
