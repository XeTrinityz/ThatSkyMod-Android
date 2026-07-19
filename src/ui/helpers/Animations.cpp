#include <ui/helpers/Animations.h>
#include <imgui/imgui.h>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>

namespace tsm { namespace ui { namespace helpers {


static float EaseLinear(float t) { return t; }

static float EaseInQuad(float t) { return t * t; }
static float EaseOutQuad(float t) { return t * (2.0f - t); }
static float EaseInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

static float EaseInCubic(float t) { return t * t * t; }
static float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
static float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static float EaseInQuart(float t) { return t * t * t * t; }
static float EaseOutQuart(float t) { return 1.0f - std::pow(1.0f - t, 4.0f); }
static float EaseInOutQuart(float t) {
    return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
}

static float EaseInBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}
static float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}
static float EaseInOutBack(float t) {
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    return t < 0.5f
        ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
        : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
}

static float EaseOutBounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}
static float EaseInBounce(float t) { return 1.0f - EaseOutBounce(1.0f - t); }
static float EaseInOutBounce(float t) {
    return t < 0.5f
        ? (1.0f - EaseOutBounce(1.0f - 2.0f * t)) / 2.0f
        : (1.0f + EaseOutBounce(2.0f * t - 1.0f)) / 2.0f;
}

static float EaseOutElastic(float t) {
    const float c4 = (2.0f * 3.14159265f) / 3.0f;
    return t == 0.0f ? 0.0f : (t == 1.0f ? 1.0f : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f);
}
static float EaseInElastic(float t) { return 1.0f - EaseOutElastic(1.0f - t); }
static float EaseInOutElastic(float t) {
    const float c5 = (2.0f * 3.14159265f) / 4.5f;
    return t == 0.0f ? 0.0f : (t == 1.0f ? 1.0f :
        t < 0.5f
            ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f
            : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f);
}

float Ease(float t, EaseType type) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type) {
        case EaseType::Linear: return EaseLinear(t);
        case EaseType::InQuad: return EaseInQuad(t);
        case EaseType::OutQuad: return EaseOutQuad(t);
        case EaseType::InOutQuad: return EaseInOutQuad(t);
        case EaseType::InCubic: return EaseInCubic(t);
        case EaseType::OutCubic: return EaseOutCubic(t);
        case EaseType::InOutCubic: return EaseInOutCubic(t);
        case EaseType::InQuart: return EaseInQuart(t);
        case EaseType::OutQuart: return EaseOutQuart(t);
        case EaseType::InOutQuart: return EaseInOutQuart(t);
        case EaseType::InBack: return EaseInBack(t);
        case EaseType::OutBack: return EaseOutBack(t);
        case EaseType::InOutBack: return EaseInOutBack(t);
        case EaseType::InElastic: return EaseInElastic(t);
        case EaseType::OutElastic: return EaseOutElastic(t);
        case EaseType::InOutElastic: return EaseInOutElastic(t);
        case EaseType::InBounce: return EaseInBounce(t);
        case EaseType::OutBounce: return EaseOutBounce(t);
        case EaseType::InOutBounce: return EaseInOutBounce(t);
        default: return t;
    }
}


Spring::Spring(float initial, const SpringConfig& config)
    : value_(initial), velocity_(0.0f), target_(initial), config_(config) {}

void Spring::SetTarget(float target) {
    target_ = target;
}

void Spring::SetValue(float value) {
    value_ = value;
    velocity_ = 0.0f;
}

void Spring::SetConfig(const SpringConfig& config) {
    config_ = config;
}

float Spring::Update(float dt) {
    if (std::abs(value_ - target_) < config_.epsilon && std::abs(velocity_) < config_.epsilon) {
        value_ = target_;
        velocity_ = 0.0f;
        return value_;
    }

    float force = -config_.stiffness * (value_ - target_);
    float damping = -config_.damping * velocity_;
    float acceleration = (force + damping) / config_.mass;

    velocity_ += acceleration * dt;
    value_ += velocity_ * dt;

    return value_;
}

