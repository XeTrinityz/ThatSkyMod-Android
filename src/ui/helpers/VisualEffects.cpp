#include <ui/helpers/VisualEffects.h>
#include <imgui/imgui.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace tsm { namespace ui { namespace helpers {

void DrawShadow(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const ShadowConfig& config, float corner_radius) {
    ImVec2 shadow_min = ImVec2(min.x + config.offset.x, min.y + config.offset.y);
    ImVec2 shadow_max = ImVec2(max.x + config.offset.x, max.y + config.offset.y);

    if (config.style == ShadowStyle::Soft || config.style == ShadowStyle::Glow) {
        int layers = (config.style == ShadowStyle::Glow) ? 5 : 3;
        float blur_step = config.blur / layers;

        for (int i = 0; i < layers; ++i) {
            float alpha_mult = 1.0f - (i / float(layers));
            alpha_mult = std::pow(alpha_mult, 0.7f);

            float expansion = i * blur_step;
            ImVec2 layer_min = ImVec2(shadow_min.x - expansion, shadow_min.y - expansion);
            ImVec2 layer_max = ImVec2(shadow_max.x + expansion, shadow_max.y + expansion);

            ImVec4 layer_color = ImVec4(
                config.color.x, config.color.y, config.color.z,
                config.color.w * alpha_mult / layers
            );

            draw_list->AddRectFilled(layer_min, layer_max, ImGui::GetColorU32(layer_color),
                                    corner_radius + expansion);
        }
    }
    else if (config.style == ShadowStyle::Hard) {
        draw_list->AddRectFilled(shadow_min, shadow_max, ImGui::GetColorU32(config.color),
                                corner_radius);
    }
    else if (config.style == ShadowStyle::Inner) {
        float blur = std::min(config.blur, std::min(max.x - min.x, max.y - min.y) * 0.5f);

        draw_list->AddRectFilledMultiColor(
            min, ImVec2(max.x, min.y + blur),
            ImGui::GetColorU32(config.color),
            ImGui::GetColorU32(config.color),
            ImGui::GetColorU32(ImVec4(config.color.x, config.color.y, config.color.z, 0)),
            ImGui::GetColorU32(ImVec4(config.color.x, config.color.y, config.color.z, 0))
        );

        draw_list->AddRectFilledMultiColor(
            min, ImVec2(min.x + blur, max.y),
            ImGui::GetColorU32(config.color),
            ImGui::GetColorU32(ImVec4(config.color.x, config.color.y, config.color.z, 0)),
            ImGui::GetColorU32(ImVec4(config.color.x, config.color.y, config.color.z, 0)),
            ImGui::GetColorU32(config.color)
        );
    }
}

void DrawCircleShadow(ImDrawList* draw_list, const ImVec2& center, float radius,
                      const ShadowConfig& config) {
    ImVec2 shadow_center = ImVec2(center.x + config.offset.x, center.y + config.offset.y);

    if (config.style == ShadowStyle::Soft || config.style == ShadowStyle::Glow) {
        int layers = (config.style == ShadowStyle::Glow) ? 5 : 3;
        float blur_step = config.blur / layers;

        for (int i = 0; i < layers; ++i) {
            float alpha_mult = 1.0f - (i / float(layers));
            alpha_mult = std::pow(alpha_mult, 0.7f);

            float expansion = i * blur_step;
            float layer_radius = radius + expansion;

            ImVec4 layer_color = ImVec4(
                config.color.x, config.color.y, config.color.z,
                config.color.w * alpha_mult / layers
            );

            draw_list->AddCircleFilled(shadow_center, layer_radius,
                                      ImGui::GetColorU32(layer_color), 64);
        }
    }
    else {
        draw_list->AddCircleFilled(shadow_center, radius, ImGui::GetColorU32(config.color), 64);
    }
}

void DrawGlassRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                   const GlassConfig& config, float corner_radius) {
    ImVec4 tint = config.tint_color;
    tint.w *= config.tint_alpha;
    draw_list->AddRectFilled(min, max, ImGui::GetColorU32(tint), corner_radius);

    if (config.use_noise) {
        for (float y = min.y; y < max.y; y += 2.0f) {
            for (float x = min.x; x < max.x; x += 2.0f) {
                float noise = (std::sin(x * 0.1f) * std::cos(y * 0.1f) + 1.0f) * 0.5f;
                if (noise > 0.5f) {
                    ImVec4 noise_color = ImVec4(1, 1, 1, config.noise_strength);
                    draw_list->AddRectFilled(
                        ImVec2(x, y), ImVec2(x + 1, y + 1),
                        ImGui::GetColorU32(noise_color)
                    );
                }
            }
        }
    }

    if (config.border_alpha > 0.0f) {
        ImVec4 border_color = ImVec4(1, 1, 1, config.border_alpha);
        draw_list->AddRect(min, max, ImGui::GetColorU32(border_color), corner_radius, 0, 1.0f);
    }
}

void DrawGradientRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                      const ImVec4& color1, const ImVec4& color2,
                      GradientDirection direction, float corner_radius) {
    ImU32 c1 = ImGui::GetColorU32(color1);
    ImU32 c2 = ImGui::GetColorU32(color2);

    if (corner_radius > 0.0f) {
        int segments = 64;
        float width = max.x - min.x;
        float height = max.y - min.y;

        for (int i = 0; i < segments; ++i) {
            float t1 = i / float(segments);
            float t2 = (i + 1) / float(segments);

            ImVec4 c_start = ImVec4(
                color1.x + (color2.x - color1.x) * t1,
                color1.y + (color2.y - color1.y) * t1,
                color1.z + (color2.z - color1.z) * t1,
                color1.w + (color2.w - color1.w) * t1
            );
            ImVec4 c_end = ImVec4(
                color1.x + (color2.x - color1.x) * t2,
                color1.y + (color2.y - color1.y) * t2,
                color1.z + (color2.z - color1.z) * t2,
                color1.w + (color2.w - color1.w) * t2
            );

            float y1, y2;
            if (direction == GradientDirection::TopToBottom) {
                y1 = min.y + height * t1;
                y2 = min.y + height * t2;
                draw_list->AddRectFilled(ImVec2(min.x, y1), ImVec2(max.x, y2),
                                        ImGui::GetColorU32(c_start));
            }
        }
    }
    else {
        switch (direction) {
            case GradientDirection::TopToBottom:
                draw_list->AddRectFilledMultiColor(min, max, c1, c1, c2, c2);
                break;
            case GradientDirection::BottomToTop:
                draw_list->AddRectFilledMultiColor(min, max, c2, c2, c1, c1);
                break;
            case GradientDirection::LeftToRight:
                draw_list->AddRectFilledMultiColor(min, max, c1, c2, c2, c1);
                break;
            case GradientDirection::RightToLeft:
                draw_list->AddRectFilledMultiColor(min, max, c2, c1, c1, c2);
                break;
            case GradientDirection::Diagonal:
                draw_list->AddRectFilledMultiColor(min, max, c1, c2, c2, c1);
                break;
            case GradientDirection::Radial: {
                ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
                float max_dist = std::sqrt(std::pow(max.x - min.x, 2) + std::pow(max.y - min.y, 2)) * 0.5f;

                for (int i = 20; i >= 0; --i) {
                    float t = i / 20.0f;
                    float dist = max_dist * t;
                    ImVec4 c = ImVec4(
                        color1.x + (color2.x - color1.x) * (1.0f - t),
                        color1.y + (color2.y - color1.y) * (1.0f - t),
                        color1.z + (color2.z - color1.z) * (1.0f - t),
                        color1.w + (color2.w - color1.w) * (1.0f - t)
                    );
                    draw_list->AddRectFilled(
                        ImVec2(center.x - dist, center.y - dist),
                        ImVec2(center.x + dist, center.y + dist),
                        ImGui::GetColorU32(c)
                    );
                }
                break;
            }
        }
    }
}

