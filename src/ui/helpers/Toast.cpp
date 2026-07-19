#include <ui/helpers/Toast.h>
#include <ui/helpers/Animations.h>
#include <ui/helpers/VisualEffects.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <ui/widgets/Icons.h>
#include <imgui/imgui.h>
#include <algorithm>
#include <cstring>
#include <cfloat>
#include <cmath>

namespace tsm {
    namespace ui {
        namespace helpers {

            namespace {
                float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

                ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
                    t = Clamp01(t);
                    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
                }

                const char* GetToastIconName(ToastType type) {
                    switch (type) {
                    case ToastType::Success: return "UiMiscCheck";
                    case ToastType::Info:    return "UiLightBulb";
                    case ToastType::Warning: return "UiMenuCaution";
                    case ToastType::Error:   return "UiMenuXTreasure";
                    case ToastType::Loading: return "UiMenuLoadingSpinner";
                    default:                 return "UiMiscQuestion";
                    }
                }
            }

            ToastManager& ToastManager::Get() {
                static ToastManager instance;
                return instance;
            }

            ToastManager::ToastManager() {}

            void ToastManager::Show(const std::string& message, ToastType type, float duration) {
                ToastConfig config;
                config.type = type;
                config.duration = duration;
                ShowCustom(message, config);
            }

            void ToastManager::ShowWithTitle(const std::string& title, const std::string& message,
                ToastType type, float duration) {
                ToastConfig config;
                config.type = type;
                config.duration = duration;
                ShowCustomWithTitle(title, message, config);
            }

            void ToastManager::ShowCustom(const std::string& message, const ToastConfig& config) {
                Toast toast;
                toast.message = message;
                toast.config = config;
                toast.lifetime = config.duration;
                toast.max_lifetime = config.duration;
                toast.active = true;
                toast.slide_progress = 0.0f;
                AddToast(toast);
            }

            void ToastManager::ShowCustomWithTitle(const std::string& title, const std::string& message,
                const ToastConfig& config) {
                Toast toast;
                toast.title = title;
                toast.message = message;
                toast.config = config;
                toast.lifetime = config.duration;
                toast.max_lifetime = config.duration;
                toast.active = true;
                toast.slide_progress = 0.0f;
                AddToast(toast);
            }

            void ToastManager::Success(const std::string& message, float duration) { Show(message, ToastType::Success, duration); }
            void ToastManager::Info(const std::string& message, float duration) { Show(message, ToastType::Info, duration); }
            void ToastManager::Warning(const std::string& message, float duration) { Show(message, ToastType::Warning, duration); }
            void ToastManager::Error(const std::string& message, float duration) { Show(message, ToastType::Error, duration); }

