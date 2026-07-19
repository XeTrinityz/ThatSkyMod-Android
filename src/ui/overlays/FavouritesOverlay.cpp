#include <ui/overlays/FavouritesOverlay.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Controls.h>

#include <ui/core/metrics.h>
#include <ui/core/App.h>

#include <ui/core/Localization.h>
#include <ui/helpers/Toast.h>
#include <state/ModState.h>
#include <features/manager/FeatureManager.h>
#include <progression/AutoWaxTools.h>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <game/memory/api.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>

#include <imgui/imgui.h>

#include <array>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace tsm { namespace ui { namespace overlays {

using tsm::features::FeatureId;

namespace Config {
    constexpr float ANIMATION_SPEED = 8.0f;
    constexpr float GLOW_PULSE_SPEED = 2.5f;
    constexpr float SLOT_SCALE_ACTIVE = 0.95f;
    constexpr int RADIAL_SEGMENTS = 64;
}

namespace Easing {
    inline float EaseOutQuart(float t) {
        return 1.0f - std::pow(1.0f - t, 4.0f);
    }

    inline float EaseInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    inline float EaseOutElastic(float t) {
        const float c4 = (2.0f * 3.14159265359f) / 3.0f;
        return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f :
               std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }
}

namespace Colors {
    const ImVec4 PRIMARY_DARK = ImVec4(0.12f, 0.14f, 0.18f, 0.95f);
    const ImVec4 PRIMARY_MEDIUM = ImVec4(0.18f, 0.20f, 0.25f, 0.92f);
    const ImVec4 PRIMARY_LIGHT = ImVec4(0.25f, 0.28f, 0.35f, 0.90f);

    const ImVec4 ACCENT_BLUE = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
    const ImVec4 ACCENT_CYAN = ImVec4(0.2f, 0.8f, 0.9f, 1.0f);
    const ImVec4 ACCENT_GREEN = ImVec4(0.3f, 0.9f, 0.4f, 1.0f);
    const ImVec4 ACCENT_ORANGE = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
    const ImVec4 ACCENT_GOLD = ImVec4(1.0f, 0.84f, 0.3f, 1.0f);
    const ImVec4 ACCENT_PURPLE = ImVec4(0.7f, 0.4f, 1.0f, 1.0f);
    const ImVec4 ACCENT_RED = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

    const ImVec4 STATE_ACTIVE = ImVec4(0.3f, 0.9f, 0.4f, 1.0f);
    const ImVec4 STATE_INACTIVE = ImVec4(0.5f, 0.5f, 0.5f, 0.6f);

    const ImVec4 GLOW_STRONG = ImVec4(0.4f, 0.7f, 1.0f, 0.8f);
    const ImVec4 GLOW_MEDIUM = ImVec4(0.4f, 0.7f, 1.0f, 0.4f);
    const ImVec4 GLOW_WEAK = ImVec4(0.4f, 0.7f, 1.0f, 0.15f);
}

enum class FavouriteKind : int {
    None = 0,
    FeatureToggle = 1,
    Action = 2,
};

enum class FavouriteAction : int {
    ReloadLevel = 0,
    Home,
    Geyser,
    Grandma,
    SocialGift,
    ResumeCheckpoint,
    DisconnectSession,
};

struct FavouriteSlot {
    FavouriteKind kind = FavouriteKind::None;
    FeatureId featureId = FeatureId::Invincibility;
    FavouriteAction action = FavouriteAction::ReloadLevel;
    float activeAnim = 0.0f;
    float glowPhase = 0.0f;
};

struct FeatureMeta {
    FeatureId id;
    const char* name;
    const char* description;
    const char* icon;
    ImVec4 color;
};

struct ActionMeta {
    FavouriteAction id;
    const char* name;
    const char* description;
    const char* icon;
};

static bool s_showFavourites = false;
static bool s_expanded = false;
static bool s_editMode = false;
static bool s_slotsLoaded = false;
static std::array<FavouriteSlot, 10> s_slots{};

static float s_expandAnim = 0.0f;
static float s_centerActiveAnim = 0.0f;
static float s_editModeAnim = 0.0f;
static float s_globalGlowPhase = 0.0f;

static int s_selectedSlotForAssignment = -1;
static std::string s_searchFilter;

static ImVec2 s_menuPosition = ImVec2(0, 0);
static bool s_isDragging = false;
static ImVec2 s_dragStartPos = ImVec2(0, 0);
static ImVec2 s_dragStartMenuPos = ImVec2(0, 0);
static bool s_positionInitialized = false;
static constexpr float DRAG_THRESHOLD = 10.0f;