void DrawGradientRectMulti(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                           const ImVec4* colors, int color_count,
                           GradientDirection direction, float corner_radius) {
    if (color_count < 2) return;

    float height = max.y - min.y;
    float segment_height = height / (color_count - 1);

    for (int i = 0; i < color_count - 1; ++i) {
        ImVec2 seg_min = ImVec2(min.x, min.y + i * segment_height);
        ImVec2 seg_max = ImVec2(max.x, min.y + (i + 1) * segment_height);
        DrawGradientRect(draw_list, seg_min, seg_max, colors[i], colors[i + 1],
                        direction, 0.0f);
    }
}

void DrawGlowRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                  const GlowConfig& config, float corner_radius) {
    float layer_step = config.radius / config.layers;

    for (int i = config.layers; i > 0; --i) {
        float expansion = i * layer_step;
        float alpha_mult = (config.layers - i + 1) / float(config.layers + 1);
        alpha_mult = std::pow(alpha_mult, 1.5f);

        ImVec2 layer_min = ImVec2(min.x - expansion, min.y - expansion);
        ImVec2 layer_max = ImVec2(max.x + expansion, max.y + expansion);

        ImVec4 layer_color = ImVec4(
            config.color.x, config.color.y, config.color.z,
            config.intensity * alpha_mult / config.layers
        );

        draw_list->AddRectFilled(layer_min, layer_max, ImGui::GetColorU32(layer_color),
                                corner_radius + expansion);
    }
}

void DrawGlowCircle(ImDrawList* draw_list, const ImVec2& center, float radius,
                    const GlowConfig& config) {
    float layer_step = config.radius / config.layers;

    for (int i = config.layers; i > 0; --i) {
        float expansion = i * layer_step;
        float alpha_mult = (config.layers - i + 1) / float(config.layers + 1);
        alpha_mult = std::pow(alpha_mult, 1.5f);

        float layer_radius = radius + expansion;

        ImVec4 layer_color = ImVec4(
            config.color.x, config.color.y, config.color.z,
            config.intensity * alpha_mult / config.layers
        );

        draw_list->AddCircleFilled(center, layer_radius, ImGui::GetColorU32(layer_color), 64);
    }
}

ParticleSystem::ParticleSystem(int max_particles) : max_particles_(max_particles) {
    particles_.reserve(max_particles);
}

void ParticleSystem::Emit(const ImVec2& position, const ImVec2& velocity,
                          const ImVec4& color, float lifetime, float size) {
    if (particles_.size() >= max_particles_) return;

    Particle p;
    p.position = position;
    p.velocity = velocity;
    p.color = color;
    p.lifetime = lifetime;
    p.max_lifetime = lifetime;
    p.size = size;
    p.rotation = 0.0f;
    p.rotation_speed = (rand() % 100 - 50) / 25.0f;

    particles_.push_back(p);
}

void ParticleSystem::EmitBurst(const ImVec2& position, int count, float speed,
                               const ImVec4& color, float lifetime, float size) {
    for (int i = 0; i < count; ++i) {
        float angle = (i / float(count)) * 6.28318f;
        ImVec2 velocity = ImVec2(std::cos(angle) * speed, std::sin(angle) * speed);
        Emit(position, velocity, color, lifetime, size);
    }
}

void ParticleSystem::Update(float dt) {
    for (auto& p : particles_) {
        p.lifetime -= dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.rotation += p.rotation_speed * dt;

        float life_ratio = p.lifetime / p.max_lifetime;
        p.color.w = life_ratio;
    }

    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
            [](const Particle& p) { return p.lifetime <= 0.0f; }),
        particles_.end()
    );
}

void ParticleSystem::Draw(ImDrawList* draw_list) {
    for (const auto& p : particles_) {
        draw_list->AddCircleFilled(p.position, p.size, ImGui::GetColorU32(p.color), 16);
    }
}

void ParticleSystem::Clear() {
    particles_.clear();
}

static ParticleSystem g_particle_system(200);

ParticleSystem& GetParticleSystem() {
    return g_particle_system;
}

