#pragma once

#include <imgui/imgui.h>
#include <vector>

namespace tsm {
    namespace ui {
        namespace helpers {



            enum class ShadowStyle {
                Soft,
                Hard,
                Inner,
                Glow
            };

            struct ShadowConfig {
                ImVec2 offset = ImVec2(0, 2);
                float blur = 8.0f;
                ImVec4 color = ImVec4(0, 0, 0, 0.3f);
                ShadowStyle style = ShadowStyle::Soft;

                static ShadowConfig Elevated() {
                    return { ImVec2(0, 4), 12.0f, ImVec4(0, 0, 0, 0.4f), ShadowStyle::Soft };
                }
                static ShadowConfig Subtle() {
                    return { ImVec2(0, 1), 4.0f, ImVec4(0, 0, 0, 0.2f), ShadowStyle::Soft };
                }
                static ShadowConfig Glow(const ImVec4& accent_color) {
                    return { ImVec2(0, 0), 16.0f, ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.5f), ShadowStyle::Glow };
                }
            };

            void DrawShadow(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const ShadowConfig& config, float corner_radius = 0.0f);

            void DrawCircleShadow(ImDrawList* draw_list, const ImVec2& center, float radius,
                const ShadowConfig& config);


            struct GlassConfig {
                float blur_strength = 0.8f;
                float tint_alpha = 0.3f;
                ImVec4 tint_color = ImVec4(0.2f, 0.22f, 0.25f, 1.0f);
                float border_alpha = 0.2f;
                bool use_noise = true;
                float noise_strength = 0.02f;
            };

            void DrawGlassRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const GlassConfig& config, float corner_radius = 0.0f);


            enum class GradientDirection {
                TopToBottom,
                BottomToTop,
                LeftToRight,
                RightToLeft,
                Diagonal,
                Radial
            };