static const FeatureMeta kFeatureMetas[] = {
    { FeatureId::Invincibility,              "Invincibility",                     "Prevents all damage",                                  "UiMenuShield",                   Colors::ACCENT_BLUE },
    { FeatureId::AntiRainDrain,              "Anti Rain Drain",                   "Blocks rain damage",                                   "UiMenuStageFallingRain",         Colors::ACCENT_CYAN },
    { FeatureId::AntiKrill,                  "Anti Krill",                        "Avoids Krill detection",                               "UiMenuBuffKrillRepellent",       Colors::ACCENT_PURPLE },
    { FeatureId::AntiAFK,                    "Anti AFK",                          "Prevents idle timeout",                                "UiMenuTimer",                    Colors::ACCENT_GREEN },
    { FeatureId::AutoCharge,                 "Infinite Energy",                   "Automatically refills energy",                         "UiMenuBattery03",                Colors::ACCENT_GREEN },
    { FeatureId::SuperRun,                   "Super Run",                         "Increases run speed",                                  "UiHudChevron",                   Colors::ACCENT_GOLD },
    { FeatureId::SuperSlidey,                "Super Slidey",                      "Extremely slippery surfaces",                          "UiMenuBlur",                     Colors::ACCENT_CYAN },
    { FeatureId::SuperFlight,                "Super Flight",                      "Increased flight power",                               "UiSocialTeleport",               Colors::ACCENT_BLUE },
    { FeatureId::SuperLaunch,                "Super Launch",                      "Increased launch power",                               "UiMenuBuffAntiGravity",          Colors::ACCENT_PURPLE },
    { FeatureId::FastFlap,                   "Fast Flap",                         "Flap your wings faster",                               "UiMenuBuffFastCharge",           Colors::ACCENT_CYAN },
    { FeatureId::UnlimitedFireworks,         "Unlimited Fireworks",               "Removes fireworks cooldown",                           "UiMenuEventCurrencyFireworks",   Colors::ACCENT_RED },
    { FeatureId::UnlockAll,                  "Unlock All",                        "Temporarily bypass all \"Has Unlock\" checks",       "UiMenuLock",                     Colors::ACCENT_GOLD },
    { FeatureId::UnlockEmoteLevels,          "Unlock All Emote Levels",           "Allow using all emote level variants",                 "UiEmoteStanceHero",              Colors::ACCENT_CYAN },
    { FeatureId::UnlockRelationshipAbilities,"Unlock All Relationship Abilities", "Enable all relationship actions",                      "UiEmoteSocialHeart",             Colors::ACCENT_GREEN },
    { FeatureId::AutoCollectWax,             "Auto Collect Wax",                  "Automatically collect wax nearby",                     "UiMenuBuffCandle",               Colors::ACCENT_GOLD },
    { FeatureId::AutoBurnCandles,            "Auto Burn Candles",                 "Continuously burn candles in range",                   "UiMiscFlame",                    Colors::ACCENT_RED },
    { FeatureId::AutoBurnPlants,             "Auto Burn Plants",                  "Continuously burn plants in range",                    "UiOutfitHornAP15DarkPlant",      Colors::ACCENT_GREEN },
    { FeatureId::FastHome,                   "Fast Home",                         "Removes home delay",                                   "UiMenuConstellationHome",        Colors::ACCENT_GOLD },
    { FeatureId::RevealAllPlayers,           "Reveal All Players",                "Make all players visible (client-side)",               "UiMiscView",                     Colors::ACCENT_CYAN },
    { FeatureId::LightAllPlayers,            "Light All Players",                 "Make all players visible",                             "UiMiscView",                     Colors::ACCENT_GREEN },
    { FeatureId::ShowModdedOutfits,          "Show Modded Outfits",               "See outfits players don't own",                        "UiMiscView",                     Colors::ACCENT_PURPLE },
    { FeatureId::EspPlayers,                 "Player ESP",                        "Show ESP on all players in session",                  "UiMenuFriends",                  Colors::ACCENT_BLUE },
    { FeatureId::EspNPCs,                    "NPC ESP",                           "Show ESP on all NPCs in session",                     "UiEmoteAP16ArmWave0",            Colors::ACCENT_CYAN },
    { FeatureId::EspWingLights,              "Wing Light ESP",                    "Show ESP on wing lights/collectibles",                "UiMenuWingBuff",                 Colors::ACCENT_GREEN },
    { FeatureId::EspDyes,                    "Dye ESP",                           "Show ESP on color dyes",                              "UiMenuDye",                      Colors::ACCENT_PURPLE },
    { FeatureId::EspCandles,                 "Candle ESP",                        "Show ESP on map candles",                             "UiMenuBuffCandle",               Colors::ACCENT_GOLD },
};

static const ActionMeta kActionMetas[] = {
    { FavouriteAction::ReloadLevel,      "Reload Level",       "Reload the current level and return to your position", "UiMenuQuestHint" },
    { FavouriteAction::Home,             "Home",               "Return to Home",                                      "UiMenuConstellationHome" },
    { FavouriteAction::Geyser,           "Geyser",             "Teleport to Sanctuary Geyser",                        "UiMenuMiscGeyser" },
    { FavouriteAction::Grandma,          "Grandma",            "Teleport to Grandma",                                "UiEmoteAP01Think" },
    { FavouriteAction::SocialGift,       "Social Gift",        "Show white candle",                                   "UiSocialOfferCandle2" },
    { FavouriteAction::ResumeCheckpoint, "Resume Checkpoint",  "Resume the latest checkpoint",                        "UiMenuCheckpoint" },
    { FavouriteAction::DisconnectSession,"Disconnect Session",  "Disconnect from the current session",                 "UiMenuSignal" },
};