            void ToastManager::Update(float dt) {
                for (auto& toast : toasts_) {
                    if (!toast.active) continue;

                    if (toast.slide_progress < 1.0f) {
                        toast.slide_progress += dt * 4.0f;
                        if (toast.slide_progress > 1.0f) toast.slide_progress = 1.0f;
                    }

                    if (toast.config.auto_dismiss) {
                        toast.lifetime -= dt;
                        if (toast.lifetime <= 0.0f) toast.active = false;
                    }
                }
                toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [](const Toast& t) { return !t.active; }), toasts_.end());
            }

            void ToastManager::Render() {
                if (toasts_.empty()) return;

                ImGuiIO& io = ImGui::GetIO();
                ImVec2 viewport_size = io.DisplaySize;
                ImVec2 start_pos = position_;
                if (start_pos.x < 0.0f || start_pos.y < 0.0f) {
                    start_pos = ImVec2(viewport_size.x * 0.5f, viewport_size.y - DP(40.0f));
                }

                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(viewport_size);
                ImGui::Begin("##ToastOverlay", nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

                ImDrawList* dl = ImGui::GetWindowDrawList();

                struct ToastDimensions {
                    float width, height;
                    ImVec2 title_size, message_size;
                    float text_height, content_height;
                    float wrap_w; // FLT_MAX when text fits naturally; capped value when clamped
                };

                std::vector<ToastDimensions> toast_dims;
                float total_height = 0.0f;

                ImFont* font = ImGui::GetFont();
                const float title_scale = 1.18f;
                const float message_scale = 1.06f;

                for (auto& toast : toasts_) {
                    if (!toast.active) continue;

                    ImVec2 padding = ImVec2(DP(16.0f), DP(11.0f));
                    float icon_size = toast.config.show_icon ? DP(28.0f) : 0.0f;
                    float icon_gap = toast.config.show_icon ? DP(12.0f) : 0.0f;

                    const char* title_localized = toast.title.empty() ? nullptr : tsm::ui::i18n::Tr(toast.title.c_str());
                    const char* message_localized = tsm::ui::i18n::Tr(toast.message.c_str());

                    ImVec2 title_size(0, 0), message_size(0, 0);
                    if (title_localized && *title_localized)
                        title_size = font->CalcTextSizeA(font->FontSize * title_scale, FLT_MAX, 0.0f, title_localized);

                    message_size = font->CalcTextSizeA(font->FontSize * message_scale, FLT_MAX, 0.0f, message_localized);

                    float max_toast_w = std::min(DP(420.0f), viewport_size.x * 0.90f);
                    float ideal_w = std::max(title_size.x, message_size.x) + padding.x * 2.0f + icon_size + icon_gap;
                    float toast_w = std::max(DP(220.0f), std::min(ideal_w, max_toast_w));

                    // Only re-measure with a hard wrap limit when the toast was actually clamped.
                    // When text fits naturally, leaving wrap_w as FLT_MAX prevents ImGui from
                    // wrapping content that would comfortably sit on a single line.
                    float wrap_w = FLT_MAX;
                    if (ideal_w > toast_w) {
                        wrap_w = toast_w - padding.x * 2.0f - icon_size - icon_gap;
                        if (title_localized && *title_localized)
                            title_size = font->CalcTextSizeA(font->FontSize * title_scale, FLT_MAX, wrap_w, title_localized);
                        message_size = font->CalcTextSizeA(font->FontSize * message_scale, FLT_MAX, wrap_w, message_localized);
                    }

                    float text_h = title_size.y + (title_size.y > 0 ? DP(4.0f) : 0.0f) + message_size.y;
                    float content_h = std::max(text_h, icon_size);
                    float toast_h = std::max(DP(40.0f), content_h + padding.y * 2.0f + DP(4.0f));

                    toast_dims.push_back({ toast_w, toast_h, title_size, message_size, text_h, content_h, wrap_w });
                    total_height += toast_h + spacing_;
                }

                if (!toast_dims.empty()) total_height -= spacing_;
                float current_y = start_pos.y - total_height;
                size_t dim_idx = 0;

                float max_all_w = 0.0f;
                for (const auto& d : toast_dims) max_all_w = std::max(max_all_w, d.width);

                for (auto& toast : toasts_) {
                    if (!toast.active) continue;
                    const auto& dims = toast_dims[dim_idx++];

                    float slide_in = Ease(toast.slide_progress, EaseType::OutCubic);
                    float slide_back = Ease(toast.slide_progress, EaseType::OutBack);

                    float slide_offset = (1.0f - slide_in) * toast.config.slide_distance;
                    float appear_scale = 0.96f + 0.04f * slide_back;
                    float lift = (1.0f - slide_in) * DP(6.0f);

                    float fade_alpha = 1.0f;
                    if (toast.lifetime < 0.4f) fade_alpha = toast.lifetime / 0.4f;
                    fade_alpha = std::min(fade_alpha, Clamp01(toast.slide_progress / 0.25f));

                    ImVec2 toast_center = ImVec2(start_pos.x, current_y + slide_offset - lift + dims.height * 0.5f);
                    ImVec2 half = ImVec2(dims.width * 0.5f * appear_scale, dims.height * 0.5f * appear_scale);
                    ImVec2 t_min = ImVec2(toast_center.x - half.x, toast_center.y - half.y);
                    ImVec2 t_max = ImVec2(toast_center.x + half.x, toast_center.y + half.y);

                    DrawShadow(dl, t_min, t_max, ShadowConfig::Elevated(), DP(12.0f));

                    ImVec4 type_col = GetTypeColor(toast.config.type);
                    ImVec4 bg_top = ImVec4(0.16f, 0.18f, 0.20f, 0.96f * fade_alpha);
                    ImVec4 bg_bot = ImVec4(0.08f, 0.09f, 0.10f, 0.94f * fade_alpha);
                    ImVec4 bg_base = ImVec4(0.10f, 0.11f, 0.12f, 0.96f * fade_alpha);

                    dl->AddRectFilled(t_min, t_max, ImGui::GetColorU32(bg_base), DP(12.0f));
                    dl->AddRectFilledMultiColor(ImVec2(t_min.x + 1, t_min.y + 1), ImVec2(t_max.x - 1, t_max.y - 1),
                        ImGui::GetColorU32(bg_top), ImGui::GetColorU32(bg_top),
                        ImGui::GetColorU32(bg_bot), ImGui::GetColorU32(bg_bot));

                    type_col.w *= fade_alpha;
                    dl->AddRectFilled(t_min, ImVec2(t_min.x + DP(5.0f), t_max.y), ImGui::GetColorU32(type_col), DP(12.0f), ImDrawFlags_RoundCornersLeft);
                    dl->AddRect(t_min, t_max, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.06f * fade_alpha)), DP(12.0f), 0, DP(1.0f));

                    float content_x = t_min.x + DP(16.0f);
                    float icon_sz = toast.config.show_icon ? DP(28.0f) : 0.0f;

                    float centerY = (t_min.y + t_max.y) * 0.5f;
                    float textY = centerY - dims.text_height * 0.5f;
                    textY = std::max(textY, t_min.y + DP(11.0f));

                    if (toast.config.show_icon) {
                        float icon_align_y = (dims.text_height > icon_sz)
                            ? textY + icon_sz * 0.5f
                            : centerY;

                        ImVec2 icon_c = ImVec2(content_x + icon_sz * 0.5f, icon_align_y);
                        ImVec4 icon_bg = LerpColor(bg_base, type_col, 0.22f);
                        icon_bg.w = fade_alpha * 0.95f;
                        dl->AddCircleFilled(icon_c, icon_sz * 0.5f, ImGui::GetColorU32(icon_bg), 24);
                        dl->AddCircle(icon_c, icon_sz * 0.5f, ImGui::GetColorU32(type_col), 24, DP(1.2f));

                        const char* iname = GetToastIconName(toast.config.type);
                        ImVec4 ic_col = ImVec4(1, 1, 1, fade_alpha);

                        ImGui::SetCursorScreenPos(ImVec2(icon_c.x - icon_sz * 0.32f, icon_c.y - icon_sz * 0.32f));
                        if (toast.config.type == ToastType::Loading) {
                            float angle = (float)ImGui::GetTime() * 3.5f;
                            tsm::ui::widgets::IconRotated(iname, icon_sz * 0.65f, angle * 57.29f, ic_col);
                        }
                        else {
                            tsm::ui::widgets::Icon(iname, icon_sz * 0.64f, ic_col);
                        }
                        content_x += icon_sz + DP(12.0f);
                    }

                    // Use the wrap width established during layout so rendering stays consistent
                    // with the measured dimensions regardless of the appear_scale animation.
                    float text_wrap_w = dims.wrap_w;
                    float drawY = textY;

                    if (!toast.title.empty()) {
                        dl->AddText(font, font->FontSize * title_scale, ImVec2(content_x, drawY),
                            ImGui::GetColorU32(ImVec4(1, 1, 1, fade_alpha)),
                            tsm::ui::i18n::Tr(toast.title.c_str()), nullptr, text_wrap_w);
                        drawY += dims.title_size.y + DP(4.0f);
                    }
                    dl->AddText(font, font->FontSize * message_scale, ImVec2(content_x, drawY),
                        ImGui::GetColorU32(ImVec4(0.85f, 0.88f, 0.92f, fade_alpha * 0.90f)),
                        tsm::ui::i18n::Tr(toast.message.c_str()), nullptr, text_wrap_w);

                    if (toast.config.auto_dismiss && toast.max_lifetime > 0.0f) {
                        float prg = Clamp01(toast.lifetime / toast.max_lifetime);
                        float bw = (t_max.x - t_min.x) - DP(16.0f);
                        ImVec2 b_min = ImVec2(t_min.x + DP(8.0f), t_max.y - DP(4.0f));
                        dl->AddRectFilled(b_min, ImVec2(b_min.x + bw, b_min.y + DP(2.0f)), ImGui::GetColorU32(ImVec4(1, 1, 1, 0.06f * fade_alpha)), DP(1.0f));
                        dl->AddRectFilled(b_min, ImVec2(b_min.x + bw * prg, b_min.y + DP(2.0f)), ImGui::GetColorU32(type_col), DP(1.0f));
                    }

                    current_y += dims.height + spacing_;
                }

                ImGui::End();
            }

            void ToastManager::Clear() { toasts_.clear(); }

            void ToastManager::AddToast(const Toast& toast) {
                if (toasts_.size() >= (size_t)max_toasts_) toasts_.erase(toasts_.begin());
                toasts_.push_back(toast);
            }

            ImVec4 ToastManager::GetTypeColor(ToastType type) const {
                switch (type) {
                case ToastType::Success: return ImVec4(0.2f, 0.85f, 0.45f, 1.0f);
                case ToastType::Info:    return ImVec4(0.3f, 0.65f, 1.0f, 1.0f);
                case ToastType::Warning: return ImVec4(1.0f, 0.75f, 0.25f, 1.0f);
                case ToastType::Error:   return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                case ToastType::Loading: return ImVec4(0.45f, 0.65f, 1.0f, 1.0f);
                default:                 return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                }
            }

            const char* ToastManager::GetTypeIcon(ToastType type) const { return ""; }

        }
    }
}