void DrawSparkle(ImDrawList* draw_list, const ImVec2& position, float size,
                 const ImVec4& color, float time) {
    float pulse = (std::sin(time * 6.0f) + 1.0f) * 0.5f;
    float current_size = size * (0.7f + pulse * 0.3f);

    for (int i = 0; i < 4; ++i) {
        float angle = (i / 4.0f) * 6.28318f + time;
        ImVec2 p1 = ImVec2(
            position.x + std::cos(angle) * current_size * 0.3f,
            position.y + std::sin(angle) * current_size * 0.3f
        );
        ImVec2 p2 = ImVec2(
            position.x + std::cos(angle) * current_size,
            position.y + std::sin(angle) * current_size
        );

        ImVec4 line_color = color;
        line_color.w *= pulse;
        draw_list->AddLine(p1, p2, ImGui::GetColorU32(line_color), 2.0f);
    }

    draw_list->AddCircleFilled(position, size * 0.2f, ImGui::GetColorU32(color), 16);
}

void DrawPulsingRing(ImDrawList* draw_list, const ImVec2& center, float base_radius,
                     const ImVec4& color, float time) {
    float pulse = (std::sin(time * 3.0f) + 1.0f) * 0.5f;
    float current_radius = base_radius * (1.0f + pulse * 0.2f);
    float alpha = color.w * (1.0f - pulse * 0.5f);

    ImVec4 ring_color = ImVec4(color.x, color.y, color.z, alpha);
    draw_list->AddCircle(center, current_radius, ImGui::GetColorU32(ring_color), 64, 3.0f);
}

void DrawScanLine(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                  const ImVec4& color, float time, float thickness) {
    float height = max.y - min.y;
    float y_pos = min.y + std::fmod(time * height * 0.5f, height);

    ImVec2 line_start = ImVec2(min.x, y_pos);
    ImVec2 line_end = ImVec2(max.x, y_pos);

    for (int i = 0; i < 5; ++i) {
        float offset = i * thickness;
        float alpha = color.w * (1.0f - i / 5.0f);
        ImVec4 trail_color = ImVec4(color.x, color.y, color.z, alpha);

        draw_list->AddLine(
            ImVec2(line_start.x, line_start.y - offset),
            ImVec2(line_end.x, line_end.y - offset),
            ImGui::GetColorU32(trail_color),
            thickness
        );
    }
}

void DrawHeatWave(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                  float intensity, float time) {
    float height = max.y - min.y;
    int line_count = int(height / 4.0f);

    for (int i = 0; i < line_count; ++i) {
        float y = min.y + (i / float(line_count)) * height;
        float wave_offset = std::sin(time * 5.0f + i * 0.5f) * intensity;

        ImVec2 p1 = ImVec2(min.x + wave_offset, y);
        ImVec2 p2 = ImVec2(max.x + wave_offset, y);

        ImVec4 wave_color = ImVec4(1, 1, 1, 0.03f * intensity);
        draw_list->AddLine(p1, p2, ImGui::GetColorU32(wave_color), 1.0f);
    }
}

static std::vector<float> g_elevation_stack;

void BeginElevatedLayer(float elevation) {
    g_elevation_stack.push_back(elevation);
}

void EndElevatedLayer() {
    if (!g_elevation_stack.empty()) {
        g_elevation_stack.pop_back();
    }
}

NeuronBackground::NeuronBackground() {
}

void NeuronBackground::Initialize(const ImVec2& bounds, const NeuronConfig& config) {
    config_ = config;
    nodes_.clear();
    nodes_.reserve(config.node_count);

    for (int i = 0; i < config.node_count; ++i) {
        NeuronNode node;
        node.position.x = (rand() % 10000) / 10000.0f * bounds.x;
        node.position.y = (rand() % 10000) / 10000.0f * bounds.y;

        float angle = (rand() % 10000) / 10000.0f * 6.28318f;
        node.velocity.x = std::cos(angle) * config.speed;
        node.velocity.y = std::sin(angle) * config.speed;

        float radius_range = config.node_radius_max - config.node_radius_min;
        node.radius = config.node_radius_min + (rand() % 100) / 100.0f * radius_range;

        node.color = config.node_color;
        nodes_.push_back(node);
    }

    initialized_ = true;
}

