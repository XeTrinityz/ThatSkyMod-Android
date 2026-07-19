#include <ui/core/App.h>
#include <ui/core/ThemeManager.h>
#include <ui/core/WindowManager.h>
#include <ui/core/TabBar.h>
#include <ui/core/metrics.h>
#include <ui/panels/PlayerPanel.h>
#include <ui/panels/SocialPanel.h>
#include <ui/panels/TeleportPanel.h>
#include <ui/overlays/QuickTeleportOverlay.h>
#include <ui/overlays/POIOverlay.h>
#include <ui/overlays/FavouritesOverlay.h>
#include <ui/panels/OutfitsPanel.h>
#include <ui/panels/UnlocksPanel.h>
#include <ui/panels/ProgressionPanel.h>
#include <ui/panels/WorldPanel.h>
#include <ui/panels/MusicPanel.h>
#include <ui/panels/SettingsPanel.h>
#include <ui/overlays/UpdateNotificationOverlay.h>
#include <ui/overlays/GameOverlay.h>
#include <ui/overlays/MusicPlaybackOverlay.h>
#include <ui/helpers/Toast.h>
#include <ui/helpers/VisualEffects.h>
#include <state/ModState.h>
#include <features/manager/FeatureManager.h>
#include <core/UpdateChecker.h>

#include <Cipher/CipherUtils.h>
#include <imgui/imgui.h>
#include <cfloat>
#include <string>

namespace tsm {
    namespace ui {

        namespace {
            bool g_autoScale = true;
        }

        void Initialize()
        {
            ThemeManager::Get().Initialize();

            bool ok = tsm::state::ModState::Get().LoadFromFile();
            if (!ok) {
                ImGui::GetStyle().ScrollbarSize = 0.0f;
            }
            auto& ms = tsm::state::ModState::Get();

            tsm::features::FeatureManager::Get().ApplyAllFeatures();

            tsm::ui::overlays::SetfavoritesVisible(ms.ui.showFavoritesRadial);

            DeviceInfo di = CipherUtils::get_DeviceInfo();
            float dens = di.density;
            if (dens <= 0.0f) {
                float x = di.xdpi > 0.0f ? di.xdpi : 0.0f;
                float y = di.ydpi > 0.0f ? di.ydpi : 0.0f;
                float avgdpi = 0.0f;
                if (x > 0.0f && y > 0.0f) avgdpi = (x + y) * 0.5f;
                else if (x > 0.0f)        avgdpi = x;
                else if (y > 0.0f)        avgdpi = y;
                dens = (avgdpi > 0.0f) ? (avgdpi / 160.0f) : 1.0f;
            }
            modui::set_screen_density(dens);

            modui::set_ui_scale(1.0f);
        }