bool Spring::IsSettled() const {
    return std::abs(value_ - target_) < config_.epsilon && std::abs(velocity_) < config_.epsilon;
}


Tween::Tween(float start, float end, const TweenConfig& config)
    : start_value_(start), end_value_(end), current_value_(start),
      elapsed_(0.0f), config_(config), started_(false), complete_(false) {}

void Tween::Start() {
    started_ = true;
    elapsed_ = 0.0f;
    complete_ = false;
}

void Tween::Reset() {
    started_ = false;
    elapsed_ = 0.0f;
    complete_ = false;
    current_value_ = start_value_;
}

float Tween::Update(float dt) {
    if (complete_) return current_value_;
    if (!started_) return start_value_;

    elapsed_ += dt;

    if (elapsed_ < config_.delay) {
        return start_value_;
    }

    float t = (elapsed_ - config_.delay) / config_.duration;
    if (t >= 1.0f) {
        t = 1.0f;
        complete_ = true;
    }

    float eased = Ease(t, config_.easing);
    current_value_ = start_value_ + (end_value_ - start_value_) * eased;
    return current_value_;
}


AnimatedFloat::AnimatedFloat(float initial) : spring_(initial) {}

void AnimatedFloat::SetTarget(float target) {
    spring_.SetTarget(target);
}

void AnimatedFloat::SetImmediate(float value) {
    spring_.SetValue(value);
    spring_.SetTarget(value);
}

void AnimatedFloat::SetSpringConfig(const SpringConfig& config) {
    spring_.SetConfig(config);
}

float AnimatedFloat::Update(float dt) {
    return spring_.Update(dt);
}

static std::unordered_map<const void*, AnimatedFloat> g_animated_floats;

AnimatedFloat& GetAnimatedFloat(const void* id, float initial_value) {
    auto it = g_animated_floats.find(id);
    if (it == g_animated_floats.end()) {
        g_animated_floats.emplace(id, AnimatedFloat(initial_value));
        return g_animated_floats[id];
    }
    return it->second;
}


AnimatedColor::AnimatedColor(const ImVec4& initial)
    : r_(initial.x), g_(initial.y), b_(initial.z), a_(initial.w) {}

void AnimatedColor::SetTarget(const ImVec4& target) {
    r_.SetTarget(target.x);
    g_.SetTarget(target.y);
    b_.SetTarget(target.z);
    a_.SetTarget(target.w);
}

void AnimatedColor::SetImmediate(const ImVec4& color) {
    r_.SetValue(color.x);
    g_.SetValue(color.y);
    b_.SetValue(color.z);
    a_.SetValue(color.w);
}

void AnimatedColor::SetSpringConfig(const SpringConfig& config) {
    r_.SetConfig(config);
    g_.SetConfig(config);
    b_.SetConfig(config);
    a_.SetConfig(config);
}

ImVec4 AnimatedColor::Update(float dt) {
    return ImVec4(r_.Update(dt), g_.Update(dt), b_.Update(dt), a_.Update(dt));
}

ImVec4 AnimatedColor::Get() const {
    return ImVec4(r_.GetValue(), g_.GetValue(), b_.GetValue(), a_.GetValue());
}

bool AnimatedColor::IsAnimating() const {
    return !r_.IsSettled() || !g_.IsSettled() || !b_.IsSettled() || !a_.IsSettled();
}

static std::unordered_map<const void*, AnimatedColor> g_animated_colors;

AnimatedColor& GetAnimatedColor(const void* id, const ImVec4& initial) {
    auto it = g_animated_colors.find(id);
    if (it == g_animated_colors.end()) {
        g_animated_colors.emplace(id, AnimatedColor(initial));
        return g_animated_colors[id];
    }
    return it->second;
}


AnimatedTransform::AnimatedTransform()
    : pos_x_(0.0f), pos_y_(0.0f), scale_x_(1.0f), scale_y_(1.0f),
      rotation_(0.0f), alpha_(1.0f) {}