void NeuronBackground::Update(float dt, const ImVec2& bounds) {
    if (!initialized_) return;

    for (auto& node : nodes_) {
        node.position.x += node.velocity.x * dt;
        node.position.y += node.velocity.y * dt;

        if (node.position.x < 0 || node.position.x > bounds.x) {
            node.velocity.x = -node.velocity.x;
            node.position.x = std::clamp(node.position.x, 0.0f, bounds.x);
        }
        if (node.position.y < 0 || node.position.y > bounds.y) {
            node.velocity.y = -node.velocity.y;
            node.position.y = std::clamp(node.position.y, 0.0f, bounds.y);
        }
    }
}

void NeuronBackground::Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset) {
    if (!initialized_ || nodes_.empty()) return;

    const float max_dist = config_.connection_distance;
    const float max_dist_sq = max_dist * max_dist;

    ImVec4 node_col = config_.use_accent_color ? accent_color : config_.node_color;
    ImVec4 line_col = config_.use_accent_color ? accent_color : config_.line_color;

    if (config_.use_accent_color) {
        node_col.w = 0.5f;
        line_col.w = 0.25f;
    }

    for (size_t i = 0; i < nodes_.size(); ++i) {
        for (size_t j = i + 1; j < nodes_.size(); ++j) {
            const NeuronNode& a = nodes_[i];
            const NeuronNode& b = nodes_[j];

            float dx = b.position.x - a.position.x;
            float dy = b.position.y - a.position.y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq < max_dist_sq) {
                float dist = std::sqrt(dist_sq);
                float alpha_mult = 1.0f - (dist / max_dist);
                alpha_mult = alpha_mult * alpha_mult;

                ImVec4 line_color = ImVec4(
                    line_col.x, line_col.y, line_col.z,
                    line_col.w * alpha_mult
                );

                ImVec2 pos_a = ImVec2(a.position.x + offset.x, a.position.y + offset.y);
                ImVec2 pos_b = ImVec2(b.position.x + offset.x, b.position.y + offset.y);

                draw_list->AddLine(pos_a, pos_b,
                                 ImGui::GetColorU32(line_color),
                                 config_.line_thickness);
            }
        }
    }

    for (const auto& node : nodes_) {
        ImVec2 draw_pos = ImVec2(node.position.x + offset.x, node.position.y + offset.y);

        if (config_.glow_intensity > 0.0f) {
            GlowConfig glow;
            glow.color = node_col;
            glow.intensity = config_.glow_intensity;
            glow.radius = node.radius * 2.0f;
            glow.layers = 2;
            DrawGlowCircle(draw_list, draw_pos, node.radius, glow);
        }

        draw_list->AddCircleFilled(draw_pos, node.radius,
                                  ImGui::GetColorU32(node_col), 16);
    }
}

void NeuronBackground::SetConfig(const NeuronConfig& config) {
    config_ = config;
}

static NeuronBackground g_neuron_background;

NeuronBackground& GetNeuronBackground() {
    return g_neuron_background;
}

FloatingParticlesBackground::FloatingParticlesBackground() {
}

void FloatingParticlesBackground::Initialize(const ImVec2& bounds, const ParticleConfig& config) {
    config_ = config;
    particles_.clear();
    particles_.reserve(config.particle_count);

    for (int i = 0; i < config.particle_count; ++i) {
        Particle p;
        p.position.x = (rand() % 10000) / 10000.0f * bounds.x;
        p.position.y = (rand() % 10000) / 10000.0f * bounds.y;

        float angle = -1.57f + ((rand() % 100) / 100.0f - 0.5f) * 0.5f;
        p.velocity.x = std::cos(angle) * config.speed * 0.3f;
        p.velocity.y = std::sin(angle) * config.speed;

        float size_range = config.size_max - config.size_min;
        p.size = config.size_min + (rand() % 100) / 100.0f * size_range;

        p.color = ImVec4(1, 1, 1, 0.3f + (rand() % 100) / 100.0f * 0.4f);
        p.lifetime = 999999.0f;
        p.max_lifetime = 999999.0f;

        particles_.push_back(p);
    }

    initialized_ = true;
}

