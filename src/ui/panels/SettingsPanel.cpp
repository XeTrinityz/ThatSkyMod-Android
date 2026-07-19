#include <ui/panels/SettingsPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/helpers/SubTabRenderer.h>
#include <ui/data/UIData.h>
#include <ui/overlays/GameOverlay.h>
#include <ui/overlays/FavouritesOverlay.h>
#include <imgui/imgui.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <state/ModState.h>
#include <iconloader/IconLoader.h>
#include <ui/helpers/Toast.h>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <game/memory/api.h>
#include <game/hooks/HookManager.h>
#include <utils/common/AccountHelper.h>
#include <utils/common/DataExportImport.h>
#include <fileselector/fileselector.h>
#include <data/DataManager.h>
#include <progression/CandleRunner.h>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace tsm { namespace ui { namespace tabs {

static int s_settingsSubIndex = 0;
static int s_uiSubTab = 0;

static bool s_snakeGameEnabled = false;
static bool s_flappybirdGameEnabled = false;
static bool s_pongGameEnabled = false;
static bool s_breakoutGameEnabled = false;
static bool s_memoryMatchGameEnabled = false;
static bool s_doomGameEnabled = false;

namespace {
    static std::vector<tsm::utils::SavedAccount> s_savedAccounts;
    static char s_newAccountName[128] = "";
    static bool s_showSaveModal = false;
    static bool s_showEditModal = false;
    static bool s_showSwitchConfirm = false;
    static bool s_showCreateLocalConfirm = false;
    static bool s_showSwitchTypeConfirm = false;
    static int s_editAccountIdx = -1;
    static int s_switchAccountIdx = -1;
    static int s_pendingAccountType = -1;
    static bool s_accountsLoaded = false;

    static bool s_importAccountSuccess = false;
    static bool s_importAccountError = false;
    static std::string s_importAccountErrorMsg;
    static bool s_shouldReloadAccounts = false;

    static void OnAccountFileSelected(int fd) {
        std::string error;
        std::string importedName;
        if (tsm::utils::ImportAccountFromFd(fd, error, &importedName)) {
            s_importAccountSuccess = true;
            s_shouldReloadAccounts = true;
        } else {
            s_importAccountError = true;
            s_importAccountErrorMsg = error;
        }
    }

    static bool s_importTranslationSuccess = false;
    static bool s_importTranslationError = false;
    static std::string s_importTranslationErrorMsg;

    static bool s_showDeleteTranslationConfirm = false;
    static int  s_deleteTranslationIndex = -1;

    static void OnTranslationFileSelected(int fd) {
        char errBuf[256] = {0};
        int newLangIndex = -1;
        if (tsm::ui::i18n::ImportTranslationFromFd(fd, &newLangIndex, errBuf, sizeof(errBuf))) {
            s_importTranslationSuccess = true;
            auto& ms = tsm::state::ModState::Get();
            if (newLangIndex >= 0) {
                ms.ui.languageId = newLangIndex;
                tsm::ui::i18n::SetActiveLanguageIndex(newLangIndex);
            }
        } else {
            s_importTranslationError = true;
            if (errBuf[0] != '\0') {
                s_importTranslationErrorMsg = errBuf;
            } else {
                s_importTranslationErrorMsg = "Failed to import translation";
            }
        }
    }

    static std::vector<std::string> s_collapseIconNames;
    static bool s_collapseIconNamesLoaded = false;

    static void EnsureCollapseIconListLoaded() {
        if (s_collapseIconNamesLoaded) return;
        s_collapseIconNamesLoaded = true;

        s_collapseIconNames = {
            "UiMenuArrowCollapse",
            "UiMenuBack",
            "UiMenuBrightness",
            "UiLightBulb",
            "UiMiscStar",
            "UiMiscRoundedStar",
            "UiMiscStarEmpty",
            "UiMiscStarLarge",
            "UiMiscDiamond",
            "UiMiscDot",
            "UiMiscHeart",
            "UiMiscHeartOutline",
            "UiMiscHeartOutlineThick",
            "UiMiscFavorite",
            "UiMiscFavoriteOutline",
            "UiMiscPlusMedium",
            "UiMiscPlusBig",
            "UiMiscMinus",
            "UiMiscQuestion",
            "UiMiscQuestionSmall",
            "UiMiscExclamation",
            "UiMiscCheck",
            "UiMiscCheckBadge",
            "UiMiscHide",
            "UiMiscTarget",
            "UiMiscMusicNote",
            "UiMiscDiamond",
            "UiMiscBubbles",

            "UiMiscArrowLeft",
            "UiMiscArrowRight",
            "UiButtonRightPS",
            "UiButtonUpPS",
            "UiButtonDownPS",

            "UiMenuAddCandle",
            "UiMenuAP10Flat",
            "UiMenuAP16Flat",
            "UiMenuAP22Flat",
            "UiMenuBattery",
            "UiMenuBattery01",
            "UiMenuBattery02",
            "UiMenuBattery03",
            "UiMenuBattery04",
            "UiMenuBlur",

            "UiHudStarSplit2",
            "UiHudStarSplit3",
            "UiHudStarSplit4",
            "UiHudStarSplit5",
            "UiHudStarSplit6",
            "UiHudStarSplit7",
            "UiHudWaypointBg",
            "UiHudWaypointOutline",
            "UiHudShieldSplit2",
            "UiHudShieldSplit3"
        };
    }
}

void DrawSettingsTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;

    static const char* kSubIcons[] = { "UiMenuBrightness", "UiMiscView", "UiMenuCreateAccount", "UiMenuSaveDisk", "UiMiscController", "UiMiscStar" };
    DrawSubTabs(kSubIcons, 6, s_settingsSubIndex);

    if (BeginCard("##settings_card", 8.0f, false)) {
        switch (s_settingsSubIndex) {
            case 0: {
                static const char* kUISubIcons[] = { "UiMenuDye", "UiPersonalityRuler", "UiMenuBrightness", "UiMiscFlame" };
                DrawSubTabs(kUISubIcons, 4, s_uiSubTab, 0.75f);

                if (BeginPannableChild("##ui_settings_scroll")) {
                    if (s_uiSubTab == 0) {
                        CenterSeparator("Accent Color");

                    using namespace tsm::ui::data;
                    const auto& kPalette = kAccentColorPalette;
                    const int color_count = kAccentColorPaletteSize;

                    const float d = DP(40.0f);
                    const float gap = DP(10.0f);
                    float avail_w2 = ImGui::GetContentRegionAvail().x;
                    int per_row = (int)((avail_w2 + gap) / (d + gap));
                    if (per_row < 3) per_row = 3;
                    if (per_row > color_count) per_row = color_count;

                    ImVec4 current = tsm::ui::GetAccentColor();

                    int idx = 0;
                    while (idx < color_count) {
                        int items_in_row = std::min(per_row, color_count - idx);
                        float row_w = items_in_row * d + (items_in_row - 1) * gap;
                        float left_pad2 = std::max(0.0f, (avail_w2 - row_w) * 0.5f);

                        ImVec2 row_min = ImGui::GetCursorScreenPos();
                        ImGui::Dummy(ImVec2(0, d));
                        for (int col = 0; col < items_in_row; ++col, ++idx) {
                            float px = row_min.x + left_pad2 + col * (d + gap);
                            float py = row_min.y;
                            ImGui::PushID(idx);
                            ImGui::SetCursorScreenPos(ImVec2(px, py));
                            ImGui::InvisibleButton("##color", ImVec2(d, d));
                            bool clicked = ImGui::IsItemClicked();

                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            float r = d * 0.5f;
                            ImVec2 c = ImVec2(px + r, py + r);

                            const ImVec4 colv = kPalette[idx];
                            float dist = std::fabs(colv.x - current.x) + std::fabs(colv.y - current.y) + std::fabs(colv.z - current.z);
                            bool selected = (dist < 0.03f);

                            ImU32 fill = ImGui::GetColorU32(ImVec4(colv.x, colv.y, colv.z, 1.0f));
                            ImU32 border = ImGui::GetColorU32(selected ? ImVec4(1,1,1, 0.95f)
                                                                    : ImVec4(1,1,1, 0.28f));
                            dl->AddCircleFilled(c, r, fill, 48);
                            dl->AddCircle(c, r, border, 48, 2.0f);

                            if (clicked) {
                                tsm::ui::SetAccentColor(colv);
                                current = colv;
                            }
                            ImGui::PopID();
                        }
                        if (idx < color_count) ImGui::Dummy(ImVec2(0, gap));
                    }

                        {
                            auto& ms = tsm::state::ModState::Get();
                            ImGui::Dummy(ImVec2(0, DP(12.0f)));
                            ToggleCardIcon("Rainbow Mode", "Slowly cycle through preset accent colors", "UiMenuSwitchMisc", &ms.ui.accentRainbow);
                        }
                        CenterSeparator("Accent Options");
                        {
                            ImVec4 customAccent = tsm::ui::GetAccentColor();
                            if (ColorPickerCard("Custom Color", "Pick a manual accent color", "UiMenuDye", &customAccent)) {
                                tsm::ui::SetAccentColor(customAccent);
                                auto& ms = tsm::state::ModState::Get();
                                if (ms.ui.accentRainbow) {
                                    ms.ui.accentRainbow = false;
                                }
                            }
                        }
                        CenterSeparator("Language & Translations");
                        {
                            auto& ms = tsm::state::ModState::Get();
                            if (s_importTranslationSuccess) {
                                ShowToastSuccess("Translation imported successfully");
                                s_importTranslationSuccess = false;
                            }
                            if (s_importTranslationError) {
                                if (!s_importTranslationErrorMsg.empty()) {
                                    ShowToastError(s_importTranslationErrorMsg.c_str());
                                } else {
                                    ShowToastError("Failed to import translation");
                                }
                                s_importTranslationError = false;
                            }

                            if (ButtonCard("Import Translation",
                                           "Add a new language pack from a JSON file",
                                           "UiMenuSwitchLanguage",
                                           "IMPORT")) {
                                requestFile("application/json", OnTranslationFileSelected, false);
                            }

                            CenterSeparator("Available Languages");

                            int activeIndex = ms.ui.languageId;
                            if (activeIndex < 0) activeIndex = 0;

                            bool englishSelected = (activeIndex == 0);
                            const char* englishDesc = "Default interface language";
                            const char* englishIcon = englishSelected ? "UiMiscCheck" : "UiMenuSwitchLanguage";
                            auto englishResult = EditableCard("English", englishDesc, englishIcon, englishSelected);
                            if (englishResult.main) {
                                activeIndex = 0;
                                ms.ui.languageId = 0;
                                tsm::ui::i18n::SetActiveLanguageIndex(0);
                            }
                            if (englishResult.edit) {
                                ShowToastInfo("Built-in English language cannot be deleted");
                            }

                            int packCount = tsm::ui::i18n::GetTranslationPackCount();
                            for (int i = 0; i < packCount; ++i) {
                                tsm::ui::i18n::TranslationPackInfo info{};
                                if (!tsm::ui::i18n::GetTranslationPackInfo(i, info)) {
                                    continue;
                                }

                                int langIndex = i + 1;
                                bool selected = (activeIndex == langIndex);

                                std::string title;
                                if (info.name && *info.name) {
                                    title = info.name;
                                } else if (info.id && *info.id) {
                                    title = info.id;
                                } else {
                                    title = "Translation";
                                }

                                std::string desc;
                                if (info.author && *info.author) {
                                    desc += info.author;
                                }
                                if (info.version && *info.version) {
                                    if (!desc.empty()) desc += " | ";
                                    desc += "v";
                                    desc += info.version;
                                }

                                const char* icon = selected ? "UiMiscCheck" : "UiMenuSwitchLanguage";
                                const char* descPtr = desc.empty() ? nullptr : desc.c_str();

                                ImGui::PushID(i);
                                auto result = EditableCard(title.c_str(), descPtr, icon, selected);
                                ImGui::PopID();

                                if (result.main) {
                                    activeIndex = langIndex;
                                    ms.ui.languageId = langIndex;
                                    tsm::ui::i18n::SetActiveLanguageIndex(langIndex);
                                }
                                if (result.edit) {
                                    s_deleteTranslationIndex = i;
                                    s_showDeleteTranslationConfirm = true;
                                }
                            }
                        }
                    } else if (s_uiSubTab == 1) {
                        CenterSeparator("Panel Sizing");
                    {
                        auto& ms = tsm::state::ModState::Get();
                        int wdp = ms.ui.panelWidthDp;
                        if (IntCardIcon("Content Width", "Adjust the width of the main content area", "UiPersonalityRuler", &wdp, 360, 720)) {
                            ms.ui.panelWidthDp = wdp;
                        }
                    }

                    {
                        auto& ms = tsm::state::ModState::Get();
                        if (ToggleCardIcon("Show Scrollbars", "Show or hide scrollbars in the UI", "UiMenuSwitchMisc", &ms.ui.scrollbarVisible)) {
                            ImGui::GetStyle().ScrollbarSize = ms.ui.scrollbarVisible ? (float)std::clamp(ms.ui.scrollbarWidthPx, 20, 40) : 0.0f;
                        }
                    }

                    {
                        auto& ms = tsm::state::ModState::Get();
                        int sbw = ms.ui.scrollbarWidthPx;
                        if (!ms.ui.scrollbarVisible) ImGui::BeginDisabled(true);
                        if (IntCardIcon("Scrollbar Width", "Adjust thickness of scrollbars (px)", "UiPersonalityRuler", &sbw, 20, 40)) {
                            ms.ui.scrollbarWidthPx = sbw;
                            if (ms.ui.scrollbarVisible) {
                                ImGui::GetStyle().ScrollbarSize = (float)sbw;
                            }
                        }
                        if (!ms.ui.scrollbarVisible) ImGui::EndDisabled();
                    }


                        CenterSeparator("Docking");
                        {
                            bool dockRight = tsm::ui::IsDockedRight();
                            const char* title = dockRight ? "Dock on Left" : "Dock on Right";
                            const char* desc  = dockRight ? "Move the panel to the left screen edge" : "Move the panel to the right screen edge";
                            const char* icon  = dockRight ? "UiMiscArrowLeft" : "UiMiscArrowRight";
                            if (ButtonCard(title, desc, icon, "SWITCH")) {
                                tsm::ui::SetDockedRight(!dockRight);
                            }
                        }

                        CenterSeparator("Collapse Handle");
                        {
                            auto& ms = tsm::state::ModState::Get();
                            static const char* kHandleStyles[] = {
                                "Classic Arrow",
                                "Minimal Bar",
                                "Pill Button",
                                "Accent Icon",
                                "Grip Lines"
                            };
                            int style = ms.ui.collapseHandleStyle;
                            if (style < 0 || style >= 5) style = 0;
                            if (SelectCardIcon("Collapse Handle Style",
                                              "Choose how the side collapse handle looks",
                                              "UiMenuSwitchMisc",
                                              &style,
                                              kHandleStyles,
                                              5)) {
                                ms.ui.collapseHandleStyle = style;
                            }

                            if (ms.ui.collapseHandleStyle == 3) {
                                EnsureCollapseIconListLoaded();
                                if (!s_collapseIconNames.empty()) {
                                    int iconCount = (int)s_collapseIconNames.size();
                                    int iconIndex = ms.ui.collapseHandleIconIndex;
                                    if (iconIndex < 0 || iconIndex >= iconCount) {
                                        iconIndex = 0;
                                    }

                                    if (ms.ui.collapseHandleIconName.empty()) {
                                        ms.ui.collapseHandleIconName = s_collapseIconNames[iconIndex];
                                    } else {
                                        bool found = false;
                                        for (int i = 0; i < iconCount; ++i) {
                                            if (s_collapseIconNames[i] == ms.ui.collapseHandleIconName) {
                                                iconIndex = i;
                                                found = true;
                                                break;
                                            }
                                        }
                                        if (!found) {
                                            iconIndex = 0;
                                            ms.ui.collapseHandleIconName = s_collapseIconNames[0];
                                        }
                                    }

                                    ms.ui.collapseHandleIconIndex = iconIndex;

                                    std::vector<const char*> iconLabels;
                                    iconLabels.reserve(iconCount);
                                    for (int i = 0; i < iconCount; ++i) {
                                        iconLabels.push_back(s_collapseIconNames[i].c_str());
                                    }

                                    int sel = iconIndex;
                                    if (SelectCardIconList("Handle Icon",
                                                          "Pick which icon the Accent handle uses",
                                                          &sel,
                                                          iconLabels.data(),
                                                          iconCount)) {
                                        if (sel < 0) sel = 0;
                                        if (sel >= iconCount) sel = iconCount - 1;
                                        ms.ui.collapseHandleIconIndex = sel;
                                        ms.ui.collapseHandleIconName = s_collapseIconNames[sel];
                                    }
                                }
                            }
                        }
                    } else if (s_uiSubTab == 2) {
                        CenterSeparator("Performance");
                        if (ButtonCard("Cycle FPS Limit", "Cycle FPS between 30, 60, and refresh rate", "UiMenuGraphicsFpsMax", "CYCLE")) {
                            tsm::lua::queue::Enqueue("TFpsSelector()");
                        }
                        if (ButtonCard("Cycle Resolution", "Cycle graphics between Simple, Medium, High and Max", "UiMenuGraphicsSettings", "CYCLE")) {
                            tsm::lua::queue::Enqueue("TResolutionSelector()");
                        }
                        CenterSeparator("Overlays & Info");
                    {
                        auto& ms = tsm::state::ModState::Get();

                        ToggleCardIcon("Show Debug Overlay",
                                      "Display game information overlay on screen",
                                      "UiMenuBrightness",
                                      &ms.debug.showOverlay);

                        ToggleCardIcon(tsm::ui::i18n::Tr("Show Sky Clock"),
                                      tsm::ui::i18n::Tr("Display server time and upcoming in-game events"),
                                      "UiMenuTimer",
                                      &ms.debug.showSkyClock);

                            int clockPos = ms.debug.skyClockPosition;
                            if (SelectCardIcon(tsm::ui::i18n::Tr("Clock Position"),
                                              tsm::ui::i18n::Tr("Choose where the Sky Clock appears on screen"),
                                              "UiMiscTarget",
                                              &clockPos,
                                              tsm::ui::data::kSkyClockPositions,
                                              tsm::ui::data::kSkyClockPositionCount)) {
                                ms.debug.skyClockPosition = clockPos;
                            }

                        bool favoritesVisible = ms.ui.showFavoritesRadial;
                        if (ToggleCardIcon("Favorites Radial Menu",
                                          "Show floating favorites radial button",
                                          "UiMiscFavorite",
                                          &favoritesVisible)) {
                            ms.ui.showFavoritesRadial = favoritesVisible;
                            tsm::ui::overlays::SetfavoritesVisible(favoritesVisible);
                        }
                        }
                    } else if (s_uiSubTab == 3) {
                        CenterSeparator("Background Animation");
                        {
                            auto& ms = tsm::state::ModState::Get();

                            static const char* kEffectTypes[] = {
                                "None",
                                "Neuron Network",
                                "Floating Particles",
                                "Animated Waves",
                                "Starfield",
                                "Fireflies",
                                "Snow"
                            };
                            int effectType = ms.ui.backgroundEffectType;
                            if (SelectCardIcon("Background Effect", "Choose animated background style", "UiMiscFlame", &effectType, kEffectTypes, 7)) {
                                ms.ui.backgroundEffectType = effectType;
                            }

                            if (ms.ui.backgroundEffectType == 1) {
                                int nodeCount = ms.ui.neuronNodeCount;
                                if (IntCardIcon("Node Count", "Number of animated orbs", "UiPersonalityRuler", &nodeCount, 10, 50)) {
                                    ms.ui.neuronNodeCount = nodeCount;
                                }

                                float speed = ms.ui.neuronSpeed;
                                if (FloatCardIcon("Movement Speed", "How fast the orbs move", "UiPersonalityRuler", &speed, 10.0f, 100.0f)) {
                                    ms.ui.neuronSpeed = speed;
                                }

                                float connDist = ms.ui.neuronConnectionDistance;
                                if (FloatCardIcon("Connection Range", "Max distance to draw lines between orbs", "UiPersonalityRuler", &connDist, 50.0f, 300.0f)) {
                                    ms.ui.neuronConnectionDistance = connDist;
                                }

                                float glowIntensity = ms.ui.neuronGlowIntensity;
                                if (FloatCardIcon("Glow Intensity", "Brightness of the glow around orbs", "UiPersonalityRuler", &glowIntensity, 0.0f, 1.0f)) {
                                    ms.ui.neuronGlowIntensity = glowIntensity;
                                }
                            }

                            if (ms.ui.backgroundEffectType == 2) {
                                int particleCount = ms.ui.particleCount;
                                if (IntCardIcon("Particle Count", "Number of floating particles", "UiPersonalityRuler", &particleCount, 20, 200)) {
                                    ms.ui.particleCount = particleCount;
                                }

                                float pSpeed = ms.ui.particleSpeed;
                                if (FloatCardIcon("Float Speed", "How fast particles drift upward", "UiPersonalityRuler", &pSpeed, 5.0f, 50.0f)) {
                                    ms.ui.particleSpeed = pSpeed;
                                }

                                float pSize = ms.ui.particleSize;
                                if (FloatCardIcon("Particle Size", "Size of each particle", "UiPersonalityRuler", &pSize, 1.0f, 8.0f)) {
                                    ms.ui.particleSize = pSize;
                                }
                            }

                            if (ms.ui.backgroundEffectType == 3) {
                                int waveCount = ms.ui.waveCount;
                                if (IntCardIcon("Wave Count", "Number of wave layers", "UiPersonalityRuler", &waveCount, 3, 10)) {
                                    ms.ui.waveCount = waveCount;
                                }

                                float wSpeed = ms.ui.waveSpeed;
                                if (FloatCardIcon("Wave Speed", "Animation speed of waves", "UiPersonalityRuler", &wSpeed, 5.0f, 30.0f)) {
                                    ms.ui.waveSpeed = wSpeed;
                                }

                                float wAmplitude = ms.ui.waveAmplitude;
                                if (FloatCardIcon("Wave Height", "Vertical amplitude of waves", "UiPersonalityRuler", &wAmplitude, 10.0f, 60.0f)) {
                                    ms.ui.waveAmplitude = wAmplitude;
                                }

                                float wThickness = ms.ui.waveThickness;
                                if (FloatCardIcon("Line Thickness", "Thickness of wave lines", "UiPersonalityRuler", &wThickness, 0.5f, 5.0f)) {
                                    ms.ui.waveThickness = wThickness;
                                }
                            }

                            if (ms.ui.backgroundEffectType == 4) {
                                int starCnt = ms.ui.starCount;
                                if (IntCardIcon("Star Count", "Number of stars", "UiPersonalityRuler", &starCnt, 50, 300)) {
                                    ms.ui.starCount = starCnt;
                                }

                                float twinkleSpeed = ms.ui.starTwinkleSpeed;
                                if (FloatCardIcon("Twinkle Speed", "How fast stars twinkle", "UiPersonalityRuler", &twinkleSpeed, 0.2f, 3.0f)) {
                                    ms.ui.starTwinkleSpeed = twinkleSpeed;
                                }

                                float starSize = ms.ui.starSizeMax;
                                if (FloatCardIcon("Max Star Size", "Maximum size of stars", "UiPersonalityRuler", &starSize, 1.0f, 5.0f)) {
                                    ms.ui.starSizeMax = starSize;
                                }
                            }

                            if (ms.ui.backgroundEffectType == 5) {
                                int fireflyCnt = ms.ui.fireflyCount;
                                if (IntCardIcon("Firefly Count", "Number of glowing fireflies", "UiPersonalityRuler", &fireflyCnt, 20, 100)) {
                                    ms.ui.fireflyCount = fireflyCnt;
                                }

                                float fireflySpeed = ms.ui.fireflySpeed;
                                if (FloatCardIcon("Flight Speed", "How fast fireflies move", "UiPersonalityRuler", &fireflySpeed, 5.0f, 40.0f)) {
                                    ms.ui.fireflySpeed = fireflySpeed;
                                }

                                float fireflyGlow = ms.ui.fireflyGlowIntensity;
                                if (FloatCardIcon("Glow Intensity", "Brightness of firefly glow", "UiPersonalityRuler", &fireflyGlow, 0.0f, 1.0f)) {
                                    ms.ui.fireflyGlowIntensity = fireflyGlow;
                                }
                            }

                            if (ms.ui.backgroundEffectType == 6) {
                                int snowCnt = ms.ui.snowflakeCount;
                                if (IntCardIcon("Snowflake Count", "Number of falling snowflakes", "UiPersonalityRuler", &snowCnt, 50, 150)) {
                                    ms.ui.snowflakeCount = snowCnt;
                                }

                                float snowSpeed = ms.ui.snowFallSpeed;
                                if (FloatCardIcon("Fall Speed", "How fast snowflakes fall", "UiPersonalityRuler", &snowSpeed, 1.0f, 30.0f)) {
                                    ms.ui.snowFallSpeed = snowSpeed;
                                }

                                float snowDrift = ms.ui.snowDriftAmount;
                                if (FloatCardIcon("Snowflake Drift", "Distance snowflakes drift", "UiPersonalityRuler", &snowDrift, 1.0f, 20.0f)) {
                                    ms.ui.snowDriftAmount = snowDrift;
                                }
                            }
                        }
                    }

                    CenterSeparator("Actions");
                    if (StandardButton("Save Settings")) {
                        auto& ms = tsm::state::ModState::Get();
                        ms.ui.dockRight = tsm::ui::IsDockedRight();
                        bool ok = ms.SaveToFile();
                        if (ok) tsm::ui::helpers::ShowToastSuccess("Settings saved");
                        else    tsm::ui::helpers::ShowToastError("Failed to save settings");
                    }

                    if (StandardButton("Reset to Defaults", false)) {
                        ImGui::OpenPopup("##confirm_reset");
                    }
                    ImGui::Dummy(ImVec2(0, DP(1.0f)));
                    if (ConfirmPopup("##confirm_reset", "Reset Settings", "Reset all settings to defaults?", "Reset", "Cancel")) {
                        auto& ms = tsm::state::ModState::Get();
                        ms.ResetToDefaults();
                        ms.SaveToFile();
                        tsm::ui::helpers::ShowToastSuccess("Settings reset");
                    }

                }
                EndPannableChild();

                if (s_showDeleteTranslationConfirm) {
                    ImGui::OpenPopup("##DeleteTranslationConfirm");
                    s_showDeleteTranslationConfirm = false;
                }

                if (s_deleteTranslationIndex >= 0) {
                    tsm::ui::i18n::TranslationPackInfo info{};
                    const char* packName = "this translation";
                    if (tsm::ui::i18n::GetTranslationPackInfo(s_deleteTranslationIndex, info)) {
                        if (info.name && *info.name) {
                            packName = info.name;
                        } else if (info.id && *info.id) {
                            packName = info.id;
                        }
                    }

                    char msg[256];
                    const char* fmt = tsm::ui::i18n::Tr("Delete translation '%s'?\n\nThis cannot be undone.");
                    std::snprintf(msg, sizeof(msg),
                                  fmt,
                                  packName);

                    if (ConfirmPopup("##DeleteTranslationConfirm",
                                     "Delete Translation",
                                     msg,
                                     "Delete",
                                     "Cancel")) {
                        if (tsm::ui::i18n::DeleteTranslationPack(s_deleteTranslationIndex)) {
                            auto& ms = tsm::state::ModState::Get();
                            ms.ui.languageId = tsm::ui::i18n::GetActiveLanguageIndex();
                            ShowToastSuccess("Translation deleted");
                        } else {
                            ShowToastError("Failed to delete translation");
                        }
                        s_deleteTranslationIndex = -1;
                    } else if (!ImGui::IsPopupOpen("##DeleteTranslationConfirm")) {
                        s_deleteTranslationIndex = -1;
                    }
                }

                break; }
            case 1: {
                if (BeginPannableChild("##esp_scroll")) {
                    CenterSeparator("ESP Settings");

                    auto& ms = tsm::state::ModState::Get();

                    if (ToggleCardIcon("Enable ESP",
                                      "Show ESP overlays on tracked targets",
                                      "UiMenuMap",
                                      &ms.debug.espEnabled)) {
                    }

                    if (!ms.debug.espEnabled) ImGui::BeginDisabled(true);

                    if (ToggleCardIcon("Show Icons",
                                      "Display type icon for tracked targets",
                                      "UiMenuStarScan",
                                      &ms.debug.espShowIcons)) {
                    }

                    if (ToggleCardIcon("Show Label",
                                      "Display text label for tracked targets",
                                      "UiMenuFriends",
                                      &ms.debug.espShowName)) {
                    }

                    if (ToggleCardIcon("Show Distance",
                                      "Display distance to tracked targets",
                                      "UiPersonalityRuler",
                                      &ms.debug.espShowDistance)) {
                    }

                    if (ToggleCardIcon("Show Line",
                                      "Draw line from screen top to target",
                                      "UiMiscTarget",
                                      &ms.debug.espShowLine)) {
                    }

                    if (ToggleCardIcon("Close Info",
                                      "Show detailed info when close to players and NPCs",
                                      "UiMenuQuestHint",
                                      &ms.debug.espCloseInfo)) {
                    }

                    if (FloatCardIcon("Max Distance",
                                     "Maximum distance to show ESP overlays",
                                     "UiPersonalityRuler",
                                     &ms.debug.espMaxDistance,
                                     10.0f,
                                     1000.0f,
						             250.0f,
                                     "%.0f")) {
                    }

                    CenterSeparator("ESP Types");

                    if (ToggleCardIcon("Player ESP",
                                      "Show ESP on all players in session",
                                      "UiMenuFriends",
                                      &ms.debug.espShowPlayers)) {
                    }

                    if (ToggleCardIcon("NPC ESP",
                                      "Show ESP on all NPCs in session",
                                      "UiEmoteAP16ArmWave0",
                                      &ms.debug.espShowNPCs)) {
                    }

                    if (ToggleCardIcon("Wing Light ESP",
                                      "Show ESP on wing lights/collectibles",
                                      "UiMenuWingBuff",
                                      &ms.debug.espShowWingLights)) {
                    }

                    if (ToggleCardIcon("Dye ESP",
                                      "Show ESP on color dyes",
                                      "UiMenuDye",
                                      &ms.debug.espShowDyes)) {
                    }

                    if (ToggleCardIcon("Candle ESP",
                                      "Show ESP on map candles",
                                      "UiMenuBuffCandle",
                                      &ms.debug.espShowCandles)) {
                    }

                    if (!ms.debug.espEnabled) ImGui::EndDisabled();
                }
                EndPannableChild();

                break; }
            case 2: {
                if (BeginPannableChild("##accounts_scroll")) {
                    if (!s_accountsLoaded) {
                        tsm::utils::LoadAccountsFromDirectory(s_savedAccounts);
                        s_accountsLoaded = true;
                    }

                    if (s_shouldReloadAccounts) {
                        tsm::utils::LoadAccountsFromDirectory(s_savedAccounts);
                        s_shouldReloadAccounts = false;
                    }

                    if (s_importAccountSuccess) {
                        ShowToastSuccess("Account imported successfully");
                        s_importAccountSuccess = false;
                    }
                    if (s_importAccountError) {
                        if (!s_importAccountErrorMsg.empty()) {
                            ShowToastError(s_importAccountErrorMsg.c_str());
                        } else {
                            ShowToastError("Failed to import account");
                        }
                        s_importAccountError = false;
                    }

                    std::uint8_t loginType = tsm::utils::GetLoginType();
                    bool isLocal = (loginType == 0);

                    CenterSeparator("Account Management");

                    if (ButtonCard("Relog Current Account", "Restart the session for the current account", "UiMenuRestart", "RELOG")) {
                        tsm::utils::RelogCurrentAccount();
                    }

                    {
                        bool fast = tsm::utils::GetFastAccountSwitch();
                        if (ToggleCardIcon("Fast Account Switching",
                                          "Skip full restart when switching accounts for faster transitions",
                                          "UiMenuTimer",
                                          &fast)) {
                            tsm::utils::SetFastAccountSwitch(fast);
                        }
                    }

                    if (ButtonCard("Create New Local Account", "Delete current account and generate a fresh local account", "UiMiscPlusMedium", "CREATE")) {
                        s_showCreateLocalConfirm = true;
                    }

                    if (isLocal) {
                        if (ButtonCard("Save Current Account", "Save your current local account to switch between accounts", "UiMenuSaveDisk", "SAVE")) {
                            s_showSaveModal = true;
                            std::snprintf(s_newAccountName, sizeof(s_newAccountName), "Account %d", (int)s_savedAccounts.size() + 1);
                        }
                    } else {
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                        ButtonCard("Save Current Account", "Only available for local accounts", "UiMiscX", "LOCKED");
                        ImGui::PopStyleVar();
                    }

                    if (ButtonCard("Import Local Account", "Import a saved local account from a .bin file", "UiMenuSaveDisk", "IMPORT")) {
                        requestFile("application/octet-stream", OnAccountFileSelected, false);
                    }

                    if (ButtonCard("Export Saved Accounts", "Copy all saved local accounts to the device Downloads folder", "UiMenuBack", "EXPORT")) {
                        std::string errorMsg;
                        if (tsm::utils::ExportAllAccountsToDownloads(&errorMsg)) {
                            ShowToastSuccess("Accounts exported to Downloads");
                        } else {
                            if (!errorMsg.empty()) {
                                ShowToastError(errorMsg.c_str());
                            } else {
                                ShowToastError("Failed to export accounts");
                            }
                        }
                    }

                    CenterSeparator("Switch Account Type");

                    ImVec4 acc2 = tsm::ui::GetAccentColor();
                    const ImU32 bg_sel2 = ImGui::GetColorU32(ImVec4(acc2.x, acc2.y, acc2.z, 0.28f));
                    const ImU32 brd_sel2 = ImGui::GetColorU32(ImVec4(acc2.x, acc2.y, acc2.z, 0.70f));
                    const ImU32 bg_nrm2 = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
                    const ImU32 brd_nrm2 = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));

                    struct AccountTypeInfo {
                        std::uint8_t value;
                        const char* icon;
                    };

                    const AccountTypeInfo accountTypeInfos[] = {
                        { 0, "UiAccountLocalBadge" },
                        { 3, "UiAccountGoogleBadge" },
                        { 4, "UiAccountFacebookBadge" },
                        { 10, "UiAccountSteamBadge" },
                        { 7, "UiAccountNintendoBadge" },
                        { 8, "UiAccountHuaweiBadge" },
                        { 9, "UiAccountSonyBadge" },
                        { 11, "UiAccountTwitchBadge" }
                    };
                    const int typeCount = 8;

                    const float base_d2 = DP(44.0f);
                    const int per_row = 5;
                    const float base_gap2 = ImGui::GetStyle().ItemSpacing.x;

                    float type_avail_w = ImGui::GetContentRegionAvail().x;
                    float s2 = 1.0f;
                    float need_w = typeCount * base_d2 + (typeCount - 1) * base_gap2;

                    float row_w = per_row * base_d2 + (per_row - 1) * base_gap2;
                    if (row_w > type_avail_w && per_row > 0) {
                        s2 = std::max(0.6f, (type_avail_w - (per_row - 1) * base_gap2) / (per_row * base_d2));
                    }
                    float slot_d = base_d2 * s2;
                    float icon_sz2 = slot_d * 0.62f;
                    float gap2 = base_gap2;

                    ImGui::BeginGroup();
                    ImVec2 start2 = ImGui::GetCursorPos();

                    for (int i = 0; i < typeCount; ++i) {
                        int row = i / per_row;
                        int col = i % per_row;

                        int items_in_row = (i / per_row == typeCount / per_row) ? (typeCount % per_row == 0 ? per_row : typeCount % per_row) : per_row;
                        float current_row_w = items_in_row * slot_d + (items_in_row - 1) * gap2;
                        float left_pad = std::max(0.0f, (type_avail_w - current_row_w) * 0.5f);

                        ImGui::SetCursorPos(ImVec2(start2.x + left_pad + col * (slot_d + gap2), start2.y + row * (slot_d + gap2)));
                        ImGui::PushID(i);

                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::InvisibleButton("##typeico", ImVec2(slot_d, slot_d));
                        bool clicked = ImGui::IsItemClicked();
                        bool isCurrent = (loginType == accountTypeInfos[i].value);

                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        float r = slot_d * 0.5f;
                        ImVec2 c = ImVec2(p.x + r, p.y + r);

                        dl->AddCircleFilled(c, r, isCurrent ? bg_sel2 : bg_nrm2, 48);
                        dl->AddCircle(c, r, isCurrent ? brd_sel2 : brd_nrm2, 48, 1.0f);

                        ImVec2 icon_pos = ImVec2(c.x - icon_sz2 * 0.5f, c.y - icon_sz2 * 0.5f);
                        ImGui::SetCursorScreenPos(icon_pos);
                        Icon(accountTypeInfos[i].icon, icon_sz2, ImVec4(1,1,1,1));

                        if (clicked && !isCurrent) {
                            s_pendingAccountType = accountTypeInfos[i].value;
                            s_showSwitchTypeConfirm = true;
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndGroup();

                    CenterSeparator("Saved Accounts");

                    {
                        if (s_savedAccounts.empty()) {
                            PaddedTextDisabled("No saved accounts yet");
                        } else {
                            for (size_t i = 0; i < s_savedAccounts.size(); ++i) {
                                const auto& account = s_savedAccounts[i];

                                ImGui::PushID((int)i);

                                bool isCurrent = tsm::utils::IsCurrentAccount(std::string(account.name));

                                const char* iconLabel = isCurrent ? "UiMiscCheck" : "UiMenuFriends";
                                auto result = EditableCard(account.name, nullptr, iconLabel);

                                if (result.main) {
                                    s_switchAccountIdx = (int)i;
                                    s_showSwitchConfirm = true;
                                }

                                if (result.edit) {
                                    s_editAccountIdx = (int)i;
                                    s_showEditModal = true;
                                    std::strncpy(s_newAccountName, account.name, sizeof(s_newAccountName) - 1);
                                }

                                ImGui::PopID();
                            }
                        }
                    }
                }
                EndPannableChild();

                break; }
            case 3: {
                if (BeginPannableChild("##data_scroll")) {
                    CenterSeparator("Reload Data");
                    if (ButtonCard("Reload Levels", "Reload level definitions from game assets", "UiMiscView", "RELOAD")) {
                        tsm::data::DataManager::Get().ClearCache();
                        (void)tsm::data::DataManager::Get().LoadLevels();
                        ShowToastSuccess("Levels reloaded");
                    }
                    if (ButtonCard("Reload Collectible Defs", "Reload collectible definitions", "UiMiscView", "RELOAD")) {
                        tsm::data::DataManager::Get().ClearCache();
                        (void)tsm::data::DataManager::Get().LoadCollectibleDefs();
                        ShowToastSuccess("Collectible defs reloaded");
                    }
                    if (ButtonCard("Reload Outfit Defs", "Reload outfit definitions", "UiMiscView", "RELOAD")) {
                        tsm::data::DataManager::Get().ClearCache();
                        (void)tsm::data::DataManager::Get().LoadOutfitDefs();
                        ShowToastSuccess("Outfit defs reloaded");
                    }
                    if (ButtonCard("Reload World Quest Defs", "Reload world quest definitions", "UiMiscView", "RELOAD")) {
                        tsm::data::DataManager::Get().ClearCache();
                        (void)tsm::data::DataManager::Get().LoadWorldQuestDefs();
                        ShowToastSuccess("World quest defs reloaded");
                    }
                    if (ButtonCard("Reload Dye Defs", "Reload dye color definitions", "UiMiscView", "RELOAD")) {
                        tsm::data::DataManager::Get().ClearCache();
                        (void)tsm::data::DataManager::Get().LoadDyeColorDefs();
                        ShowToastSuccess("Dye defs reloaded");
                    }
                    if (ButtonCard("Reload Languages", "Re-scan the Translations folder", "UiMenuSwitchLanguage", "RELOAD")) {
                        tsm::ui::i18n::ReloadTranslations();
                        ShowToastSuccess("Languages reloaded");
                    }
                    if (ButtonCard("Reload Configuration", "Re-load the mod_state.json settings", "UiMenuCog", "RELOAD")) {
                        if (tsm::state::ModState::Get().LoadFromFile()) {
                            ShowToastSuccess("Configuration reloaded");
                        } else {
                            ShowToastError("Failed to reload configuration");
                        }
                    }
                    if (ButtonCard("Reload All Data", "Reload all definitions at once", "UiMiscView", "RELOAD")) {
                        tsm::data::DataManager::Get().ClearCache();
                        tsm::progression::CandleRunner::Get().ClearCache();
                        (void)tsm::data::DataManager::Get().LoadAll();
                        tsm::ui::i18n::ReloadTranslations();
                        (void)tsm::state::ModState::Get().LoadFromFile();
                        ShowToastSuccess("All data reloaded");
                    }

                    CenterSeparator("Export / Import Data");
                    if (ButtonCard("Export All Data", "Save everything to Downloads/ThatSkyMod-Export", "UiMenuBack", "EXPORT")) {
                        std::string errorMsg;
                        if (tsm::utils::ExportAllToFolder(&errorMsg)) {
                            ShowToastSuccess("Data exported to Downloads/ThatSkyMod-Export");
                        } else {
                            ShowToastError(errorMsg.empty() ? "Export failed" : errorMsg.c_str());
                        }
                    }

                    if (ButtonCard("Import from Export Folder", "Import from Downloads/ThatSkyMod-Export", "UiMenuSaveDisk", "IMPORT")) {
                        std::string errorMsg;
                        if (tsm::utils::ImportAllFromFolder(tsm::utils::kExportBaseDir, &errorMsg)) {
                            ShowToastSuccess("Data imported successfully");
                            s_shouldReloadAccounts = true;
                        } else {
                            ShowToastError(errorMsg.empty() ? "Import failed" : errorMsg.c_str());
                        }
                    }
                }
                EndPannableChild();
                break; }
            case 4: {
                    using namespace tsm::ui::widgets;
                    if (BeginPannableChild("##games_scroll")) {
                        CenterSeparator("Games");
                        if (ToggleCardIcon("Snake", "Play the Snake minigame", "UiMenuSnakeGame0", &s_snakeGameEnabled)) {
                            s_flappybirdGameEnabled = false;
                            s_pongGameEnabled = false;
                            s_breakoutGameEnabled = false;
                            s_memoryMatchGameEnabled = false;
                            s_doomGameEnabled = false;
                            tsm::ui::overlays::SetPongGameEnabled(s_pongGameEnabled);
                            tsm::ui::overlays::SetFlappyBirdGameEnabled(s_flappybirdGameEnabled);
                            tsm::ui::overlays::SetBreakoutGameEnabled(s_breakoutGameEnabled);
                            tsm::ui::overlays::SetMemoryMatchGameEnabled(s_memoryMatchGameEnabled);
                            tsm::ui::overlays::SetDoomGameEnabled(s_doomGameEnabled);
                            tsm::ui::overlays::SetSnakeGameEnabled(s_snakeGameEnabled);
                        }

                        if (ToggleCardIcon("Flappy Bird", "Play the Flappy Bird minigame", "UiPersonalityButterfly", &s_flappybirdGameEnabled)) {
                            s_snakeGameEnabled = false;
                            s_pongGameEnabled = false;
                            s_breakoutGameEnabled = false;
                            s_memoryMatchGameEnabled = false;
                            s_doomGameEnabled = false;
                            tsm::ui::overlays::SetPongGameEnabled(s_pongGameEnabled);
                            tsm::ui::overlays::SetSnakeGameEnabled(s_snakeGameEnabled);
                            tsm::ui::overlays::SetBreakoutGameEnabled(s_breakoutGameEnabled);
                            tsm::ui::overlays::SetMemoryMatchGameEnabled(s_memoryMatchGameEnabled);
                            tsm::ui::overlays::SetDoomGameEnabled(s_doomGameEnabled);
                            tsm::ui::overlays::SetFlappyBirdGameEnabled(s_flappybirdGameEnabled);
                        }

                        if (ToggleCardIcon("Pong", "Play the Pong minigame", "UiMenuBallGame", &s_pongGameEnabled)) {
                            s_snakeGameEnabled = false;
                            s_flappybirdGameEnabled = false;
                            s_breakoutGameEnabled = false;
                            s_memoryMatchGameEnabled = false;
                            s_doomGameEnabled = false;
                            tsm::ui::overlays::SetSnakeGameEnabled(s_snakeGameEnabled);
                            tsm::ui::overlays::SetFlappyBirdGameEnabled(s_flappybirdGameEnabled);
                            tsm::ui::overlays::SetBreakoutGameEnabled(s_breakoutGameEnabled);
                            tsm::ui::overlays::SetMemoryMatchGameEnabled(s_memoryMatchGameEnabled);
                            tsm::ui::overlays::SetDoomGameEnabled(s_doomGameEnabled);
                            tsm::ui::overlays::SetPongGameEnabled(s_pongGameEnabled);
                        }

                        if (ToggleCardIcon("Breakout", "Play the Breakout minigame", "UiOutfitPropBrick", &s_breakoutGameEnabled)) {
                            s_snakeGameEnabled = false;
                            s_flappybirdGameEnabled = false;
                            s_pongGameEnabled = false;
                            s_memoryMatchGameEnabled = false;
                            s_doomGameEnabled = false;
                            tsm::ui::overlays::SetSnakeGameEnabled(s_snakeGameEnabled);
                            tsm::ui::overlays::SetFlappyBirdGameEnabled(s_flappybirdGameEnabled);
                            tsm::ui::overlays::SetPongGameEnabled(s_pongGameEnabled);
                            tsm::ui::overlays::SetMemoryMatchGameEnabled(s_memoryMatchGameEnabled);
                            tsm::ui::overlays::SetDoomGameEnabled(s_doomGameEnabled);
                            tsm::ui::overlays::SetBreakoutGameEnabled(s_breakoutGameEnabled);
                        }

                        if (ToggleCardIcon("Memory Match", "Play the Memory Match minigame", "UiMenuBuffSharedMemory", &s_memoryMatchGameEnabled)) {
                            s_snakeGameEnabled = false;
                            s_flappybirdGameEnabled = false;
                            s_pongGameEnabled = false;
                            s_breakoutGameEnabled = false;
                            s_doomGameEnabled = false;
                            tsm::ui::overlays::SetSnakeGameEnabled(s_snakeGameEnabled);
                            tsm::ui::overlays::SetFlappyBirdGameEnabled(s_flappybirdGameEnabled);
                            tsm::ui::overlays::SetPongGameEnabled(s_pongGameEnabled);
                            tsm::ui::overlays::SetBreakoutGameEnabled(s_breakoutGameEnabled);
                            tsm::ui::overlays::SetDoomGameEnabled(s_doomGameEnabled);
                            tsm::ui::overlays::SetMemoryMatchGameEnabled(s_memoryMatchGameEnabled);
                        }

                        if (ToggleCardIcon("Doom", "Play the Doom minigame", "UiMiscTarget", &s_doomGameEnabled)) {
                            s_snakeGameEnabled = false;
                            s_flappybirdGameEnabled = false;
                            s_pongGameEnabled = false;
                            s_breakoutGameEnabled = false;
                            s_memoryMatchGameEnabled = false;
                            tsm::ui::overlays::SetSnakeGameEnabled(s_snakeGameEnabled);
                            tsm::ui::overlays::SetFlappyBirdGameEnabled(s_flappybirdGameEnabled);
                            tsm::ui::overlays::SetPongGameEnabled(s_pongGameEnabled);
                            tsm::ui::overlays::SetBreakoutGameEnabled(s_breakoutGameEnabled);
                            tsm::ui::overlays::SetMemoryMatchGameEnabled(s_memoryMatchGameEnabled);
                            tsm::ui::overlays::SetDoomGameEnabled(s_doomGameEnabled);
                        }
                    }
                    EndPannableChild();
                break; }
            case 5: {
                CenterSeparator("Credits");
                CenterSeparator("Developer");
                CenteredText("XeTrinityz");
                CenterSeparator("Contributors");
                CenteredText("Mr Gatto");
                CenteredText("Vexadros & Gxost");
                CenteredText("Kiojeen");
                CenteredText("TheSR");
                CenterSeparator("Links");
                if (ButtonCard("Join our Discord", "Get support, updates, and chat with the community", "social_discord", "OPEN")) {
                    tsm::lua::helpers::OpenURL("https://discord.gg/kjpGzTU9hH");
                }
                if (ButtonCard("GitHub Repository", "Official releases", "GitHub", "OPEN")) {
                    tsm::lua::helpers::OpenURL("https://github.com/XeTrinityz/ThatSkyMod-Android");
                }

                break; }
        }
        EndCard();
    }

    if (s_showSaveModal) {
        ImGui::OpenPopup("##SaveAccountModal");
        s_showSaveModal = false;
    }

    if (InputPopup("##SaveAccountModal", "Save Account", "Enter account name",
                  s_newAccountName, sizeof(s_newAccountName), "Save", "Cancel")) {
        if (s_newAccountName[0] != '\0') {
            std::string errorMsg;
            if (tsm::utils::SaveCurrentAccount(std::string(s_newAccountName), &errorMsg)) {
                tsm::utils::SavedAccount account;
                std::strncpy(account.name, s_newAccountName, sizeof(account.name) - 1);
                account.name[sizeof(account.name) - 1] = '\0';
                s_savedAccounts.push_back(account);
                tsm::ui::helpers::ShowToastSuccess("Account saved");
            } else {
                if (!errorMsg.empty()) {
                    tsm::ui::helpers::ShowToastError(errorMsg.c_str());
                } else {
                    tsm::ui::helpers::ShowToastError("Failed to save account");
                }
            }
        }
    }

    if (s_showCreateLocalConfirm) {
        ImGui::OpenPopup("##CreateLocalConfirm");
        s_showCreateLocalConfirm = false;
    }

    if (ConfirmPopup("##CreateLocalConfirm", "Create New Local Account",
                    "This will delete your current account data and create a fresh local account.\n\nThis action cannot be undone!\n\nAre you sure?",
                    "Create", "Cancel")) {
        tsm::utils::CreateNewLocalAccount();
        tsm::ui::helpers::ShowToastSuccess("Creating new local account...");
    }

    if (s_showSwitchTypeConfirm) {
        ImGui::OpenPopup("##SwitchTypeConfirm");
        s_showSwitchTypeConfirm = false;
    }

    if (s_pendingAccountType >= 0) {
        const char* typeName = tsm::utils::GetAccountTypeName(s_pendingAccountType);
        char confirmMsg[256];
        const char* fmt = tsm::ui::i18n::Tr("Switch to %s login?\n\nThis will trigger a relog and prompt you to sign in.");
        std::snprintf(confirmMsg, sizeof(confirmMsg),
                     fmt,
                     typeName);

        if (ConfirmPopup("##SwitchTypeConfirm", "Switch Account Type", confirmMsg, "Switch", "Cancel")) {
            tsm::utils::SwitchAccountType(s_pendingAccountType);
            tsm::ui::helpers::ShowToastSuccess((std::string("Switching to ") + typeName + "...").c_str());
            s_pendingAccountType = -1;
        } else if (ImGui::IsPopupOpen("##SwitchTypeConfirm") == false) {
            s_pendingAccountType = -1;
        }
    }

    if (s_showSwitchConfirm) {
        ImGui::OpenPopup("##SwitchAccountConfirm");
        s_showSwitchConfirm = false;
    }

    if (s_switchAccountIdx >= 0 && s_switchAccountIdx < (int)s_savedAccounts.size()) {
        const auto& switchAccount = s_savedAccounts[s_switchAccountIdx];
        char confirmMsg[256];
        const char* fmt = tsm::ui::i18n::Tr("Switch to account '%s'?\n\nThis will trigger a relog.");
        std::snprintf(confirmMsg, sizeof(confirmMsg),
                     fmt,
                     switchAccount.name);

        if (ConfirmPopup("##SwitchAccountConfirm", "Switch Account", confirmMsg, "Switch", "Cancel")) {
            if (tsm::utils::SwitchToLocalAccount(std::string(switchAccount.name))) {
                tsm::ui::helpers::ShowToastSuccess("Switching account...");
            } else {
                tsm::ui::helpers::ShowToastError("Failed to switch account");
            }
            s_switchAccountIdx = -1;
        } else if (ImGui::IsPopupOpen("##SwitchAccountConfirm") == false) {
            s_switchAccountIdx = -1;
        }
    }

    if (s_showEditModal) {
        ImGui::OpenPopup("##EditAccountModal");
        s_showEditModal = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
    if (ImGui::BeginPopupModal("##EditAccountModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        if (s_editAccountIdx >= 0 && s_editAccountIdx < (int)s_savedAccounts.size()) {
            const char* title = tsm::ui::i18n::Tr("Edit Account");
            float title_w = ImGui::CalcTextSize(title).x;
            float window_w = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
            ImGui::Text("%s", title);
            ImGui::Separator();

            ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
            ImGui::PushItemWidth(-1);
            InputWithPlaceholder("##editname", s_newAccountName, sizeof(s_newAccountName), "Account name");
            ImGui::PopItemWidth();

            ImGui::Dummy(ImVec2(0, DP(8.0f)));

            float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;

            if (AccentButton("Save", ImVec2(btn_w, 0))) {
                std::string oldName = std::string(s_savedAccounts[s_editAccountIdx].name);
                std::string newName = std::string(s_newAccountName);

                if (oldName != newName) {
                    std::string accountsDir = tsm::utils::GetAccountsDir();
                    std::string oldPath = accountsDir + "/" + oldName + ".bin";
                    std::string newPath = accountsDir + "/" + newName + ".bin";

                    if (rename(oldPath.c_str(), newPath.c_str()) == 0) {
                        std::strncpy(s_savedAccounts[s_editAccountIdx].name, s_newAccountName,
                                   sizeof(s_savedAccounts[s_editAccountIdx].name) - 1);
                        tsm::ui::helpers::ShowToastSuccess("Account renamed");
                    } else {
                        tsm::ui::helpers::ShowToastError("Failed to rename account");
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::Dummy(ImVec2(0, DP(4.0f)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, DP(4.0f)));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
            if (ImGui::Button(tsm::ui::i18n::Tr("Delete Account"), ImVec2(-1, 0))) {
                std::string accountName = std::string(s_savedAccounts[s_editAccountIdx].name);
                if (tsm::utils::DeleteAccount(accountName)) {
                    s_savedAccounts.erase(s_savedAccounts.begin() + s_editAccountIdx);
                    tsm::ui::helpers::ShowToastSuccess("Account deleted");
                } else {
                    tsm::ui::helpers::ShowToastError("Failed to delete account");
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

}}}
