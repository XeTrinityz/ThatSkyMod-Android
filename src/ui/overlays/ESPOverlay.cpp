#include <ui/overlays/ESPOverlay.h>
#include <ui/panels/SocialPanel.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <state/ModState.h>
#include <utils/common/esp.h>
#include <game/memory/api.h>
#include <game/hooks/HookManager.h>
#include <network/SocialManager.h>
#include <iconloader/IconLoader.h>
#include <game/data/VoiceData.h>
#include <game/data/StanceData.h>

#include <imgui/imgui.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <algorithm>

namespace tsm { namespace ui { namespace overlays {

namespace {
    constexpr float kIconSize = 64.0f;
    constexpr float kIconLineOffset = 40.0f;
    constexpr float kTextLineOffset = 30.0f;
    constexpr float kTextYOffset = -20.0f;
    constexpr float kLineThickness = 2.0f;
    constexpr float kCloseInfoDistance = 1.0f;
    constexpr float kInfoWindowOffsetX = 16.0f;
    constexpr float kInfoWindowMinWidth = 48.0f;
    constexpr float kDyeClusterRadius = 10.0f;
    constexpr float kCandleClusterRadius = 1.5f;
    constexpr std::uintptr_t kDyeStride = 0x40;
    constexpr std::uintptr_t kCandleStride = 0x70;
    constexpr std::uintptr_t kWingLightStride = 0x140;

    struct LocalPlayerCache {
        vec3 position;
        bool valid = false;

        void Update() {
            valid = false;
            auto localAvatar = tsm::game::api::GetCachedAvatar(0);
            if (localAvatar != 0) {
                auto localInfo = tsm::game::api::GetAvatarInfo(localAvatar);
                if (localInfo.avatarPosition) {
                    position = *localInfo.avatarPosition;
                    valid = true;
                }
            }
        }

        float DistanceTo(const vec3& target) const {
            return valid ? target.distance_to(position) : 0.0f;
        }
    };

    struct Position3D {
        float x, y, z;

        float DistanceTo(const Position3D& other) const {
            float dx = other.x - x;
            float dy = other.y - y;
            float dz = other.z - z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        vec3 ToVec3() const { return vec3{x, y, z}; }
    };

    std::vector<Position3D> ClusterPositions(const std::vector<Position3D>& positions, float clusterRadius) {
        if (positions.empty()) return {};

        std::vector<bool> visited(positions.size(), false);
        std::vector<Position3D> clusterCenters;

        for (size_t i = 0; i < positions.size(); ++i) {
            if (visited[i]) continue;

            std::vector<size_t> clusterIndices;
            clusterIndices.push_back(i);
            visited[i] = true;

            for (size_t j = i + 1; j < positions.size(); ++j) {
                if (visited[j]) continue;

                float dist = positions[i].DistanceTo(positions[j]);
                if (dist <= clusterRadius) {
                    clusterIndices.push_back(j);
                    visited[j] = true;
                }
            }

            float sumX = 0, sumY = 0, sumZ = 0;
            for (size_t idx : clusterIndices) {
                sumX += positions[idx].x;
                sumY += positions[idx].y;
                sumZ += positions[idx].z;
            }

            float cnt = static_cast<float>(clusterIndices.size());
            clusterCenters.push_back(Position3D{sumX / cnt, sumY / cnt, sumZ / cnt});
        }

        return clusterCenters;
    }

    std::string BuildESPText(const std::string& label, float distance, bool showName, bool showDistance) {
        std::string espText;

        if (showName) {
            espText = label;
        }

        if (showDistance) {
            char distBuf[64];
            std::snprintf(distBuf, sizeof(distBuf), "%.1fm", distance);
            if (!espText.empty()) espText += "\n";
            espText += distBuf;
        }

        return espText;
    }

    struct InfoLine {
        const char* iconName = nullptr;
        std::string label;
        std::string value;
    };

    void MeasureTextBlock(const char* text, float fontSize, ImFont* font, float& outWidth, float& outHeight) {
        outWidth = 0.0f;
        outHeight = 0.0f;
        if (!text || !*text) return;

        const char* lineStart = text;
        while (*lineStart) {
            const char* lineEnd = lineStart;
            while (*lineEnd && *lineEnd != '\n') ++lineEnd;

            ImVec2 lineSize = font
                ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, lineStart, lineEnd)
                : ImGui::CalcTextSize(lineStart, lineEnd);
            if (lineSize.x > outWidth) outWidth = lineSize.x;
            outHeight += lineSize.y;

            if (*lineEnd == '\n') {
                lineStart = lineEnd + 1;
            } else {
                break;
            }
        }
    }