        void Draw(bool* menu_open)
        {
            ImGuiIO& io = ImGui::GetIO();
            float dt = io.DeltaTime;

            if (tsm::core::UpdateChecker::Get().ConsumeJustInstalledFlag()) {
                std::string msg = std::string("Updated That Sky Mod to ") + tsm::core::UpdateChecker::Get().GetLatestVersion();
                tsm::ui::helpers::ShowToastSuccess(msg);
            }

            tsm::ui::helpers::ToastManager::Get().Update(dt);

            tsm::ui::overlays::RenderQuickTeleport();
            tsm::ui::overlays::RenderPOI();
            tsm::ui::overlays::RenderFavourites();
            tsm::ui::overlays::RenderGameOverlay();
            tsm::ui::overlays::RenderMusicPlaybackOverlay();
            tsm::ui::helpers::ToastManager::Get().Render();

            if (!menu_open || !*menu_open) return;

            auto& ms = tsm::state::ModState::Get();

            ThemeManager::Get().UpdateRainbow(io.DeltaTime, ms.ui.accentRainbow);

            ImVec4 accent = ThemeManager::Get().GetAccentColor();
            auto& winMgr = WindowManager::Get();

            if (!winMgr.BeginFrame(menu_open, g_autoScale, accent)) {
                return;
            }

            ImVec2 win_size = ImGui::GetWindowSize();

            switch (ms.ui.backgroundEffectType) {
                case 1: {
                    auto& neuron_bg = tsm::ui::helpers::GetNeuronBackground();
                    static bool neuron_initialized = false;
                    static int last_node_count = 0;
                    static float last_speed = 0.0f;
                    static float last_conn_dist = 0.0f;
                    static float last_glow = 0.0f;

                    bool config_changed = (last_node_count != ms.ui.neuronNodeCount ||
                                          last_speed != ms.ui.neuronSpeed ||
                                          last_conn_dist != ms.ui.neuronConnectionDistance ||
                                          last_glow != ms.ui.neuronGlowIntensity);

                    if (!neuron_initialized || config_changed) {
                        tsm::ui::helpers::NeuronConfig config;
                        config.node_count = ms.ui.neuronNodeCount;
                        config.speed = ms.ui.neuronSpeed;
                        config.connection_distance = ms.ui.neuronConnectionDistance;
                        config.use_accent_color = true;
                        config.glow_intensity = ms.ui.neuronGlowIntensity;
                        config.node_radius_min = 2.5f;
                        config.node_radius_max = 5.0f;
                        config.line_thickness = 1.2f;
                        neuron_bg.Initialize(win_size, config);

                        last_node_count = ms.ui.neuronNodeCount;
                        last_speed = ms.ui.neuronSpeed;
                        last_conn_dist = ms.ui.neuronConnectionDistance;
                        last_glow = ms.ui.neuronGlowIntensity;
                        neuron_initialized = true;
                    }
                    neuron_bg.Update(dt, win_size);
                    break;
                }

                case 2: {
                    auto& particles_bg = tsm::ui::helpers::GetFloatingParticlesBackground();
                    static bool particles_initialized = false;
                    static int last_particle_count = 0;
                    static float last_particle_speed = 0.0f;
                    static float last_particle_size = 0.0f;

                    bool config_changed = (last_particle_count != ms.ui.particleCount ||
                                          last_particle_speed != ms.ui.particleSpeed ||
                                          last_particle_size != ms.ui.particleSize);

                    if (!particles_initialized || config_changed) {
                        tsm::ui::helpers::ParticleConfig config;
                        config.particle_count = ms.ui.particleCount;
                        config.speed = ms.ui.particleSpeed;
                        config.size_min = ms.ui.particleSize * 0.7f;
                        config.size_max = ms.ui.particleSize;
                        config.use_accent_color = true;
                        particles_bg.Initialize(win_size, config);

                        last_particle_count = ms.ui.particleCount;
                        last_particle_speed = ms.ui.particleSpeed;
                        last_particle_size = ms.ui.particleSize;
                        particles_initialized = true;
                    }
                    particles_bg.Update(dt, win_size);
                    break;
                }

                case 3: {
                    auto& waves_bg = tsm::ui::helpers::GetAnimatedWavesBackground();
                    static bool waves_initialized = false;
                    static int last_wave_count = 0;
                    static float last_wave_speed = 0.0f;
                    static float last_wave_amplitude = 0.0f;
                    static float last_wave_thickness = 0.0f;

                    bool config_changed = (last_wave_count != ms.ui.waveCount ||
                                          last_wave_speed != ms.ui.waveSpeed ||
                                          last_wave_amplitude != ms.ui.waveAmplitude ||
                                          last_wave_thickness != ms.ui.waveThickness);

                    if (!waves_initialized || config_changed) {
                        tsm::ui::helpers::WaveConfig config;
                        config.wave_count = ms.ui.waveCount;
                        config.speed = ms.ui.waveSpeed;
                        config.amplitude = ms.ui.waveAmplitude;
                        config.thickness = ms.ui.waveThickness;
                        config.use_accent_color = true;
                        waves_bg.Initialize(win_size, config);

                        last_wave_count = ms.ui.waveCount;
                        last_wave_speed = ms.ui.waveSpeed;
                        last_wave_amplitude = ms.ui.waveAmplitude;
                        last_wave_thickness = ms.ui.waveThickness;
                        waves_initialized = true;
                    }
                    waves_bg.Update(dt, win_size);
                    break;
                }

                case 4: {
                    auto& starfield_bg = tsm::ui::helpers::GetStarfieldBackground();
                    static bool starfield_initialized = false;
                    static int last_star_count = 0;
                    static float last_twinkle_speed = 0.0f;
                    static float last_star_size = 0.0f;

                    bool config_changed = (last_star_count != ms.ui.starCount ||
                                          last_twinkle_speed != ms.ui.starTwinkleSpeed ||
                                          last_star_size != ms.ui.starSizeMax);

                    if (!starfield_initialized || config_changed) {
                        tsm::ui::helpers::StarfieldConfig config;
                        config.star_count = ms.ui.starCount;
                        config.twinkle_speed = ms.ui.starTwinkleSpeed;
                        config.size_max = ms.ui.starSizeMax;
                        config.use_accent_color = true;
                        starfield_bg.Initialize(win_size, config);

                        last_star_count = ms.ui.starCount;
                        last_twinkle_speed = ms.ui.starTwinkleSpeed;
                        last_star_size = ms.ui.starSizeMax;
                        starfield_initialized = true;
                    }
                    starfield_bg.Update(dt, win_size);
                    break;
                }

                case 5: {
                    auto& fireflies_bg = tsm::ui::helpers::GetFirefliesBackground();
                    static bool fireflies_initialized = false;
                    static int last_firefly_count = 0;
                    static float last_firefly_speed = 0.0f;
                    static float last_firefly_glow = 0.0f;

                    bool config_changed = (last_firefly_count != ms.ui.fireflyCount ||
                                          last_firefly_speed != ms.ui.fireflySpeed ||
                                          last_firefly_glow != ms.ui.fireflyGlowIntensity);

                    if (!fireflies_initialized || config_changed) {
                        tsm::ui::helpers::FirefliesConfig config;
                        config.firefly_count = ms.ui.fireflyCount;
                        config.speed = ms.ui.fireflySpeed;
                        config.glow_intensity = ms.ui.fireflyGlowIntensity;
                        config.use_accent_color = true;
                        fireflies_bg.Initialize(win_size, config);

                        last_firefly_count = ms.ui.fireflyCount;
                        last_firefly_speed = ms.ui.fireflySpeed;
                        last_firefly_glow = ms.ui.fireflyGlowIntensity;
                        fireflies_initialized = true;
                    }
                    fireflies_bg.Update(dt, win_size);
                    break;
                }
                case 6: {
                    auto& snow_bg = tsm::ui::helpers::GetSnowEffect();
                    static bool snow_initialized = false;
                    static int last_snowflake_count = 0;
                    static float last_fall_speed = 0.0f;
                    static float last_drift_amount = 0.0f;

                    bool config_changed = (last_snowflake_count != ms.ui.snowflakeCount ||
                        last_fall_speed != ms.ui.snowFallSpeed ||
                        last_drift_amount != ms.ui.snowDriftAmount);

                    if (!snow_initialized || config_changed) {
                        tsm::ui::helpers::SnowConfig config;
                        config.snowflake_count = ms.ui.snowflakeCount;
                        config.fall_speed = ms.ui.snowFallSpeed;
                        config.drift_amount = ms.ui.snowDriftAmount;
                        config.size_min = 2.0f;
                        config.size_max = 6.0f;
                        config.opacity_min = 0.4f;
                        config.opacity_max = 0.9f;
                        config.use_accent_color = false;
                        snow_bg.Initialize(win_size, config);

                        last_snowflake_count = ms.ui.snowflakeCount;
                        last_fall_speed = ms.ui.snowFallSpeed;
                        last_drift_amount = ms.ui.snowDriftAmount;
                        snow_initialized = true;
                    }
                    snow_bg.Update(dt, win_size);
                    break;
                }

                default:
                    break;
            }

            const float header_h = DP(56.0f);

            static ImVec4 cached_child_bg = ImVec4(0, 0, 0, 0);
            static bool bg_initialized = false;
            if (!bg_initialized) {
                cached_child_bg = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];
                cached_child_bg.w = 0.95f;
                bg_initialized = true;
            }
            ImGui::PushStyleColor(ImGuiCol_ChildBg, cached_child_bg);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("##main_container", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
            {
                ImGui::PopStyleColor();

                int new_tab = TabBar::Render(winMgr.GetActiveTab(), accent);
                if (new_tab != winMgr.GetActiveTab()) {
                    winMgr.SetActiveTab(new_tab);
                }

                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::BeginChild("##right_side", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
                {
                    ImGui::PopStyleColor();

                    ImGui::BeginChild("##header", ImVec2(0, header_h), false, ImGuiWindowFlags_NoScrollbar);
                    {
                        ImVec2 avail = ImGui::GetContentRegionAvail();

                        static const char* title = "THAT SKY MOD";
                        static const float title_scale = 1.8f;
                        static ImVec2 title_size = ImVec2(0, 0);
                        static bool title_calculated = false;

                        ImFont* font = ImGui::GetFont();
                        if (!title_calculated) {
                            title_size = font->CalcTextSizeA(font->FontSize * title_scale, FLT_MAX, 0.0f, title);
                            title_calculated = true;
                        }

                        float text_x = (avail.x - title_size.x) * 0.5f;
                        float text_y = (header_h - title_size.y) * 0.5f;

                        ImGui::SetCursorPos(ImVec2(text_x, text_y));
                        ImGui::PushStyleColor(ImGuiCol_Text, accent);

                        ImVec2 pos = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddText(font, font->FontSize * title_scale, pos,
                                                             ImGui::GetColorU32(accent), title);
                        ImGui::PopStyleColor();
                    }
                    ImGui::EndChild();

                    if (ImGui::BeginChild("##content", ImVec2(0, 0), false)) {
                    using namespace tsm::ui::tabs;
                    switch (winMgr.GetActiveTab()) {
                        case 0: DrawPlayerTab(); break;
                        case 1: DrawSocialTab(); break;
                        case 2: DrawTeleportTab(); break;
                        case 3: DrawOutfitsTab(); break;
                        case 4: DrawUnlocksTab(); break;
                        case 5: DrawProgressionTab(); break;
                        case 6: DrawMusicTab(); break;
                        case 7: DrawWorldTab(); break;
                        case 8: DrawSettingsTab(); break;
                        default: break;
                    }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();

            ImDrawList* fg_draw_list = ImGui::GetForegroundDrawList();
            ImVec2 win_pos = ImGui::GetWindowPos();
            fg_draw_list->PushClipRect(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y), true);

            switch (ms.ui.backgroundEffectType) {
                case 1: {
                    auto& neuron_bg = tsm::ui::helpers::GetNeuronBackground();
                    neuron_bg.Draw(fg_draw_list, accent, win_pos);
                    break;
                }
                case 2: {
                    auto& particles_bg = tsm::ui::helpers::GetFloatingParticlesBackground();
                    particles_bg.Draw(fg_draw_list, accent, win_pos);
                    break;
                }
                case 3: {
                    auto& waves_bg = tsm::ui::helpers::GetAnimatedWavesBackground();
                    waves_bg.Draw(fg_draw_list, accent, win_pos);
                    break;
                }
                case 4: {
                    auto& starfield_bg = tsm::ui::helpers::GetStarfieldBackground();
                    starfield_bg.Draw(fg_draw_list, accent, win_pos);
                    break;
                }
                case 5: {
                    auto& fireflies_bg = tsm::ui::helpers::GetFirefliesBackground();
                    fireflies_bg.Draw(fg_draw_list, accent, win_pos);
                    break;
                }
                case 6: {
                    auto& snow_bg = tsm::ui::helpers::GetSnowEffect();
                    snow_bg.Draw(fg_draw_list, accent, win_pos);
                    break;
                }
                default:
                    break;
            }

            fg_draw_list->PopClipRect();

            winMgr.EndFrame();

            ImGui::PopStyleColor();

            winMgr.SyncToModState();
        }

        float GetUiScale() {
            return ::tsm::ui::metrics::GetUiScale();
        }

        void SetAccentColor(const ImVec4& c) {
            ThemeManager::Get().SetAccentColor(c);
        }

        ImVec4 GetAccentColor() {
            return ThemeManager::Get().GetAccentColor();
        }

        void SetAutoScaleEnabled(bool enabled) {
            g_autoScale = enabled;
        }

        bool IsAutoScaleEnabled() {
            return g_autoScale;
        }

        void SetDockedRight(bool right) {
            WindowManager::Get().SetDockedRight(right);
        }

        bool IsDockedRight() {
            return WindowManager::Get().IsDockedRight();
        }

        void ToggleDockedRight() {
            WindowManager::Get().ToggleDockedRight();
        }

    }
}