void FloatingParticlesBackground::Update(float dt, const ImVec2& bounds) {
    if (!initialized_) return;

    for (auto& p : particles_) {
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;

        if (p.position.x < 0) p.position.x = bounds.x;
        if (p.position.x > bounds.x) p.position.x = 0;
        if (p.position.y < 0) p.position.y = bounds.y;
        if (p.position.y > bounds.y) p.position.y = 0;
    }
}

void FloatingParticlesBackground::Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset) {
    if (!initialized_) return;

    ImVec4 color = config_.use_accent_color ? accent_color : ImVec4(1, 1, 1, 1);

    for (const auto& p : particles_) {
        ImVec2 draw_pos = ImVec2(p.position.x + offset.x, p.position.y + offset.y);
        ImVec4 particle_color = ImVec4(color.x, color.y, color.z, p.color.w * 0.4f);

        draw_list->AddCircleFilled(draw_pos, p.size, ImGui::GetColorU32(particle_color), 12);
    }
}

static FloatingParticlesBackground g_floating_particles;

FloatingParticlesBackground& GetFloatingParticlesBackground() {
    return g_floating_particles;
}

AnimatedWavesBackground::AnimatedWavesBackground() {
}

void AnimatedWavesBackground::Initialize(const ImVec2& bounds, const WaveConfig& config) {
    config_ = config;
    bounds_ = bounds;
    time_offset_ = 0.0f;
    initialized_ = true;
}

void AnimatedWavesBackground::Update(float dt, const ImVec2& bounds) {
    if (!initialized_) return;
    bounds_ = bounds;
    time_offset_ += dt * config_.speed;
}

void AnimatedWavesBackground::Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset) {
    if (!initialized_) return;

    ImVec4 color = config_.use_accent_color ? accent_color : ImVec4(1, 1, 1, 1);

    for (int wave_idx = 0; wave_idx < config_.wave_count; ++wave_idx) {
        float t = config_.wave_count > 1 ? wave_idx / float(config_.wave_count - 1) : 0.5f;
        float y_base = config_.amplitude + t * (bounds_.y - 2.0f * config_.amplitude);

        float phase = time_offset_ + wave_idx * 1.5f;
        float alpha = 0.15f - (wave_idx * 0.02f);

        ImVec4 wave_color = ImVec4(color.x, color.y, color.z, alpha);
        ImU32 col = ImGui::GetColorU32(wave_color);

        const int segments = 100;
        for (int i = 0; i < segments; ++i) {
            float x1 = (i / float(segments)) * bounds_.x;
            float x2 = ((i + 1) / float(segments)) * bounds_.x;

            float y1 = y_base + std::sin((x1 * config_.frequency + phase) * 0.01f) * config_.amplitude;
            float y2 = y_base + std::sin((x2 * config_.frequency + phase) * 0.01f) * config_.amplitude;

            ImVec2 p1 = ImVec2(x1 + offset.x, y1 + offset.y);
            ImVec2 p2 = ImVec2(x2 + offset.x, y2 + offset.y);

            draw_list->AddLine(p1, p2, col, config_.thickness);
        }
    }
}

static AnimatedWavesBackground g_animated_waves;

AnimatedWavesBackground& GetAnimatedWavesBackground() {
    return g_animated_waves;
}

StarfieldBackground::StarfieldBackground() {
}