void AnimatedTransform::SetTarget(const Transform2D& target) {
    pos_x_.SetTarget(target.position.x);
    pos_y_.SetTarget(target.position.y);
    scale_x_.SetTarget(target.scale.x);
    scale_y_.SetTarget(target.scale.y);
    rotation_.SetTarget(target.rotation);
    alpha_.SetTarget(target.alpha);
}

void AnimatedTransform::SetImmediate(const Transform2D& transform) {
    pos_x_.SetValue(transform.position.x);
    pos_y_.SetValue(transform.position.y);
    scale_x_.SetValue(transform.scale.x);
    scale_y_.SetValue(transform.scale.y);
    rotation_.SetValue(transform.rotation);
    alpha_.SetValue(transform.alpha);
}

void AnimatedTransform::SetSpringConfig(const SpringConfig& config) {
    pos_x_.SetConfig(config);
    pos_y_.SetConfig(config);
    scale_x_.SetConfig(config);
    scale_y_.SetConfig(config);
    rotation_.SetConfig(config);
    alpha_.SetConfig(config);
}

Transform2D AnimatedTransform::Update(float dt) {
    Transform2D result;
    result.position = ImVec2(pos_x_.Update(dt), pos_y_.Update(dt));
    result.scale = ImVec2(scale_x_.Update(dt), scale_y_.Update(dt));
    result.rotation = rotation_.Update(dt);
    result.alpha = alpha_.Update(dt);
    return result;
}

Transform2D AnimatedTransform::Get() const {
    Transform2D result;
    result.position = ImVec2(pos_x_.GetValue(), pos_y_.GetValue());
    result.scale = ImVec2(scale_x_.GetValue(), scale_y_.GetValue());
    result.rotation = rotation_.GetValue();
    result.alpha = alpha_.GetValue();
    return result;
}

bool AnimatedTransform::IsAnimating() const {
    return !pos_x_.IsSettled() || !pos_y_.IsSettled() ||
           !scale_x_.IsSettled() || !scale_y_.IsSettled() ||
           !rotation_.IsSettled() || !alpha_.IsSettled();
}


static std::unordered_map<const void*, std::vector<RippleState>> g_ripples;

void StartRipple(const void* id, const ImVec2& position) {
    RippleState ripple;
    ripple.center = position;
    ripple.radius = 0.0f;
    ripple.max_radius = 0.0f;
    ripple.alpha = 0.5f;
    ripple.time = 0.0f;
    ripple.active = true;

    g_ripples[id].push_back(ripple);
}

void UpdateAndDrawRipples(const void* id, ImDrawList* draw_list, const ImVec2& rect_min,
                          const ImVec2& rect_max, const ImVec4& color, float corner_radius) {
    auto it = g_ripples.find(id);
    if (it == g_ripples.end() || it->second.empty()) return;

    float dt = ImGui::GetIO().DeltaTime;
    auto& ripples = it->second;

    for (auto& ripple : ripples) {
        if (!ripple.active) continue;

        if (ripple.max_radius == 0.0f) {
            float dx = std::max(std::abs(ripple.center.x - rect_min.x),
                               std::abs(ripple.center.x - rect_max.x));
            float dy = std::max(std::abs(ripple.center.y - rect_min.y),
                               std::abs(ripple.center.y - rect_max.y));
            ripple.max_radius = std::sqrt(dx * dx + dy * dy) * 1.2f;
        }

        ripple.time += dt;
        float duration = 0.8f;
        float progress = ripple.time / duration;

        if (progress >= 1.0f) {
            ripple.active = false;
            continue;
        }

        ripple.radius = ripple.max_radius * EaseOutCubic(progress);
        ripple.alpha = 0.35f * (1.0f - EaseOutQuad(progress));

        draw_list->PushClipRect(rect_min, rect_max, true);
        ImU32 ripple_color = ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, ripple.alpha));
        draw_list->AddCircleFilled(ripple.center, ripple.radius, ripple_color, 48);
        draw_list->PopClipRect();
    }

    ripples.erase(std::remove_if(ripples.begin(), ripples.end(),
        [](const RippleState& r) { return !r.active; }), ripples.end());
}