    bool BuildEspAnchor(const ImVec2& screenPos,
                        const char* label,
                        float distance,
                        bool showIcons,
                        bool showName,
                        bool showDistance,
                        ImVec2& outMin,
                        ImVec2& outMax) {
        const bool hasText = showName || showDistance;
        float textWidth = 0.0f;
        float textHeight = 0.0f;

        std::string espText;
        if (hasText) {
            espText = BuildESPText(label ? label : "", distance, showName, showDistance);
            ImFont* font = ImGui::GetFont();
            float fontSize = font ? font->FontSize : ImGui::GetFontSize();
            MeasureTextBlock(espText.c_str(), fontSize, font, textWidth, textHeight);
        }

        const float iconSize = showIcons ? kIconSize : 0.0f;
        float width = iconSize;
        if (hasText && textWidth > width) width = textWidth;
        if (width < kInfoWindowMinWidth) width = kInfoWindowMinWidth;

        float top = screenPos.y;
        float bottom = screenPos.y;
        if (showIcons) {
            top = screenPos.y - iconSize * 0.5f;
            bottom = screenPos.y + iconSize * 0.5f;
        }

        if (hasText && textHeight > 0.0f) {
            const float textOffset = showIcons ? (iconSize * 0.6f) : kTextYOffset;
            const float textTop = screenPos.y + textOffset;
            const float textBottom = textTop + textHeight;
            if (textTop < top) top = textTop;
            if (textBottom > bottom) bottom = textBottom;
        }

        if (bottom - top < 1.0f) {
            top -= 0.5f;
            bottom += 0.5f;
        }

        outMin = ImVec2(screenPos.x - width * 0.5f, top);
        outMax = ImVec2(screenPos.x + width * 0.5f, bottom);
        return true;
    }

    bool DrawInfoWindowAtWorldPos(const ImVec2& anchorMin, const ImVec2& anchorMax,
                                  const std::vector<InfoLine>& lines, ImU32 accentColor) {
        if (lines.empty()) return false;

        ImFont* font = ImGui::GetFont();
        const float fontSize = font ? font->FontSize : ImGui::GetFontSize();
        const float headerSize = fontSize * 1.12f;
        const float iconSize = fontSize * 0.95f;
        const float padX = 10.0f;
        const float padY = 8.0f;
        const float iconGap = 6.0f;
        const float valueGap = 10.0f;
        const float lineGap = 4.0f;

        struct LineMetrics {
            float size;
            float labelW;
            float valueW;
            float height;
        };

        std::vector<LineMetrics> metrics;
        metrics.reserve(lines.size());

        float maxWidth = 0.0f;
        float totalHeight = 0.0f;
        for (size_t i = 0; i < lines.size(); ++i) {
            const float size = (i == 0) ? headerSize : fontSize;
            const auto& line = lines[i];
            ImVec2 labelSize = font
                ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, line.label.c_str())
                : ImGui::CalcTextSize(line.label.c_str());
            ImVec2 valueSize = ImVec2(0.0f, 0.0f);
            if (!line.value.empty()) {
                valueSize = font
                    ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, line.value.c_str())
                    : ImGui::CalcTextSize(line.value.c_str());
            }

            float lineWidth = labelSize.x + (line.value.empty() ? 0.0f : valueGap + valueSize.x);
            if (line.iconName && *line.iconName) {
                lineWidth += iconSize + iconGap;
            }
            if (lineWidth > maxWidth) maxWidth = lineWidth;

            float lineHeight = size + lineGap;
            totalHeight += lineHeight;
            metrics.push_back(LineMetrics{ size, labelSize.x, valueSize.x, lineHeight });
        }

        ImVec2 boxSize(maxWidth + padX * 2.0f, totalHeight + padY * 2.0f);
        const float anchorCenterY = (anchorMin.y + anchorMax.y) * 0.5f;
        const float anchorRightX = anchorMax.x;
        ImVec2 boxPos(anchorRightX + kInfoWindowOffsetX, anchorCenterY - boxSize.y * 0.5f);