void StarfieldBackground::Initialize(const ImVec2& bounds, const StarfieldConfig& config) {
    config_ = config;
    stars_.clear();
    stars_.reserve(config.star_count);

    for (int i = 0; i < config.star_count; ++i) {
        Star star;
        star.position.x = (rand() % 10000) / 10000.0f * bounds.x;
        star.position.y = (rand() % 10000) / 10000.0f * bounds.y;
        star.size = 0.5f + (rand() % 100) / 100.0f * config.size_max;
        star.twinkle_phase = (rand() % 100) / 100.0f * 6.28f;
        star.twinkle_speed = config.twinkle_speed * (0.5f + (rand() % 100) / 100.0f);
        stars_.push_back(star);
    }

    initialized_ = true;
}

void StarfieldBackground::Update(float dt, const ImVec2& bounds) {
    if (!initialized_) return;

    for (auto& star : stars_) {
        star.twinkle_phase += dt * star.twinkle_speed;
        if (star.twinkle_phase > 6.28f) star.twinkle_phase -= 6.28f;
    }
}

void StarfieldBackground::Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset) {
    if (!initialized_) return;

    ImVec4 color = config_.use_accent_color ? accent_color : ImVec4(1, 1, 1, 1);

    for (const auto& star : stars_) {
        float twinkle = (std::sin(star.twinkle_phase) + 1.0f) * 0.5f;
        float alpha = 0.3f + twinkle * 0.7f;

        ImVec4 star_color = ImVec4(color.x, color.y, color.z, alpha);
        ImVec2 pos = ImVec2(star.position.x + offset.x, star.position.y + offset.y);

        draw_list->AddCircleFilled(pos, star.size, ImGui::GetColorU32(star_color), 8);
    }
}

static StarfieldBackground g_starfield;

StarfieldBackground& GetStarfieldBackground() {
    return g_starfield;
}

FirefliesBackground::FirefliesBackground() {
}

void FirefliesBackground::Initialize(const ImVec2& bounds, const FirefliesConfig& config) {
    config_ = config;
    fireflies_.clear();
    fireflies_.reserve(config.firefly_count);

    for (int i = 0; i < config.firefly_count; ++i) {
        Firefly fly;
        fly.position.x = (rand() % 10000) / 10000.0f * bounds.x;
        fly.position.y = (rand() % 10000) / 10000.0f * bounds.y;

        float angle = (rand() % 10000) / 10000.0f * 6.28f;
        fly.velocity.x = std::cos(angle) * config.speed;
        fly.velocity.y = std::sin(angle) * config.speed;

        fly.glow_phase = (rand() % 100) / 100.0f * 6.28f;
        fly.glow_speed = 2.0f + (rand() % 100) / 100.0f * 2.0f;
        fly.size = 2.0f + (rand() % 100) / 100.0f * 2.0f;

        fireflies_.push_back(fly);
    }

    initialized_ = true;
}

void FirefliesBackground::Update(float dt, const ImVec2& bounds) {
    if (!initialized_) return;

    for (auto& fly : fireflies_) {
        fly.position.x += fly.velocity.x * dt;
        fly.position.y += fly.velocity.y * dt;

        if (fly.position.x < 0) fly.position.x = bounds.x;
        if (fly.position.x > bounds.x) fly.position.x = 0;
        if (fly.position.y < 0) fly.position.y = bounds.y;
        if (fly.position.y > bounds.y) fly.position.y = 0;

        fly.glow_phase += dt * fly.glow_speed;
        if (fly.glow_phase > 6.28f) fly.glow_phase -= 6.28f;

        if ((rand() % 100) < 2) {
            float angle = (rand() % 10000) / 10000.0f * 6.28f;
            fly.velocity.x = std::cos(angle) * config_.speed;
            fly.velocity.y = std::sin(angle) * config_.speed;
        }
    }
}