            void DrawGradientRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const ImVec4& color1, const ImVec4& color2,
                GradientDirection direction, float corner_radius = 0.0f);

            void DrawGradientRectMulti(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const ImVec4* colors, int color_count,
                GradientDirection direction, float corner_radius = 0.0f);


            struct GlowConfig {
                float intensity = 0.6f;
                float radius = 12.0f;
                ImVec4 color;
                int layers = 3;

                static GlowConfig Accent(const ImVec4& accent_color) {
                    return { 0.6f, 12.0f, accent_color, 3 };
                }
                static GlowConfig Subtle(const ImVec4& accent_color) {
                    return { 0.3f, 8.0f, accent_color, 2 };
                }
                static GlowConfig Intense(const ImVec4& accent_color) {
                    return { 0.9f, 20.0f, accent_color, 4 };
                }
            };

            void DrawGlowRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const GlowConfig& config, float corner_radius = 0.0f);

            void DrawGlowCircle(ImDrawList* draw_list, const ImVec2& center, float radius,
                const GlowConfig& config);


            struct Particle {
                ImVec2 position;
                ImVec2 velocity;
                ImVec4 color;
                float lifetime;
                float max_lifetime;
                float size;
                float rotation;
                float rotation_speed;
            };

            class ParticleSystem {
            public:
                ParticleSystem(int max_particles = 100);

                void Emit(const ImVec2& position, const ImVec2& velocity,
                    const ImVec4& color, float lifetime, float size);
                void EmitBurst(const ImVec2& position, int count, float speed,
                    const ImVec4& color, float lifetime, float size);

                void Update(float dt);
                void Draw(ImDrawList* draw_list);
                void Clear();

            private:
                std::vector<Particle> particles_;
                int max_particles_;
            };

            ParticleSystem& GetParticleSystem();


            void DrawSparkle(ImDrawList* draw_list, const ImVec2& position, float size,
                const ImVec4& color, float time);

            void DrawPulsingRing(ImDrawList* draw_list, const ImVec2& center, float base_radius,
                const ImVec4& color, float time);

            void DrawScanLine(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                const ImVec4& color, float time, float thickness = 2.0f);

            void DrawHeatWave(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max,
                float intensity, float time);


            void BeginElevatedLayer(float elevation = 1.0f);
            void EndElevatedLayer();


            struct NeuronNode {
                ImVec2 position;
                ImVec2 velocity;
                float radius;
                ImVec4 color;
            };

            struct NeuronConfig {
                int node_count = 25;
                float node_radius_min = 3.0f;
                float node_radius_max = 6.0f;
                float speed = 30.0f;
                float connection_distance = 180.0f;
                float line_thickness = 1.5f;
                ImVec4 node_color = ImVec4(1, 1, 1, 0.4f);
                ImVec4 line_color = ImVec4(1, 1, 1, 0.2f);
                bool use_accent_color = true;
                float glow_intensity = 0.4f;
            };

            class NeuronBackground {
            public:
                NeuronBackground();

                void Initialize(const ImVec2& bounds, const NeuronConfig& config);
                void Update(float dt, const ImVec2& bounds);
                void Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset = ImVec2(0, 0));
                void SetConfig(const NeuronConfig& config);

            private:
                std::vector<NeuronNode> nodes_;
                NeuronConfig config_;
                bool initialized_ = false;
            };

            NeuronBackground& GetNeuronBackground();


            struct ParticleConfig {
                int particle_count = 100;
                float speed = 20.0f;
                float size_min = 2.0f;
                float size_max = 4.0f;
                bool use_accent_color = true;
            };

            class FloatingParticlesBackground {
            public:
                FloatingParticlesBackground();

                void Initialize(const ImVec2& bounds, const ParticleConfig& config);
                void Update(float dt, const ImVec2& bounds);
                void Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset = ImVec2(0, 0));

            private:
                std::vector<Particle> particles_;
                ParticleConfig config_;
                bool initialized_ = false;
            };

            FloatingParticlesBackground& GetFloatingParticlesBackground();


            struct WaveConfig {
                int wave_count = 5;
                float speed = 15.0f;
                float amplitude = 30.0f;
                float frequency = 0.5f;
                float thickness = 2.0f;
                bool use_accent_color = true;
            };

            class AnimatedWavesBackground {
            public:
                AnimatedWavesBackground();

                void Initialize(const ImVec2& bounds, const WaveConfig& config);
                void Update(float dt, const ImVec2& bounds);
                void Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset = ImVec2(0, 0));

            private:
                WaveConfig config_;
                ImVec2 bounds_;
                float time_offset_ = 0.0f;
                bool initialized_ = false;
            };

            AnimatedWavesBackground& GetAnimatedWavesBackground();


            struct Star {
                ImVec2 position;
                float size;
                float twinkle_phase;
                float twinkle_speed;
            };

            struct StarfieldConfig {
                int star_count = 150;
                float twinkle_speed = 1.0f;
                float size_max = 3.0f;
                bool use_accent_color = true;
            };

            class StarfieldBackground {
            public:
                StarfieldBackground();

                void Initialize(const ImVec2& bounds, const StarfieldConfig& config);
                void Update(float dt, const ImVec2& bounds);
                void Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset = ImVec2(0, 0));

            private:
                std::vector<Star> stars_;
                StarfieldConfig config_;
                bool initialized_ = false;
            };

            StarfieldBackground& GetStarfieldBackground();


            struct Firefly {
                ImVec2 position;
                ImVec2 velocity;
                float glow_phase;
                float glow_speed;
                float size;
            };

            struct FirefliesConfig {
                int firefly_count = 50;
                float speed = 15.0f;
                float glow_intensity = 0.6f;
                bool use_accent_color = true;
            };

            class FirefliesBackground {
            public:
                FirefliesBackground();

                void Initialize(const ImVec2& bounds, const FirefliesConfig& config);
                void Update(float dt, const ImVec2& bounds);
                void Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset = ImVec2(0, 0));

            private:
                std::vector<Firefly> fireflies_;
                FirefliesConfig config_;
                bool initialized_ = false;
            };

            FirefliesBackground& GetFirefliesBackground();


            struct Snowflake {
                ImVec2 position;
                ImVec2 velocity;
                float size;
                float opacity;
                float drift_offset;
                float drift_speed;
            };

            struct SnowConfig {
                int snowflake_count = 150;
                float fall_speed = 30.0f;
                float drift_amount = 20.0f;
                float size_min = 2.0f;
                float size_max = 6.0f;
                float opacity_min = 0.3f;
                float opacity_max = 0.9f;
                bool use_accent_color = false;
            };

            class SnowEffect {
            public:
                SnowEffect();

                void Initialize(const ImVec2& bounds, const SnowConfig& config);
                void Update(float dt, const ImVec2& bounds);
                void Draw(ImDrawList* draw_list, const ImVec4& accent_color, const ImVec2& offset = ImVec2(0, 0));

            private:
                std::vector<Snowflake> snowflakes_;
                SnowConfig config_;
                ImVec2 window_size_;
                float time_;
                bool initialized_ = false;

                float RandomFloat(float min, float max);
                void RespawnSnowflake(Snowflake& flake, bool from_top = true);
            };

            SnowEffect& GetSnowEffect();

        }
    }
}