        const ImVec2& displaySize = ImGui::GetIO().DisplaySize;
        const float margin = 6.0f;
        if (boxPos.x + boxSize.x > displaySize.x - margin) {
            boxPos.x = displaySize.x - boxSize.x - margin;
        }
        if (boxPos.x < margin) boxPos.x = margin;
        if (boxPos.y + boxSize.y > displaySize.y - margin) {
            boxPos.y = displaySize.y - boxSize.y - margin;
        }
        if (boxPos.y < margin) boxPos.y = margin;

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ImU32 bg = IM_COL32(20, 20, 20, 210);
        ImU32 border = IM_COL32(255, 255, 255, 70);
        drawList->AddRectFilled(boxPos, ImVec2(boxPos.x + boxSize.x, boxPos.y + boxSize.y), bg, 8.0f);
        drawList->AddRect(boxPos, ImVec2(boxPos.x + boxSize.x, boxPos.y + boxSize.y), border, 8.0f);
        drawList->AddRectFilled(boxPos, ImVec2(boxPos.x + 3.0f, boxPos.y + boxSize.y), accentColor, 8.0f, ImDrawFlags_RoundCornersLeft);

        ImU32 labelColor = IM_COL32(210, 210, 210, 255);
        ImU32 valueColor = IM_COL32(245, 245, 245, 255);

        float cursorY = boxPos.y + padY;
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            const auto& metric = metrics[i];
            float cursorX = boxPos.x + padX;

            if (line.iconName && *line.iconName) {
                UIIcon icon{};
                IconLoader::getUIIcon(line.iconName, &icon);
                if (icon.textureId != IL_NO_TEXTURE) {
                    ImVec2 iconPos(cursorX, cursorY + (metric.height - iconSize) * 0.5f);
                    drawList->AddImage(icon.textureId, iconPos,
                        ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                        icon.uv0, icon.uv1, IM_COL32(255, 255, 255, 255));
                }
                cursorX += iconSize + iconGap;
            }

            ImU32 lineLabelColor = (i == 0) ? accentColor : labelColor;
            ImU32 lineValueColor = valueColor;

            if (font) {
                drawList->AddText(font, metric.size, ImVec2(cursorX, cursorY), lineLabelColor, line.label.c_str());
            } else {
                drawList->AddText(ImVec2(cursorX, cursorY), lineLabelColor, line.label.c_str());
            }
            cursorX += metric.labelW;

            if (!line.value.empty()) {
                cursorX += valueGap;
                if (font) {
                    drawList->AddText(font, metric.size, ImVec2(cursorX, cursorY), lineValueColor, line.value.c_str());
                } else {
                    drawList->AddText(ImVec2(cursorX, cursorY), lineValueColor, line.value.c_str());
                }
            }

            cursorY += metric.height;
        }

        return true;
    }

    std::vector<InfoLine> BuildAvatarInfoLines(const char* label, const tsm::game::api::AvatarInfo& info,
                                              float distance, const char* headerIcon) {
        (void)distance;
        char buffer[128];
        std::vector<InfoLine> lines;
        lines.reserve(6);

        InfoLine header;
        header.iconName = (headerIcon && *headerIcon) ? headerIcon : "UiMenuFriends";
        header.label = (label && *label) ? label : "Avatar";
        lines.push_back(std::move(header));

        bool hasHeight = (info.avatarHeight != nullptr);
        bool hasScale = (info.avatarScale != nullptr);
        float height = hasHeight ? *info.avatarHeight : 0.0f;
        float scale = hasScale ? *info.avatarScale : 0.0f;

        unsigned voiceId = 0;
        bool hasVoice = (info.avatarVoice != nullptr);
        if (hasVoice) voiceId = *info.avatarVoice;
        const char* voiceName = (hasVoice && voiceId < tsm::game::data::kVoiceCount)
            ? tsm::game::data::kVoiceNames[voiceId].c_str()
            : "Unknown";

        unsigned stanceId = 0;
        bool hasStance = (info.avatarStance != nullptr);
        if (hasStance) stanceId = *info.avatarStance;
        const char* stanceName = (hasStance && stanceId < tsm::game::data::kStanceCount)
            ? tsm::game::data::kStanceNames[stanceId]
            : "Unknown";

        InfoLine heightLine;
        heightLine.iconName = "UiPersonalityRuler";
        heightLine.label = "Height";
        if (hasHeight) {
            std::snprintf(buffer, sizeof(buffer), "%.2f", height);
        } else {
            std::snprintf(buffer, sizeof(buffer), "N/A");
        }
        heightLine.value = buffer;
        lines.push_back(std::move(heightLine));

        InfoLine scaleLine;
        scaleLine.iconName = "UiPersonalityRuler";
        scaleLine.label = "Scale";
        if (hasScale) {
            std::snprintf(buffer, sizeof(buffer), "%.2f", scale);
        } else {
            std::snprintf(buffer, sizeof(buffer), "N/A");
        }
        scaleLine.value = buffer;
        lines.push_back(std::move(scaleLine));

        InfoLine voiceLine;
        voiceLine.iconName = tsm::game::data::GetVoiceIcon(voiceId);
        voiceLine.label = "Voice";
        std::snprintf(buffer, sizeof(buffer), "%s", voiceName);
        voiceLine.value = buffer;
        lines.push_back(std::move(voiceLine));

        InfoLine stanceLine;
        stanceLine.iconName = tsm::game::data::GetStanceIcon(stanceId);
        stanceLine.label = "Stance";
        std::snprintf(buffer, sizeof(buffer), "%s", stanceName);
        stanceLine.value = buffer;
        lines.push_back(std::move(stanceLine));

        return lines;
    }