void SkeletonRect(const ImVec2& size, float corner_radius) {
    static float skeleton_time = 0.0f;
    skeleton_time += ImGui::GetIO().DeltaTime;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 min = pos;
    ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);

    ImVec4 base_color = ImVec4(0.2f, 0.22f, 0.25f, 1.0f);
    draw_list->AddRectFilled(min, max, ImGui::GetColorU32(base_color), corner_radius);

    float shimmer_pos = std::fmod(skeleton_time * 0.8f, 2.0f) - 1.0f;
    float shimmer_width = 0.5f;

    for (int i = 0; i < 3; ++i) {
        float offset = (i - 1) * 0.15f;
        float x = shimmer_pos + offset;
        float alpha = 1.0f - std::abs(offset) * 2.0f;
        alpha *= 0.15f;

        if (x > -shimmer_width && x < 1.0f + shimmer_width) {
            float x_start = min.x + std::max(0.0f, (x - shimmer_width * 0.5f)) * size.x;
            float x_end = min.x + std::min(1.0f, (x + shimmer_width * 0.5f)) * size.x;

            ImU32 shimmer_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));
            draw_list->AddRectFilled(ImVec2(x_start, min.y), ImVec2(x_end, max.y), shimmer_color, corner_radius);
        }
    }

    ImGui::Dummy(size);
}

void SkeletonCircle(float radius) {
    static float skeleton_time = 0.0f;
    skeleton_time += ImGui::GetIO().DeltaTime;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);

    ImVec4 base_color = ImVec4(0.2f, 0.22f, 0.25f, 1.0f);
    draw_list->AddCircleFilled(center, radius, ImGui::GetColorU32(base_color), 64);

    float shimmer_angle = std::fmod(skeleton_time * 2.0f, 6.28318f);
    float shimmer_width = 1.0f;

    for (int i = 0; i < 32; ++i) {
        float angle = (i / 32.0f) * 6.28318f;
        float diff = std::abs(angle - shimmer_angle);
        if (diff > 3.14159f) diff = 6.28318f - diff;

        if (diff < shimmer_width) {
            float alpha = (1.0f - diff / shimmer_width) * 0.2f;
            ImVec2 p1 = ImVec2(center.x + std::cos(angle) * radius * 0.7f,
                              center.y + std::sin(angle) * radius * 0.7f);
            ImVec2 p2 = ImVec2(center.x + std::cos(angle) * radius,
                              center.y + std::sin(angle) * radius);
            ImU32 shimmer_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));
            draw_list->AddLine(p1, p2, shimmer_color, 2.0f);
        }
    }

    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}


void ShimmerRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                 const ImVec4& base_color, float corner_radius) {
    static float shimmer_time = 0.0f;
    shimmer_time += ImGui::GetIO().DeltaTime;

    draw_list->AddRectFilled(min, max, ImGui::GetColorU32(base_color), corner_radius);

    float width = max.x - min.x;
    float height = max.y - min.y;
    float shimmer_pos = std::fmod(shimmer_time * 0.5f, 2.0f) - 1.0f;
    float shimmer_width = 0.3f;

    ImVec2 gradient_min = min;
    ImVec2 gradient_max = max;

    float x_offset = shimmer_pos * (width + height);

    ImVec4 shimmer_color = ImVec4(
        std::min(1.0f, base_color.x + 0.2f),
        std::min(1.0f, base_color.y + 0.2f),
        std::min(1.0f, base_color.z + 0.2f),
        base_color.w
    );

    float t = (shimmer_pos + 1.0f) * 0.5f;
    if (t > 0.0f && t < 1.0f) {
        float alpha_peak = std::sin(t * 3.14159f) * 0.3f;
        ImVec4 overlay = ImVec4(shimmer_color.x, shimmer_color.y, shimmer_color.z, alpha_peak);
        draw_list->AddRectFilled(min, max, ImGui::GetColorU32(overlay), corner_radius);
    }
}

}}}