void FirefliesBackground::Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset) {
    if (!initialized_) return;

    ImVec4 color = config_.use_accent_color ? accent_color : ImVec4(1, 1, 0.5f, 1);

    for (const auto& fly : fireflies_) {
        float glow = (std::sin(fly.glow_phase) + 1.0f) * 0.5f;
        float alpha = 0.3f + glow * 0.7f;

        ImVec2 pos = ImVec2(fly.position.x + offset.x, fly.position.y + offset.y);

        if (config_.glow_intensity > 0) {
            GlowConfig glow_cfg;
            glow_cfg.color = ImVec4(color.x, color.y, color.z, alpha * config_.glow_intensity);
            glow_cfg.intensity = config_.glow_intensity;
            glow_cfg.radius = fly.size * 3.0f;
            glow_cfg.layers = 2;
            DrawGlowCircle(draw_list, pos, fly.size, glow_cfg);
        }

        ImVec4 fly_color = ImVec4(color.x, color.y, color.z, alpha);
        draw_list->AddCircleFilled(pos, fly.size, ImGui::GetColorU32(fly_color), 12);
    }
}

static FirefliesBackground g_fireflies;

FirefliesBackground& GetFirefliesBackground() {
    return g_fireflies;
}


SnowEffect::SnowEffect() : time_(0.0f), initialized_(false) {
}

float SnowEffect::RandomFloat(float min, float max) {
    float range = max - min;
    return min + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * range;
}

void SnowEffect::RespawnSnowflake(Snowflake& flake, bool from_top) {
    if (from_top) {
        flake.position.x = RandomFloat(0, window_size_.x);
        flake.position.y = RandomFloat(-50.0f, 0.0f);
    }
    else {
        flake.position.x = RandomFloat(0, window_size_.x);
        flake.position.y = RandomFloat(0, window_size_.y);
    }

    flake.size = RandomFloat(config_.size_min, config_.size_max);
    float size_ratio = (flake.size - config_.size_min) / (config_.size_max - config_.size_min);

    flake.velocity.y = config_.fall_speed * (0.5f + size_ratio * 0.5f);
    flake.velocity.x = 0.0f;

    flake.opacity = RandomFloat(config_.opacity_min, config_.opacity_max);
    flake.drift_offset = RandomFloat(0.0f, 6.28f);
    flake.drift_speed = RandomFloat(0.8f, 1.5f);
}

void SnowEffect::Initialize(const ImVec2& bounds, const SnowConfig& config) {
    config_ = config;
    window_size_ = bounds;
    time_ = 0.0f;

    snowflakes_.clear();
    snowflakes_.reserve(config_.snowflake_count);

    for (int i = 0; i < config_.snowflake_count; i++) {
        Snowflake flake;
        RespawnSnowflake(flake, false);
        snowflakes_.push_back(flake);
    }

    initialized_ = true;
}

void SnowEffect::Update(float dt, const ImVec2& bounds) {
    if (!initialized_) return;

    window_size_ = bounds;
    time_ += dt;

    for (auto& flake : snowflakes_) {
        flake.position.y += flake.velocity.y * dt;

        float drift = std::sin(time_ * flake.drift_speed + flake.drift_offset) * config_.drift_amount;
        flake.position.x += drift * dt;

        if (flake.position.y > window_size_.y + 10.0f) {
            RespawnSnowflake(flake, true);
        }

        if (flake.position.x < -10.0f) {
            flake.position.x = window_size_.x + 10.0f;
        }
        else if (flake.position.x > window_size_.x + 10.0f) {
            flake.position.x = -10.0f;
        }
    }
}

void SnowEffect::Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset) {
    if (!initialized_) return;

    for (const auto& flake : snowflakes_) {
        ImVec2 pos = ImVec2(offset.x + flake.position.x, offset.y + flake.position.y);

        ImVec4 color = config_.use_accent_color ? accent_color : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        color.w = flake.opacity;

        ImU32 color_u32 = ImGui::ColorConvertFloat4ToU32(color);

        draw_list->AddCircleFilled(pos, flake.size, color_u32, 8);

        if (flake.size > config_.size_min + 1.0f) {
            ImVec4 glow_color = color;
            glow_color.w *= 0.3f;
            draw_list->AddCircleFilled(pos, flake.size * 1.5f,
                ImGui::ColorConvertFloat4ToU32(glow_color), 12);
        }
    }
}

static SnowEffect g_snow_effect;

SnowEffect& GetSnowEffect() {
    return g_snow_effect;
}

}}}