    void RenderESPCore(const vec3& worldPos, const char* icon, const std::string& label,
                       float distance, const tsm::state::ModState& ms, ImU32 espColor) {

        if (distance > ms.debug.espMaxDistance) return;

        std::string espText = BuildESPText(label, distance, ms.debug.espShowName, ms.debug.espShowDistance);

        vec2 screenPos;
        if (!tsm::utils::esp::WorldToScreen(worldPos, screenPos)) return;

        bool hasIcon = ms.debug.espShowIcons;
        bool hasText = ms.debug.espShowName || ms.debug.espShowDistance;

        if (hasIcon && hasText) {
            tsm::utils::esp::DrawIconTextAtWorldPos(worldPos, icon, espText.c_str(), kIconSize, espColor);
        } else if (hasIcon) {
            tsm::utils::esp::DrawIconAtWorldPos(worldPos, icon, kIconSize);
        } else if (hasText) {
            tsm::utils::esp::DrawTextAtWorldPos(worldPos, espText.c_str(), espColor, kTextYOffset);
        }

        if (ms.debug.espShowLine) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            const ImVec2& displaySize = ImGui::GetIO().DisplaySize;
            ImVec2 topCenter(displaySize.x * 0.5f, 0.0f);

            float lineEndY = screenPos.y - (hasIcon ? kIconLineOffset : kTextLineOffset);

            dl->AddLine(topCenter, ImVec2(screenPos.x, lineEndY), espColor, kLineThickness);
        }
    }

    ImU32 GetESPColor() {
        ImVec4 accentColor = tsm::ui::GetAccentColor();
        return IM_COL32(
            static_cast<int>(accentColor.x * 255),
            static_cast<int>(accentColor.y * 255),
            static_cast<int>(accentColor.z * 255),
            255
        );
    }

    const char* GetAvatarStanceIcon(int index, const char* fallback) {
        if (index < 0 || index >= tsm::game::api::kMaxAvatars) return fallback;
        std::uintptr_t avatar = tsm::game::api::GetCachedAvatar(index);
        if (avatar == 0) return fallback;

        auto info = tsm::game::api::GetAvatarInfo(avatar);
        if (!info.avatarStance) return fallback;

        unsigned stanceId = *info.avatarStance;
        return tsm::game::data::GetStanceIcon(stanceId);
    }

    void RenderEntityESP(int index, const char* icon, const std::string& label,
                         const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        std::uintptr_t avatar = tsm::game::api::GetCachedAvatar(index);
        if (avatar == 0 || !tsm::game::api::ShouldDisplay(avatar)) return;

        auto info = tsm::game::api::GetAvatarInfo(avatar);
        if (!info.avatarPosition) return;

        vec3& targetPos = *info.avatarPosition;
        float distance = localCache.DistanceTo(targetPos);

        RenderESPCore(targetPos, icon, label, distance, ms, espColor);

        if (ms.debug.espCloseInfo && index != 0 && distance <= kCloseInfoDistance) {
            vec2 screenPosRaw;
            if (tsm::utils::esp::WorldToScreen(targetPos, screenPosRaw)) {
                ImVec2 anchorMin;
                ImVec2 anchorMax;
                BuildEspAnchor(ImVec2(screenPosRaw.x, screenPosRaw.y), label.c_str(), distance,
                               ms.debug.espShowIcons, ms.debug.espShowName, ms.debug.espShowDistance,
                               anchorMin, anchorMax);
                std::vector<InfoLine> infoLines = BuildAvatarInfoLines(label.c_str(), info, distance, icon);
                DrawInfoWindowAtWorldPos(anchorMin, anchorMax, infoLines, espColor);
            }
        }
    }

    void RenderPositionESP(const vec3& pos, const char* icon, const std::string& label,
                          const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        float distance = localCache.DistanceTo(pos);
        RenderESPCore(pos, icon, label, distance, ms, espColor);
    }

    void RenderPlayers(const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        for (int playerIdx = 1; playerIdx < tsm::game::api::kMaxPlayers; ++playerIdx) {
            char nameBuf[32];
            const char* fmt = tsm::ui::i18n::Tr("Player %d");
            std::snprintf(nameBuf, sizeof(nameBuf), fmt, playerIdx);

            const char* icon = GetAvatarStanceIcon(playerIdx, "UiMenuFriends");
            RenderEntityESP(playerIdx, icon, nameBuf, localCache, ms, espColor);
        }
    }

    void RenderNPCs(const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        for (int i = 8; i < tsm::game::api::kMaxAvatars; ++i) {
            char nameBuf[32];
            const char* fmt = tsm::ui::i18n::Tr("NPC %d");
            std::snprintf(nameBuf, sizeof(nameBuf), fmt, i);

            const char* icon = GetAvatarStanceIcon(i, "UiEmoteAP16ArmWave0");
            RenderEntityESP(i, icon, nameBuf, localCache, ms, espColor);
        }
    }

    void RenderWingLights(const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        int count = 0;
        if (int* p = tsm::game::hooks::HookManager::Get().GetWingCountPtr()) {
            count = *p;
        }
        std::uintptr_t first = tsm::game::hooks::HookManager::Get().GetFirstWing();

        if (first == 0 || count <= 0) return;

        for (int i = 0; i < count; ++i) {
            std::uintptr_t addr = first + static_cast<std::uintptr_t>(i) * kWingLightStride;
            const float* v = reinterpret_cast<const float*>(addr);
            vec3 pos{v[0], v[1], v[2]};

            char labelBuf[64];
            const char* fmt = tsm::ui::i18n::Tr("Wing Light %d");
            std::snprintf(labelBuf, sizeof(labelBuf), fmt, i + 1);

            RenderPositionESP(pos, "UiMenuWingBuff", labelBuf, localCache, ms, espColor);
        }
    }

    void RenderDyes(const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        int dyeCount = 0;
        if (int* p = tsm::game::hooks::HookManager::Get().GetDyeCountPtr()) {
            dyeCount = *p;
        }
        std::uintptr_t first = tsm::game::hooks::HookManager::Get().GetFirstDye();

        if (first == 0 || dyeCount <= 0) return;

        std::vector<Position3D> allDyes;
        allDyes.reserve(dyeCount);

        for (int i = 0; i < dyeCount; ++i) {
            std::uintptr_t addr = first + static_cast<std::uintptr_t>(i) * kDyeStride;
            const float* v = reinterpret_cast<const float*>(addr);
            allDyes.push_back(Position3D{v[0], v[1], v[2]});
        }

        std::vector<Position3D> clusters = ClusterPositions(allDyes, kDyeClusterRadius);

        for (size_t i = 0; i < clusters.size(); ++i) {
            vec3 pos = clusters[i].ToVec3();
            char labelBuf[64];
            const char* fmt = tsm::ui::i18n::Tr("Dye %d");
            std::snprintf(labelBuf, sizeof(labelBuf), fmt, static_cast<int>(i + 1));

            RenderPositionESP(pos, "UiMenuDye", labelBuf, localCache, ms, espColor);
        }
    }

    void RenderCandles(const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        int candleCount = tsm::game::api::CandleCount();
        std::uintptr_t first = tsm::game::api::FirstCandle();

        if (first == 0 || candleCount <= 0) return;

        std::vector<Position3D> redCandles;
        std::vector<Position3D> seasonCandles;

        for (int i = 0; i < candleCount; ++i) {
            if (!tsm::game::api::CandleIsActive(i)) continue;

            int typeIndex = tsm::game::api::CandleTypeIndex(i);
            if (typeIndex != 1 && typeIndex != 6 && typeIndex != 8) continue;

            std::uintptr_t addr = first + static_cast<std::uintptr_t>(i) * kCandleStride;
            const float* v = reinterpret_cast<const float*>(addr);
            Position3D pos{v[0], v[1], v[2]};

            if (typeIndex == 6) {
                seasonCandles.push_back(pos);
            } else {
                redCandles.push_back(pos);
            }
        }

        if (!redCandles.empty()) {
            std::vector<Position3D> redClusters = ClusterPositions(redCandles, kCandleClusterRadius);
            const char* redLabel = tsm::ui::i18n::Tr("Red Candle");

            for (const auto& cluster : redClusters) {
                vec3 pos = cluster.ToVec3();
                RenderPositionESP(pos, "UiMenuCandle", redLabel, localCache, ms, espColor);
            }
        }

        if (!seasonCandles.empty()) {
            std::vector<Position3D> seasonClusters = ClusterPositions(seasonCandles, kCandleClusterRadius);
            const char* seasonLabel = tsm::ui::i18n::Tr("Season Candle");

            for (const auto& cluster : seasonClusters) {
                vec3 pos = cluster.ToVec3();
                RenderPositionESP(pos, "UiMenuSeasonCandle", seasonLabel, localCache, ms, espColor);
            }
        }
    }

    void RenderSelectedEntity(int index, const char* icon, const std::string& label,
                              const LocalPlayerCache& localCache, const tsm::state::ModState& ms, ImU32 espColor) {
        RenderEntityESP(index, icon, label, localCache, ms, espColor);
    }

}