static const FeatureMeta* FindMeta(FeatureId id) {
    for (const auto& m : kFeatureMetas) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

static const ActionMeta* FindActionMeta(FavouriteAction id) {
    for (const auto& m : kActionMetas) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

static void LoadSlotsFromPrefs() {
    if (s_slotsLoaded) return;
    s_slotsLoaded = true;

    for (auto& slot : s_slots) {
        slot.kind = FavouriteKind::None;
        slot.featureId = FeatureId::Invincibility;
        slot.action = FavouriteAction::ReloadLevel;
        slot.glowPhase = (float)(rand() % 1000) / 1000.0f * 6.28318f;
    }

    auto& ms = tsm::state::ModState::Get();
    const std::string& encoded = ms.ui.favouritesRadialSlots;
    if (encoded.empty()) return;

    std::stringstream ss(encoded);
    std::string token;
    std::size_t index = 0;
    while (std::getline(ss, token, ';') && index < s_slots.size()) {
        if (!token.empty()) {
            int kindInt = 0;
            int vInt = 0;
            char comma = 0;
            std::stringstream ts(token);
            if (ts >> kindInt >> comma >> vInt && comma == ',') {
                if (kindInt == (int)FavouriteKind::FeatureToggle &&
                    vInt >= 0 && vInt <= (int)FeatureId::EspCandles) {

                    s_slots[index].kind = FavouriteKind::FeatureToggle;
                    s_slots[index].featureId = static_cast<FeatureId>(vInt);
                } else if (kindInt == (int)FavouriteKind::Action &&
                    vInt >= 0 && vInt <= (int)FavouriteAction::DisconnectSession) {
                    s_slots[index].kind = FavouriteKind::Action;
                    s_slots[index].action = static_cast<FavouriteAction>(vInt);
                }
            }
        }
        ++index;
    }
}

static void SaveSlotsToPrefs() {
    auto& ms = tsm::state::ModState::Get();

    std::string encoded;
    encoded.reserve(64);

    for (std::size_t i = 0; i < s_slots.size(); ++i) {
        if (i > 0) encoded.push_back(';');
        const auto& slot = s_slots[i];

        int kindInt = 0;
        int valueInt = 0;
        switch (slot.kind) {
            case FavouriteKind::FeatureToggle:
                kindInt = (int)FavouriteKind::FeatureToggle;
                valueInt = (int)slot.featureId;
                break;
            case FavouriteKind::Action:
                kindInt = (int)FavouriteKind::Action;
                valueInt = (int)slot.action;
                break;
            default:
                kindInt = 0;
                valueInt = 0;
                break;
        }
        encoded += std::to_string(kindInt);
        encoded.push_back(',');
        encoded += std::to_string(valueInt);
    }

    ms.ui.favouritesRadialSlots = encoded;
    bool ok = ms.SaveToFile();
    if (ok) {
        tsm::ui::helpers::ShowToastSuccess("Favorites Saved");
    } else {
        tsm::ui::helpers::ShowToastError("Failed to save favorites");
    }
}

static bool GetFeatureState(FeatureId id, bool& outState) {
    auto& ms = tsm::state::ModState::Get();
    auto& pg = ms.playerGeneral;
    auto& unlocks = ms.unlocks;

    switch (id) {
        case FeatureId::Invincibility:               outState = pg.invincibility;               return true;
        case FeatureId::AntiRainDrain:               outState = pg.antiRainDrain;               return true;
        case FeatureId::AntiKrill:                   outState = pg.antiKrill;                   return true;
        case FeatureId::AntiAFK:                     outState = pg.antiAFK;                     return true;
        case FeatureId::AutoCharge:                  outState = pg.autoCharge;                  return true;
        case FeatureId::SuperRun:                    outState = pg.superRun;                    return true;
        case FeatureId::SuperSlidey:                 outState = pg.superSlidey;                 return true;
        case FeatureId::SuperFlight:                 outState = pg.superFlight;                 return true;
        case FeatureId::SuperLaunch:                 outState = pg.superLaunch;                 return true;
        case FeatureId::FastFlap:                    outState = pg.fastFlap;                    return true;
        case FeatureId::UnlimitedFireworks:          outState = pg.unlimitedFireworks;          return true;
        case FeatureId::UnlockAll:                   outState = unlocks.unlockAll;              return true;
        case FeatureId::UnlockEmoteLevels:           outState = unlocks.unlockEmoteLevels;      return true;
        case FeatureId::UnlockRelationshipAbilities: outState = unlocks.unlockRelationshipAbilities; return true;
        case FeatureId::AutoCollectWax:              outState = tsm::progression::IsAutoCollectWaxEnabled(); return true;
        case FeatureId::AutoBurnCandles:             outState = tsm::progression::IsAutoBurnCandlesEnabled(); return true;
        case FeatureId::AutoBurnPlants:              outState = tsm::progression::IsAutoBurnPlantsEnabled(); return true;
        case FeatureId::FastHome:                    outState = ms.world.fastHome;              return true;
        case FeatureId::ShowModdedOutfits:           outState = ms.world.showModdedOutfits;     return true;
        case FeatureId::EspPlayers:                  outState = ms.debug.espShowPlayers;        return true;
        case FeatureId::EspNPCs:                     outState = ms.debug.espShowNPCs;           return true;
        case FeatureId::EspWingLights:               outState = ms.debug.espShowWingLights;     return true;
        case FeatureId::EspDyes:                     outState = ms.debug.espShowDyes;           return true;
        case FeatureId::EspCandles:                  outState = ms.debug.espShowCandles;        return true;
        case FeatureId::LightAllPlayers: {
            std::uint8_t b = tsm::game::memory::ReadByte(tsm::game::Offsets::kAvatarCharcoaling);
            outState = (b == 0);
            return true;
        }
        case FeatureId::RevealAllPlayers: {
            std::uint32_t v = tsm::game::memory::ReadU32(tsm::game::Offsets::kRevealPlayers);
            outState = (v == 0xD503201Fu);
            return true;
        }
    }
    return false;
}

static void ToggleFeature(FeatureId id) {
    bool state = false;
    if (!GetFeatureState(id, state)) return;
    bool newState = !state;

    auto& ms = tsm::state::ModState::Get();

    switch (id) {
        case FeatureId::UnlockAll:
            ms.unlocks.unlockAll = newState;
            tsm::features::FeatureManager::Get().ApplyUnlockFeatures();
            break;
        case FeatureId::UnlockEmoteLevels:
            ms.unlocks.unlockEmoteLevels = newState;
            tsm::features::FeatureManager::Get().ApplyUnlockFeatures();
            break;
        case FeatureId::UnlockRelationshipAbilities:
            ms.unlocks.unlockRelationshipAbilities = newState;
            tsm::features::FeatureManager::Get().ApplyUnlockFeatures();
            break;
        default:
            tsm::features::FeatureManager::Get().ApplyFeature(id, newState);
            break;
    }

    if (const FeatureMeta* meta = FindMeta(id)) {
        char msg[256];

        const char* featureName = tsm::ui::i18n::Tr(meta->name);
        const char* state = tsm::ui::i18n::Tr(
            newState ? "Enabled" : "Disabled"
        );

        const char* fmt = tsm::ui::i18n::Tr("%1$s: %2$s");

        std::snprintf(msg, sizeof(msg), fmt, featureName, state);

        if (newState) {
            tsm::ui::helpers::ShowToastSuccess(msg);
        }
        else {
            tsm::ui::helpers::ShowToastError(msg);
        }
    }

}

static void ExecuteAction(FavouriteAction action) {
    switch (action) {
        case FavouriteAction::ReloadLevel: {
            const char* currentLevel = tsm::game::api::LevelName();
            if (currentLevel && *currentLevel) {
                tsm::lua::helpers::ChangeLevel(currentLevel, true);
                tsm::ui::helpers::ShowToastInfo("Reloading level...");
            } else {
                tsm::ui::helpers::ShowToastError("Cannot reload: No level loaded");
            }
            break;
        }
        case FavouriteAction::Home:
            tsm::lua::queue::Enqueue("TTitleGate()");
            tsm::ui::helpers::ShowToastInfo("Loading Home...");
            break;
        case FavouriteAction::Geyser:
            tsm::lua::helpers::ChangeLevel("Prairie_Island", true);
            tsm::ui::helpers::ShowToastInfo("Loading Prairie_Island...");
            break;
        case FavouriteAction::Grandma:
            tsm::lua::helpers::ChangeLevel("RainShelter", true);
            tsm::ui::helpers::ShowToastInfo("Loading RainShelter...");
            break;
        case FavouriteAction::SocialGift:
            tsm::lua::helpers::SocialGift();
            tsm::ui::helpers::ShowToastSuccess("Executed: Social Gift");
            break;
        case FavouriteAction::ResumeCheckpoint:
            tsm::lua::queue::Enqueue("CheckpointResumeLatest(game)");
            tsm::ui::helpers::ShowToastInfo("Resuming Checkpoint...");
            break;
        case FavouriteAction::DisconnectSession:
            tsm::lua::queue::Enqueue("Disconnect(game)");
            tsm::ui::helpers::ShowToastSuccess("Executed: Disconnect Session");
            break;
    }
}

static void DrawGlowCircle(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float intensity) {
    const int segments = Config::RADIAL_SEGMENTS;
    const int glowLayers = 3;

    for (int layer = 0; layer < glowLayers; ++layer) {
        float layerT = (float)(layer + 1) / (float)glowLayers;
        float layerRadius = radius * (1.0f + layerT * 0.4f);
        float layerAlpha = intensity * (1.0f - layerT * layerT);

        ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(color);
        colorVec.w *= layerAlpha;
        dl->AddCircle(center, layerRadius, ImGui::GetColorU32(colorVec), segments, 2.0f);
    }
}

static void DrawRadialGradientCircle(ImDrawList* dl, ImVec2 center, float radius,
                                     ImU32 innerColor, ImU32 outerColor) {
    const int segments = Config::RADIAL_SEGMENTS;
    const int rings = 8;

    for (int i = 0; i < rings; ++i) {
        float t = (float)i / (float)rings;
        float nextT = (float)(i + 1) / (float)rings;

        float r1 = radius * t;
        float r2 = radius * nextT;

        ImVec4 c1Vec = ImGui::ColorConvertU32ToFloat4(innerColor);
        ImVec4 c2Vec = ImGui::ColorConvertU32ToFloat4(outerColor);
        ImVec4 blendColor = ImVec4(
            c1Vec.x * (1.0f - t) + c2Vec.x * t,
            c1Vec.y * (1.0f - t) + c2Vec.y * t,
            c1Vec.z * (1.0f - t) + c2Vec.z * t,
            c1Vec.w * (1.0f - t) + c2Vec.w * t
        );

        dl->AddCircleFilled(center, r2, ImGui::GetColorU32(blendColor), segments);
    }
}

static void DrawConnectingLine(ImDrawList* dl, ImVec2 from, ImVec2 to,
                               ImU32 color, float thickness, float alpha) {
    ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(color);
    colorVec.w *= alpha;

    dl->AddLine(from, to, ImGui::GetColorU32(colorVec), thickness);
}

bool IsfavoritesVisible() {
    return s_showFavourites;
}

void SetfavoritesVisible(bool visible) {
    s_showFavourites = visible;
    if (!visible) {
        s_expanded = false;
        s_editMode = false;
    }
}

void RenderFavourites() {
    if (!s_showFavourites) return;

    LoadSlotsFromPrefs();

    ImGuiIO& io = ImGui::GetIO();
    float deltaTime = io.DeltaTime;
    ImVec2 displaySize = io.DisplaySize;

    if (!s_positionInitialized) {
        const float centerMargin = DP(72.0f);
        s_menuPosition = ImVec2(displaySize.x - centerMargin, displaySize.y * 0.5f);
        s_positionInitialized = true;
    }

    s_globalGlowPhase += deltaTime * Config::GLOW_PULSE_SPEED;
    if (s_globalGlowPhase > 6.28318f) s_globalGlowPhase -= 6.28318f;

    float targetExpand = s_expanded ? 1.0f : 0.0f;
    s_expandAnim += (targetExpand - s_expandAnim) * Config::ANIMATION_SPEED * deltaTime;
    s_expandAnim = std::clamp(s_expandAnim, 0.0f, 1.0f);
    float expandT = Easing::EaseInOutCubic(s_expandAnim);

    float targetEdit = s_editMode ? 1.0f : 0.0f;
    s_editModeAnim += (targetEdit - s_editModeAnim) * Config::ANIMATION_SPEED * deltaTime;
    s_editModeAnim = std::clamp(s_editModeAnim, 0.0f, 1.0f);

    const float centerBtnSize = DP(64.0f);
    const float slotSize = DP(52.0f);
    const float radius = DP(110.0f);

    ImVec2 center = s_menuPosition;

    float collapsedSize = centerBtnSize * 2.2f;
    float contentExtent = radius + slotSize * 0.8f;
    float expandedSize = contentExtent * 2.0f;
    float windowSide = collapsedSize + (expandedSize - collapsedSize) * expandT;

    ImVec2 windowSize = ImVec2(windowSide, windowSide);
    ImVec2 windowPos = ImVec2(center.x - windowSide * 0.5f, center.y - windowSide * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    if (!ImGui::Begin("FavouritesRadialInteractive", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 windowPosActual = ImGui::GetWindowPos();
    ImVec2 localCenter = ImVec2(windowSize.x * 0.5f, windowSize.y * 0.5f);
    ImVec2 centerScreen = ImVec2(windowPosActual.x + localCenter.x, windowPosActual.y + localCenter.y);

    const int slotCount = (int)s_slots.size();
    const float pi = 3.14159265359f;
    const float twoPi = pi * 2.0f;
    const float startAngle = -pi * 0.5f;

    bool anySlotInteracted = false;
    bool openAssignPopup = false;
    bool clickConsumedByPopup = false;

    bool slotsInteractive = s_expanded && (expandT > 0.45f);

    if (s_expandAnim > 0.01f) {
        float animRadius = radius * expandT;

        for (int i = 0; i < slotCount; ++i) {
            FavouriteSlot& slot = s_slots[i];

            float t = (float)i / (float)slotCount;
            float angle = startAngle + twoPi * t;

            float sx = centerScreen.x + std::cos(angle) * animRadius;
            float sy = centerScreen.y + std::sin(angle) * animRadius;

            ImVec2 slotCenter = ImVec2(sx, sy);
            ImVec2 slotTopLeft = ImVec2(sx - slotSize * 0.5f, sy - slotSize * 0.5f);

            ImGui::PushID(i);
            bool clicked = false;
            bool rightClicked = false;
            bool active = false;

            if (slotsInteractive) {
                ImGui::SetCursorScreenPos(slotTopLeft);
                ImGui::InvisibleButton("##fav_slot", ImVec2(slotSize, slotSize));
                clicked = ImGui::IsItemClicked(0);
                rightClicked = ImGui::IsItemClicked(1);
                active = ImGui::IsItemActive();

                if (active || clicked || rightClicked) {
                    anySlotInteracted = true;
                }
            }

            float targetActive = active ? 1.0f : 0.0f;
            slot.activeAnim += (targetActive - slot.activeAnim) * (Config::ANIMATION_SPEED * 1.5f) * deltaTime;
            slot.activeAnim = std::clamp(slot.activeAnim, 0.0f, 1.0f);

            slot.glowPhase += deltaTime * Config::GLOW_PULSE_SPEED;
            if (slot.glowPhase > twoPi) slot.glowPhase -= twoPi;

            float scale = 1.0f - slot.activeAnim * (1.0f - Config::SLOT_SCALE_ACTIVE);
            float currentSize = slotSize * scale * expandT;

            const FeatureMeta* meta = nullptr;
            const ActionMeta* actionMeta = nullptr;
            bool featureActive = false;
            ImVec4 slotColor = Colors::PRIMARY_LIGHT;

            if (slot.kind == FavouriteKind::FeatureToggle) {
                meta = FindMeta(slot.featureId);
                if (meta) {
                    GetFeatureState(slot.featureId, featureActive);
                    slotColor = featureActive ? meta->color : Colors::PRIMARY_MEDIUM;
                }
            } else if (slot.kind == FavouriteKind::Action) {
                actionMeta = FindActionMeta(slot.action);
                if (actionMeta) {
                    slotColor = Colors::ACCENT_ORANGE;
                }
            }

            if (expandT > 0.5f && slot.kind != FavouriteKind::None) {
                float lineAlpha = (expandT - 0.5f) * 2.0f * 0.3f;

                ImU32 lineColor;
                if (featureActive && meta) {
                    lineColor = ImGui::GetColorU32(meta->color);
                } else {
                    lineColor = ImGui::GetColorU32(Colors::PRIMARY_LIGHT);
                }
                DrawConnectingLine(dl, centerScreen, slotCenter, lineColor, 1.5f, lineAlpha);
            }

            if (featureActive && meta && !s_editMode) {
                float glowIntensity = 0.3f + 0.2f * std::sin(slot.glowPhase);
                glowIntensity *= expandT;

                DrawGlowCircle(dl, slotCenter, currentSize * 0.5f,
                             ImGui::GetColorU32(meta->color), glowIntensity);
            }

            ImU32 bgColor;
            ImU32 innerColor;
            ImU32 outerColor;

            if (s_editMode) {
                bool hasContent = (slot.kind != FavouriteKind::None);

                if (hasContent) {
                    ImVec4 editBgColor = Colors::PRIMARY_MEDIUM;
                    editBgColor.x += 0.15f * s_editModeAnim;
                    editBgColor.w = 0.8f;
                    bgColor = ImGui::GetColorU32(editBgColor);
                    innerColor = bgColor;

                    ImVec4 outerVec = editBgColor;
                    outerVec.x *= 0.5f;
                    outerVec.y *= 0.5f;
                    outerVec.z *= 0.5f;
                    outerColor = ImGui::GetColorU32(outerVec);
                } else {
                    float pulseIntensity = 0.15f + 0.1f * std::sin(s_globalGlowPhase);
                    ImVec4 emptyColor = Colors::PRIMARY_LIGHT;
                    emptyColor.x += pulseIntensity;
                    emptyColor.y += pulseIntensity;
                    emptyColor.z += pulseIntensity;
                    emptyColor.w = 0.6f;
                    bgColor = ImGui::GetColorU32(emptyColor);
                    innerColor = bgColor;

                    ImVec4 outerVec = emptyColor;
                    outerVec.x *= 0.7f;
                    outerVec.y *= 0.7f;
                    outerVec.z *= 0.7f;
                    outerColor = ImGui::GetColorU32(outerVec);
                }
            } else {
                float alpha = 0.75f;
                ImVec4 color = slotColor;
                color.w *= alpha;
                bgColor = ImGui::GetColorU32(color);
                innerColor = bgColor;

                ImVec4 outerVec = slotColor;
                outerVec.x *= 0.6f;
                outerVec.y *= 0.6f;
                outerVec.z *= 0.6f;
                outerColor = ImGui::GetColorU32(outerVec);
            }

            dl->AddCircleFilled(slotCenter, currentSize * 0.5f, outerColor, Config::RADIAL_SEGMENTS);
            dl->AddCircleFilled(slotCenter, currentSize * 0.4f, innerColor, Config::RADIAL_SEGMENTS);

            ImU32 borderColor;
            if (s_editMode) {
                bool hasContent = (slot.kind != FavouriteKind::None);
                if (hasContent) {
                    ImVec4 redBorder = Colors::ACCENT_RED;
                    redBorder.w = 0.8f + 0.2f * std::sin(s_globalGlowPhase);
                    borderColor = ImGui::GetColorU32(redBorder);
                } else {
                    ImVec4 blueBorder = Colors::ACCENT_CYAN;
                    blueBorder.w = 0.8f + 0.2f * std::sin(s_globalGlowPhase + 3.14159f);
                    borderColor = ImGui::GetColorU32(blueBorder);
                }
            } else {
                if (featureActive && meta) {
                    borderColor = ImGui::GetColorU32(meta->color);
                } else if (slot.kind == FavouriteKind::Action && actionMeta) {
                    borderColor = ImGui::GetColorU32(Colors::ACCENT_ORANGE);
                } else {
                    borderColor = ImGui::GetColorU32(Colors::PRIMARY_DARK);
                }
            }
            dl->AddCircle(slotCenter, currentSize * 0.5f, borderColor, Config::RADIAL_SEGMENTS, 2.0f);

            bool hasItem = false;
            const char* iconLabel = "UiMiscPlusMedium";
            if (slot.kind == FavouriteKind::FeatureToggle && meta) {
                iconLabel = meta->icon;
                hasItem = true;
            } else if (slot.kind == FavouriteKind::Action && actionMeta) {
                iconLabel = actionMeta->icon;
                hasItem = true;
            }

            if (s_editMode && !hasItem) {
                iconLabel = "UiMiscMinus";
            }

            float iconScale = 0.65f;
            float slotIconSize = currentSize * iconScale;

            ImVec2 slotIconPos = ImVec2(slotCenter.x - slotIconSize * 0.5f,
                                       slotCenter.y - slotIconSize * 0.5f);

            ImVec4 iconColor = Colors::STATE_INACTIVE;

            if (!s_editMode) {
                if (featureActive && meta) {
                    iconColor = ImVec4(1, 1, 1, 1);
                }
                if (slot.kind == FavouriteKind::Action && actionMeta) {
                    iconColor = ImVec4(1, 1, 1, 1);
                }
            } else if (!hasItem) {
                iconColor = ImVec4(1, 1, 1, 1);
            }
            ImGui::SetCursorScreenPos(slotIconPos);
            tsm::ui::widgets::Icon(iconLabel, slotIconSize, iconColor);

            if (slotsInteractive && clicked) {

                if (s_editMode) {
                    slot.kind = FavouriteKind::None;
                    SaveSlotsToPrefs();
                } else {
                    if (slot.kind == FavouriteKind::FeatureToggle) {
                        ToggleFeature(slot.featureId);
                    } else if (slot.kind == FavouriteKind::Action) {
                        ExecuteAction(slot.action);
                    } else {
                        s_selectedSlotForAssignment = i;
                        s_searchFilter.clear();
                        openAssignPopup = true;
                    }
                }
            }

            if (slotsInteractive && rightClicked && !s_editMode) {

                s_selectedSlotForAssignment = i;
                s_searchFilter.clear();
                openAssignPopup = true;
            }

            ImGui::PopID();
        }

        if (openAssignPopup) {
            ImGui::OpenPopup("##fav_select");
        }

        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (vp) {
            float minWidth = DP(320.0f);
            float maxWidth = std::max(minWidth, vp->Size.x - DP(32.0f));
            float targetWidth = std::min(std::max(minWidth, vp->Size.x * 0.5f), maxWidth);

            float minHeight = DP(320.0f);
            float verticalPadding = DP(32.0f);
            float targetHeight = std::max(minHeight, vp->Size.y - verticalPadding);

            ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f,
                                           vp->Pos.y + vp->Size.y * 0.5f),
                                    ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(targetWidth, targetHeight), ImGuiCond_Appearing);
        }

        ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoTitleBar |
                                      ImGuiWindowFlags_NoResize   |
                                      ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(Colors::PRIMARY_DARK));

        if (ImGui::BeginPopupModal("##fav_select", nullptr, modalFlags)) {
            float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            const char* titleText = tsm::ui::i18n::Tr("Assign Favourite");
            ImVec2 ts = ImGui::CalcTextSize(titleText);
            float titleX = ImGui::GetWindowContentRegionMin().x + std::max(0.0f, (cont_w - ts.x) * 0.5f);
            ImGui::SetCursorPosX(titleX);
            ImGui::TextUnformatted(titleText);
            ImGui::Separator();

            ImGui::Dummy(ImVec2(0, DP(4.0f)));

            char searchBuf[256] = {};
            strncpy(searchBuf, s_searchFilter.c_str(), sizeof(searchBuf) - 1);
            if (tsm::ui::widgets::SearchCard("Search features...", searchBuf, sizeof(searchBuf))) {
                s_searchFilter = searchBuf;
            }

            float contentHeight = ImGui::GetContentRegionAvail().y;
            float buttonHeight = ImGui::GetFrameHeight();
            float bottomPadding = DP(16.0f);
            float reservedHeight = DP(8.0f) + buttonHeight + bottomPadding;
            float listHeight = std::max(DP(120.0f), contentHeight - reservedHeight);

            if (tsm::ui::widgets::BeginPannableChild("##fav_feature_list", ImVec2(0, listHeight))) {
                tsm::ui::widgets::CenterSeparator("FEATURES");

                for (const auto& meta : kFeatureMetas) {
                    if (!s_searchFilter.empty()) {
                        std::string nameLower = meta.name;
                        std::string searchLower = s_searchFilter;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                        if (nameLower.find(searchLower) == std::string::npos) {
                            continue;
                        }
                    }

                    bool isActive = false;
                    GetFeatureState(meta.id, isActive);

                    const char* trailing = "FAVORITE";

                    if (tsm::ui::widgets::ButtonCard(meta.name,
                                                     meta.description,
                                                     meta.icon,
                                                     trailing)) {
                        if (s_selectedSlotForAssignment >= 0 && s_selectedSlotForAssignment < (int)s_slots.size()) {
                            s_slots[s_selectedSlotForAssignment].kind = FavouriteKind::FeatureToggle;
                            s_slots[s_selectedSlotForAssignment].featureId = meta.id;
                            SaveSlotsToPrefs();
                            s_expanded = true;
                        }
                        clickConsumedByPopup = true;
                        ImGui::CloseCurrentPopup();
                    }
                }

                tsm::ui::widgets::CenterSeparator("ACTIONS");

                for (const auto& act : kActionMetas) {
                    if (!s_searchFilter.empty()) {
                        std::string nameLower = act.name;
                        std::string searchLower = s_searchFilter;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                        if (nameLower.find(searchLower) == std::string::npos) {
                            continue;
                        }
                    }

                    const char* trailing = "FAVORITE";

                    if (tsm::ui::widgets::ButtonCard(act.name,
                                                     act.description,
                                                     act.icon,
                                                     trailing)) {
                        if (s_selectedSlotForAssignment >= 0 && s_selectedSlotForAssignment < (int)s_slots.size()) {
                            s_slots[s_selectedSlotForAssignment].kind = FavouriteKind::Action;
                            s_slots[s_selectedSlotForAssignment].action = act.id;
                            SaveSlotsToPrefs();
                            s_expanded = true;
                        }
                        clickConsumedByPopup = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            tsm::ui::widgets::EndPannableChild();

            ImGui::Dummy(ImVec2(0, DP(8.0f)));

            float btn_w = cont_w * 0.4f;
            float btnX = ImGui::GetWindowContentRegionMin().x + std::max(0.0f, (cont_w - btn_w) * 0.5f);
            ImGui::SetCursorPosX(btnX);
            if (tsm::ui::widgets::SecondaryButton("Close", ImVec2(btn_w, 0))) {
                clickConsumedByPopup = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

    }

    ImVec2 centerTopLeft = ImVec2(centerScreen.x - centerBtnSize * 0.5f,
                                  centerScreen.y - centerBtnSize * 0.5f);

    ImGui::SetCursorScreenPos(centerTopLeft);
    ImGui::PushID("fav_center");
    bool centerClicked = ImGui::InvisibleButton("##fav_center_btn", ImVec2(centerBtnSize, centerBtnSize));
    bool centerActive = ImGui::IsItemActive();
    bool centerActivated = ImGui::IsItemActivated();
    ImGui::PopID();

    if (centerActivated) {
        s_dragStartMenuPos = s_menuPosition;
        s_isDragging = false;
    }

    if (centerActive && !anySlotInteracted) {
        ImVec2 dragDelta = ImGui::GetMouseDragDelta(0, DRAG_THRESHOLD);
        if (dragDelta.x != 0.0f || dragDelta.y != 0.0f) {
            s_isDragging = true;

            ImVec2 newPos = ImVec2(s_dragStartMenuPos.x + dragDelta.x, s_dragStartMenuPos.y + dragDelta.y);

            float margin = contentExtent;
            newPos.x = std::clamp(newPos.x, margin, displaySize.x - margin);
            newPos.y = std::clamp(newPos.y, margin, displaySize.y - margin);

            s_menuPosition = newPos;
        }
    }

    float targetCenterActive = centerActive ? 1.0f : 0.0f;
    s_centerActiveAnim += (targetCenterActive - s_centerActiveAnim) * (Config::ANIMATION_SPEED * 1.5f) * deltaTime;
    s_centerActiveAnim = std::clamp(s_centerActiveAnim, 0.0f, 1.0f);

    float centerScale = 1.0f - s_centerActiveAnim * 0.05f;
    float currentCenterSize = centerBtnSize * centerScale;

    ImVec4 accentColor = tsm::ui::GetAccentColor();

    if (s_expanded) {
        float glowIntensity = 0.3f;
        ImVec4 glowColor = s_editMode
            ? Colors::ACCENT_RED
            : ImVec4(accentColor.x, accentColor.y, accentColor.z, 1.0f);
        DrawGlowCircle(dl, centerScreen, currentCenterSize * 0.5f,
                       ImGui::GetColorU32(glowColor), glowIntensity);
    }

    ImVec4 centerColorBase = s_editMode
        ? Colors::ACCENT_RED
        : ImVec4(accentColor.x, accentColor.y, accentColor.z, 1.0f);

    ImU32 centerInner = ImGui::GetColorU32(centerColorBase);
    ImVec4 outerVec = centerColorBase;
    outerVec.x *= 0.6f;
    outerVec.y *= 0.6f;
    outerVec.z *= 0.6f;
    ImU32 centerOuter = ImGui::GetColorU32(outerVec);

    dl->AddCircleFilled(centerScreen, currentCenterSize * 0.5f, centerOuter, Config::RADIAL_SEGMENTS);
    dl->AddCircleFilled(centerScreen, currentCenterSize * 0.4f, centerInner, Config::RADIAL_SEGMENTS);

    ImU32 borderColor = ImGui::GetColorU32(Colors::PRIMARY_DARK);
    dl->AddCircle(centerScreen, currentCenterSize * 0.5f, borderColor, Config::RADIAL_SEGMENTS, 3.0f);

    const char* centerIcon = "UiMiscFavorite";
    if (s_expanded) {
        centerIcon = s_editMode ? "UiMiscMinus" : "UiMenuEdit";
    }

    float centerIconSize = currentCenterSize * 0.5f;
    ImVec2 centerIconPos = ImVec2(centerScreen.x - centerIconSize * 0.5f,
                                  centerScreen.y - centerIconSize * 0.5f);
    ImGui::SetCursorScreenPos(centerIconPos);
    tsm::ui::widgets::Icon(centerIcon, centerIconSize, ImVec4(1, 1, 1, 1));

    if (centerClicked && !s_isDragging) {
        if (!s_expanded) {
            s_expanded = true;
            s_editMode = false;
        } else {
            s_editMode = !s_editMode;
        }
    }

    bool assignPopupOpen = ImGui::IsPopupOpen("##fav_select");

    if (s_expanded && !assignPopupOpen && !clickConsumedByPopup && ImGui::IsMouseClicked(0) && !centerActive && !anySlotInteracted) {
        s_expanded = false;
        s_editMode = false;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

}}}