void RenderESP() {
    auto& ms = tsm::state::ModState::Get();
    if (!ms.debug.espEnabled || !tsm::utils::esp::IsAvailable()) return;

    LocalPlayerCache localCache;
    localCache.Update();

    ImU32 espColor = GetESPColor();

    int socialSubIndex = tsm::ui::tabs::GetSocialSubIndex();
    int selectedPlayerIndex = tsm::ui::tabs::GetSelectedPlayerIndex();
    int selectedNPCIndex = tsm::ui::tabs::GetSelectedNPCIndex();

    bool hasSelectedPlayer = (socialSubIndex == 1 && selectedPlayerIndex >= 0);
    bool hasSelectedNPC = (socialSubIndex == 2 && selectedNPCIndex >= 0);

    if (hasSelectedPlayer) {
        const char* icon;
        std::string label;

        if (selectedPlayerIndex == 0) {
            label = tsm::ui::i18n::Tr("You");
            icon = GetAvatarStanceIcon(selectedPlayerIndex, "UiEmoteStanceHero");
        } else if (selectedPlayerIndex < tsm::game::api::kMaxPlayers) {
            char nameBuf[32];
            const char* fmt = tsm::ui::i18n::Tr("Player %d");
            std::snprintf(nameBuf, sizeof(nameBuf), fmt, selectedPlayerIndex);
            label = nameBuf;
            icon = GetAvatarStanceIcon(selectedPlayerIndex, "UiMenuFriends");
        } else {
            char nameBuf[32];
            const char* fmt = tsm::ui::i18n::Tr("NPC %d");
            std::snprintf(nameBuf, sizeof(nameBuf), fmt, selectedPlayerIndex);
            label = nameBuf;
            icon = GetAvatarStanceIcon(selectedPlayerIndex, "UiEmoteAP16ArmWave0");
        }

        RenderSelectedEntity(selectedPlayerIndex, icon, label, localCache, ms, espColor);
        return;
    }

    if (hasSelectedNPC) {
        char nameBuf[32];
        const char* fmt = tsm::ui::i18n::Tr("NPC %d");
        std::snprintf(nameBuf, sizeof(nameBuf), fmt, selectedNPCIndex);

        const char* icon = GetAvatarStanceIcon(selectedNPCIndex, "UiEmoteAP16ArmWave0");
        RenderSelectedEntity(selectedNPCIndex, icon, nameBuf, localCache, ms, espColor);
        return;
    }

    if (ms.debug.espShowPlayers) {
        RenderPlayers(localCache, ms, espColor);
    }

    if (ms.debug.espShowNPCs) {
        RenderNPCs(localCache, ms, espColor);
    }

    if (ms.debug.espShowWingLights) {
        RenderWingLights(localCache, ms, espColor);
    }

    if (ms.debug.espShowDyes) {
        RenderDyes(localCache, ms, espColor);
    }

    if (ms.debug.espShowCandles) {
        RenderCandles(localCache, ms, espColor);
    }
}

}}}
