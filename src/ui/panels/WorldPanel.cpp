#include <ui/panels/WorldPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/helpers/SubTabRenderer.h>
#include <imgui/imgui.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <game/memory/Patch.h>
#include <game/memory/Address.h>
#include <game/memory/offsets.h>
#include <game/memory/Memory.h>
#include <game/memory/api.h>

#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <network/QuestApi.h>
#include <network/SocialManager.h>
#include <algorithm>
#include <state/ModState.h>
#include <features/manager/FeatureManager.h>
#include <resources/data/timelines/CandleSpace_Timelines.h>
#include <resources/data/timelines/Dawn_Timelines.h>
#include <resources/data/timelines/DayHubCave_Timelines.h>
#include <resources/data/timelines/MainStreet_Timelines.h>
#include <resources/data/timelines/Prairie_ButterflyFields_Timelines.h>
#include <resources/data/timelines/Prairie_NestAndKeeper_Timelines.h>
#include <resources/data/timelines/Prairie_Island_Timelines.h>
#include <resources/data/timelines/Prairie_WildLifePark_Timelines.h>
#include <resources/data/timelines/RainForest_Timelines.h>
#include <resources/data/timelines/Rain_Timelines.h>
#include <sstream>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

namespace tsm { namespace ui { namespace tabs {

void DrawWorldTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;

    tsm::game::memory::EnsureInitialized();

    static int s_worldSubIndex = 0;
    static bool s_allowOverride = false;
    static float s_sunMoonX = 0.0f;
    static float s_sunMoonY = 0.0f;
    static float s_sunMoon  = 0.0f;
    static float s_sunSize  = 1.0f;
    static float s_moonPhase = 0.0f;
    static float s_exposure  = 1.0f;
    static float s_flameToCandle = 1.0f;
    static float s_flowerHeight  = 1.0f;
    static float s_flowerSize    = 1.0f;
    static float s_rainIntensity = 0.0f;
    static float s_windIntensity = 0.0f;
    static float s_gameSpeed = 1.0f;
    static bool s_gameSpeedInit = false;

    static const char* kSubIcons[] = {
        "UiMiscTarget",
        "UiMenuDevCorner",
        "UiMiscHide",
        "UiMenuBrightness",
        "UiMenuTimer",
        "UiMenuBuffEventCurrency"
    };
    DrawSubTabs(kSubIcons, 6, s_worldSubIndex);

    if (BeginCard("##world_card", 8.0f, false)) {
        switch (s_worldSubIndex) {
            case 0: {
                if (BeginPannableChild("##session_scroll")) {
                    auto& ms = tsm::state::ModState::Get();

					static bool s_showMapRadar = false;
                    static bool s_disableMultiplayer = false;
                    static bool s_disableLevelChangeEvents = false;
                    static bool s_lightAllPlayers = false;
                    static bool s_revealPlayers = false;
                    static bool s_hideHudExceptForStarFragments = false;

                    CenterSeparator("Multiplayer & Session");
                    if (ButtonCard("Disconnect Session", "Disconnect from the current session", "UiMenuSignal")) {
                        tsm::lua::queue::Enqueue("Disconnect(game)");
                    }
                    if (ButtonCard("Clear Level Authority", "Clear current level authority", "UiMenuSignal")) {
                        tsm::lua::queue::Enqueue("ClearLevelAuthority(game)");
                    }
                    if (ToggleCardIcon("Disable Multiplayer", "Puts you into an offline state", "UiMenuSignal", &s_disableMultiplayer)) {
                        std::uint8_t b = s_disableMultiplayer ? 0 : 1;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kEnableMultiplayer, b);
                    }
                    if (ToggleCardIcon("Fast Home", "Removes home delay", "UiMenuConstellationHome", &ms.world.fastHome)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Disable Spirit Gates", "Removes gate requirements", "UiMenuLock", &ms.world.disableSpiritGates)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Disable Cutscenes", "Auto-skip all cutscenes", "UiMenuTimer", &ms.world.disableCutscenes)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Disable Level Change Events", "Disables level change events", "UiMenuBroadcastDisabled", &s_disableLevelChangeEvents)) {
                        std::uint8_t b = s_disableLevelChangeEvents ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableLevelChangeEvents, b);
                    }

                    CenterSeparator("Visibility & Discovery");
                    if (ButtonCard("Hide Unlit Players", "Hide unlit/ghost players from the session", "UiHudWoundedPlayer", "HIDE")) {
                        tsm::lua::queue::Enqueue("THideRemotePlayers()");
                    }
                    {
                        std::uint8_t b = tsm::game::memory::ReadByte(tsm::game::Offsets::kAvatarCharcoaling);
                        s_lightAllPlayers = (b == 0);
                        if (ToggleCardIcon("Light All Players", "Make all players visible", "UiMiscView", &s_lightAllPlayers)) {
                            tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::LightAllPlayers, s_lightAllPlayers);
                        }
                    }
                    {
                        std::uint32_t v = tsm::game::memory::ReadU32(tsm::game::Offsets::kRevealPlayers);
                        s_revealPlayers = (v == tsm::game::Signatures::kNopInstruction);
                        if (ToggleCardIcon("Reveal All Players", "Make all players visible (client-side)", "UiMiscView", &s_revealPlayers)) {
                            tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::RevealAllPlayers, s_revealPlayers);
                        }
                    }
                    if (ToggleCardIcon("Show Modded Outfits", "See outfits players don't own", "UiMiscView", &ms.world.showModdedOutfits)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Read Table Messages", "View chat messages from tables without sitting at them", "UiMenuGlobalChatEnabled", &ms.world.readTableMessages)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }

                    CenterSeparator("UI & HUD");
                    if (ToggleCardIcon("Hide UI", "Disables all UI elements", "UiMiscHide", &s_hideHudExceptForStarFragments)) {
                        std::uint8_t b = s_hideHudExceptForStarFragments ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kHideHudExceptForStarFragments, b);
                    }
                    if (ToggleCardIcon("Pause UI Animations", "Stops all UI animations", "UiMenuNoSymbol", &ms.world.pauseUiAnimations)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }

                    CenterSeparator("Map & Locations");
                    if (ToggleCardIcon("Show All Spirits", "Displays spirits on honk", "UiEmotePoint", &ms.world.showSpirits)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Show All Wing Buffs", "Displays wing buffs on honk", "UiMenuWingBuff", &ms.world.showWingBuffs)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Show All Map Shrines", "Displays map shrines on honk", "UiMenuMap", &ms.world.showMapShrines)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    if (ToggleCardIcon("Show Map Shrine Radar", "Displays all map shrines and current distance", "UiMenuMap", &s_showMapRadar)) {
                        std::uint8_t b = s_showMapRadar ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kMapShrineRadar, b);
                    }
                    if (ToggleCardIcon("Show All Map Items", "Displays all map items", "UiMenuMap", &ms.world.showMapItems)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                }
                EndPannableChild();
                break; }

            case 1: {
                if (BeginPannableChild("##debug_scroll")) {
                    CenterSeparator("Debug Menus");
                    static bool s_fishSchoolDebug = false;
                    if (ToggleCardIcon("Show Fish School Menu", "Enables fish school debug menu", "UiMenuDevCorner", &s_fishSchoolDebug)) {
                        std::uint8_t b = s_fishSchoolDebug ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kFishSchoolDebug, b);
                    }
                    static bool s_uiShowHierarchy = false;
                    if (ToggleCardIcon("Show Hierarchy Menu", "Enables hierarchy debug menu", "UiMenuDevCorner", &s_uiShowHierarchy)) {
                        std::uint8_t b = s_uiShowHierarchy ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kUiShowHierarchy, b);
                    }
                    static bool s_uiShowTvDebugUi = false;
                    if (ToggleCardIcon("Show TV Menu", "Enables TV debug menu", "UiMenuDevCorner", &s_uiShowTvDebugUi)) {
                        std::uint8_t b = s_uiShowTvDebugUi ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kTvDebugUi, b);
                    }
                    static bool s_dyeDebug = false;
                    if (ToggleCardIcon("Show Dye Menu", "Enables dye debug menu", "UiMenuDevCorner", &s_dyeDebug)) {
                        std::uint8_t b = s_dyeDebug ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDyeDebug, b);
                    }
                }
                EndPannableChild();
                break; }

            case 2: {
                if (BeginPannableChild("##rendering_scroll")) {
                    CenterSeparator("Rendering Toggles");
                    static bool s_disableTerrain = false;
                    if (ToggleCardIcon("Disable Terrain", "Disables terrain rendering", "UiMiscHide", &s_disableTerrain)) {
                        std::uint8_t b = s_disableTerrain ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableTerrain, b);
                    }
                    static bool s_disableObjects = false;
                    if (ToggleCardIcon("Disable Objects", "Disables object rendering", "UiMiscHide", &s_disableObjects)) {
                        std::uint8_t b = s_disableObjects ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableObjects, b);
                    }
                    static bool s_disableObjectSkirts = false;
                    if (ToggleCardIcon("Disable Object Skirts", "Disables object skirts rendering", "UiMiscHide", &s_disableObjectSkirts)) {
                        std::uint8_t b = s_disableObjectSkirts ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableObjectSkirts, b);
                    }
                    static bool s_disableModels = false;
                    if (ToggleCardIcon("Disable Models", "Disables model rendering", "UiMiscHide", &s_disableModels)) {
                        std::uint8_t b = s_disableModels ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableModels, b);
                    }
                    static bool s_disableAvatars = false;
                    if (ToggleCardIcon("Disable Avatars", "Disables avatar rendering", "UiMiscHide", &s_disableAvatars)) {
                        std::uint8_t b = s_disableAvatars ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableAvatars, b);
                    }
                    static bool s_disableWater = false;
                    if (ToggleCardIcon("Disable Water", "Disables water rendering", "UiMiscHide", &s_disableWater)) {
                        std::uint8_t b = s_disableWater ? 0 : 1;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kEnableWater, b);
                    }
                    static bool s_disableOcean = false;
                    if (ToggleCardIcon("Disable Ocean", "Disables ocean rendering", "UiMiscHide", &s_disableOcean)) {
                        std::uint8_t b = s_disableOcean ? 0 : 1;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kEnableOcean, b);
                    }
                    static bool s_disableClouds = false;
                    if (ToggleCardIcon("Disable Clouds", "Disables cloud rendering", "UiMiscHide", &s_disableClouds)) {
                        std::uint8_t b = s_disableClouds ? 0 : 1;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kEnableClouds, b);
                    }
                    static bool s_disableLights = false;
                    if (ToggleCardIcon("Disable Lights", "Disables light rendering", "UiMiscHide", &s_disableLights)) {
                        std::uint8_t b = s_disableLights ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kDisableLights, b);
                    }
                }
                EndPannableChild();
                break; }

            case 3: {
                if (BeginPannableChild("##environment_scroll")) {
                    if (!s_gameSpeedInit) {
                        s_gameSpeed = tsm::game::api::GameSpeed();
                        s_gameSpeedInit = true;
                    }

                    CenterSeparator("Game Speed");

                    if (FloatCardIcon("Game Speed", "Adjust overall game tick speed", "UiMenuTimer",
                        &s_gameSpeed, 0.10f, 50.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_gameSpeed, 0.10f, 10.0f);
                        tsm::game::api::SetGameSpeed(v);
                    }

                    ImGui::Dummy(ImVec2(0, 4));

                    CenterSeparator("Environment");
                    if (ToggleCardIcon("Allow Override", "Enable to apply Sun/Moon, Phase and Exposure changes", "UiMenuLock", &s_allowOverride)) {
                        std::uint8_t b = s_allowOverride ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kAllowOverride, b);
                    }

                    ImGui::BeginDisabled(!s_allowOverride);
                    if (FloatCardIcon("Sun/Moon X", "Horizontal position (degrees)", "UiMenuMap", &s_sunMoonX, -360.0f, 360.0f, 1.0f, "%.1f")) {
                        float v = std::clamp(s_sunMoonX, -360.0f, 360.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kSunMoonXPosition, v);
                    }
                    if (FloatCardIcon("Sun/Moon Y", "Vertical position (degrees)", "UiMenuMap", &s_sunMoonY, -90.0f, 90.0f, 1.0f, "%.1f")) {
                        float v = std::clamp(s_sunMoonY, -90.0f, 90.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kSunMoonYPosition, v);
                    }
                    if (FloatCardIcon("Sun-Moon Blend", "0 = Sun, 1 = Moon", "UiMenuMemoryRecord", &s_sunMoon, 0.0f, 1.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_sunMoon, 0.0f, 1.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kSunMoon, v);
                    }
                    if (FloatCardIcon("Sun/Moon Size", "Apparent size scale", "UiMiscTarget", &s_sunSize, 0.0f, 10.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_sunSize, 0.0f, 10.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kSunMoonSize, v);
                    }
                    if (FloatCardIcon("Moon Phase", "0.0 - 0.5", "UiMenuTimer", &s_moonPhase, 0.0f, 0.5f, 1.0f, "%.3f")) {
                        float v = std::clamp(s_moonPhase, 0.0f, 0.5f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kMoonPhase, v);
                    }
                    if (FloatCardIcon("Exposure", "Scene exposure", "UiMiscView", &s_exposure, 0.0f, 10.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_exposure, 0.0f, 10.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kExposure, v);
                    }
                    ImGui::EndDisabled();

                    CenterSeparator("World Props");
                    if (FloatCardIcon("Flame to Candle Scale", "Scale when converting flame to candle", "UiMenuBuffCandle", &s_flameToCandle, 0.0f, 10.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_flameToCandle, 0.0f, 10.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kFlameToCandleScale, v);
                    }
                    if (FloatCardIcon("Flower Height", "Plant height scale", "UiMenuStageFallingFlower", &s_flowerHeight, 0.0f, 10.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_flowerHeight, 0.0f, 10.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kFlowerHeight, v);
                    }
                    if (FloatCardIcon("Flower Size", "Plant size scale", "UiMenuStageFallingFlower", &s_flowerSize, 0.0f, 10.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_flowerSize, 0.0f, 10.0f);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kFlowerSize, v);
                    }

                    CenterSeparator("Weather");
                    if (FloatCardIcon("Rain Intensity", "Increase rain amount and speed", "UiMenuStageFallingRain", &s_rainIntensity, 0.0f, 100.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_rainIntensity, 0.0f, 100.0f);
                        tsm::lua::helpers::SetRainIntensity(v);
                    }
                    if (FloatCardIcon("Wind Intensity", "Increase wind speed", "UiMiscTarget", &s_windIntensity, 0.0f, 100.0f, 1.0f, "%.2f")) {
                        float v = std::clamp(s_windIntensity, 0.0f, 100.0f);
                        tsm::lua::helpers::SetWindIntensity(v);
                    }

                    CenterSeparator("World & Physics");
                    auto& ms = tsm::state::ModState::Get();
                    if (ToggleCardIcon("Disable Wind Walls", "Bypass wind wall barriers", "UiMenuSpellWind", &ms.world.disableWindWalls)) {
                        tsm::features::FeatureManager::Get().ApplyWorldFeatures();
                    }
                    static bool s_enableGravity = false;
                    if (ToggleCardIcon("Disable Gravity", "Removes gravity", "UiMenuBuffAntiGravity", &s_enableGravity)) {
                        std::uint8_t b = s_enableGravity ? 0 : 1;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kEnableGravity, b);
                    }

                    static bool s_disableAllCollision = false;
                    static std::uint32_t s_disableAllCollisionOriginal = 0;
                    if (ToggleCardIcon("Disable World Collision", "Disable collision with all geometry and entities", "UiMiscHide", &s_disableAllCollision)) {
                        if (s_disableAllCollision) {
                            s_disableAllCollisionOriginal = tsm::game::memory::ReadU32(tsm::game::Offsets::kDisableAllCollision);
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kDisableAllCollision, 0xD503201Fu);
                        } else {
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kDisableAllCollision, s_disableAllCollisionOriginal);
                        }
                    }

                    static bool s_disableObjectCollision = false;
                    static std::uint32_t s_disableObjectCollisionOriginal = 0;
                    if (ToggleCardIcon("Disable Object Collision", "Disable collision with world objects", "UiMiscHide", &s_disableObjectCollision)) {
                        if (s_disableObjectCollision) {
                            s_disableObjectCollisionOriginal = tsm::game::memory::ReadU32(tsm::game::Offsets::kDisableObjectCollision);
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kDisableObjectCollision, 0xD503201Fu);
                        } else {
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kDisableObjectCollision, s_disableObjectCollisionOriginal);
                        }
                    }

                    CenterSeparator("Entities & NPCs");
                    static bool s_freezeKrills = false;
                    static std::uint32_t s_freezeKrillsOriginal = 0;
                    if (ToggleCardIcon("Freeze Krills", "Freezes krills so they don't move", "UiEmoteCold", &s_freezeKrills)) {
                        if (s_freezeKrills) {
                            s_freezeKrillsOriginal = tsm::game::memory::ReadU32(tsm::game::Offsets::kFreezeKrills);
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kFreezeKrills, 0x52800008u);
                        } else {
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kFreezeKrills, s_freezeKrillsOriginal);
                        }
                    }
                    static bool s_birthdayKrills = false;
                    static std::uint32_t s_birthdayKrillsOriginal = 0;
                    if (ToggleCardIcon("Birthday Krills", "Gives krills birthday hats", "UiOutfitHornBirthdayHat01", &s_birthdayKrills)) {
                        if (s_birthdayKrills) {
                            s_birthdayKrillsOriginal = tsm::game::memory::ReadU32(tsm::game::Offsets::kBirthdayKrills);
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kBirthdayKrills, 0x14000006u);
                        } else {
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kBirthdayKrills, s_birthdayKrillsOriginal);
                        }
                    }
                }
                EndPannableChild();
                break; }

            case 4: {
                const char* levelPtr = tsm::game::api::LevelName();
                    std::string currentLevel = levelPtr ? levelPtr : "";

                    std::vector<std::string> timelines;
                    const unsigned char* data = nullptr;
                    std::size_t dataSize = 0;

                    if (currentLevel == "CandleSpace") {
                        data = kCandleSpace_TimelinesData;
                        dataSize = kCandleSpace_TimelinesSize;
                        CenterSeparator("CandleSpace Timelines");
                    } else if (currentLevel == "Dawn") {
                        data = kDawn_TimelinesData;
                        dataSize = kDawn_TimelinesSize;
                        CenterSeparator("Dawn Timelines");
                    } else if (currentLevel == "DayHubCave") {
                        data = kDayHubCave_TimelinesData;
                        dataSize = kDayHubCave_TimelinesSize;
                        CenterSeparator("DayHubCave Timelines");
                    } else if (currentLevel == "MainStreet") {
                        data = kMainStreet_TimelinesData;
                        dataSize = kMainStreet_TimelinesSize;
                        CenterSeparator("MainStreet Timelines");
                    } else if (currentLevel == "Prairie_ButterflyFields") {
                        data = kPrairie_ButterflyFields_TimelinesData;
                        dataSize = kPrairie_ButterflyFields_TimelinesSize;
                        CenterSeparator("Prairie ButterflyFields Timelines");
                    } else if (currentLevel == "Prairie_NestAndKeeper") {
                        data = kPrairie_NestAndKeeper_TimelinesData;
                        dataSize = kPrairie_NestAndKeeper_TimelinesSize;
                        CenterSeparator("Prairie NestAndKeeper Timelines");
                    } else if (currentLevel == "Prairie_Island") {
                        data = kPrairie_Island_TimelinesData;
                        dataSize = kPrairie_Island_TimelinesSize;
                        CenterSeparator("Prairie Island Timelines");
                    } else if (currentLevel == "Prairie_WildLifePark") {
                        data = kPrairie_WildLifePark_TimelinesData;
                        dataSize = kPrairie_WildLifePark_TimelinesSize;
                        CenterSeparator("Prairie WildLifePark Timelines");
                    } else if (currentLevel == "RainForest") {
                        data = kRainForest_TimelinesData;
                        dataSize = kRainForest_TimelinesSize;
                        CenterSeparator("RainForest Timelines");
                    } else if (currentLevel == "Rain") {
                        data = kRain_TimelinesData;
                        dataSize = kRain_TimelinesSize;
                        CenterSeparator("Rain Timelines");
                    } else {
                        CenterSeparator("Timelines");
                        char buf[256];
                        const char* fmt = tsm::ui::i18n::Tr("No timelines available for '%s'.");
                        std::snprintf(buf, sizeof(buf), fmt, currentLevel.c_str());
                        PaddedTextDisabled(buf);
                    }

                    if (data && dataSize > 0) {
                        std::string content(reinterpret_cast<const char*>(data), dataSize);
                        std::istringstream stream(content);
                        std::string line;
                        while (std::getline(stream, line)) {
                            if (!line.empty() && line.back() == '\r') {
                                line.pop_back();
                            }
                            if (!line.empty()) {
                                timelines.push_back(line);
                            }
                        }

                        if (timelines.empty()) {
                            PaddedTextDisabled("No timelines found in file.");
                        } else {
                            static char s_timelineSearch[128] = "";
                            SearchCard("Search...", s_timelineSearch, sizeof(s_timelineSearch));
                            if (BeginPannableChild("##timelines_scroll")) {

                                std::string searchLower = s_timelineSearch;
                                for (auto& ch : searchLower) ch = (char)tolower((unsigned char)ch);
                                const bool doFilter = !searchLower.empty();

                                for (const auto& timeline : timelines) {
                                    if (doFilter) {
                                        std::string timelineLower = timeline;
                                        for (auto& ch : timelineLower) ch = (char)tolower((unsigned char)ch);
                                        if (timelineLower.find(searchLower) == std::string::npos) {
                                            continue;
                                        }
                                    }

                                    ImGui::PushID(timeline.c_str());
                                    if (ButtonCard(timeline.c_str(), nullptr, "UiMenuTimer", "PLAY")) {
                                        std::string luaCmd = "PlayTimeline(game,\"" + timeline + "\")";
                                        tsm::lua::queue::Enqueue(luaCmd);
                                    }
                                    ImGui::PopID();
                                }
                            }
                            EndPannableChild();
                    }
                }
                break; }
            case 5: {
                if (!tsm::network::SocialManager::Get().IsLoggedIn()) {
                    CenterSeparator("Login required");
                    PaddedTextDisabled("No user/session detected.");
                    if (StandardButton("Check Again")) {}
                    break;
                }
                static std::atomic<bool> s_upcomingLoading{false};
                static std::mutex s_upcomingMtx;
                static std::vector<std::pair<std::string, int64_t>> s_upcomingQuests;
                static std::vector<std::tuple<std::string, int64_t, int>> s_upcomingEvents;
                static std::string s_upcomingError;
                static int s_upcomingSubTab = 0;
                static const char* kUpcomingSubIcons[] = { "UiMenuBuffEventCurrency", "UiMenuQuestHint" };
                DrawSubTabs(kUpcomingSubIcons, 2, s_upcomingSubTab, 0.75f);
                CenterSeparator("Upcoming");

                auto RequestUpcomingRefresh = [](){
                    if (s_upcomingLoading.exchange(true)) return;
                    std::thread([](){
                        std::vector<std::pair<std::string, int64_t>> quests;
                        std::vector<std::tuple<std::string, int64_t, int>> events;
                        std::string err;
                        const int64_t nowEpoch = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                        try {
                            auto qResp = tsm::network::FetchQuestSchedule();
                            if (qResp.is_object() && qResp.contains("quest_schedule") && qResp["quest_schedule"].is_array()) {
                                for (const auto& it : qResp["quest_schedule"]) {
                                    if (!it.is_object() || !it.contains("epoch") || !it.contains("quest_id")) continue;
                                    if (!it["epoch"].is_number_integer() || !it["quest_id"].is_string()) continue;
                                    int64_t ep = it["epoch"].get<int64_t>();
                                    if (ep >= nowEpoch) quests.push_back({it["quest_id"].get<std::string>(), ep});
                                }
                            }
                        } catch (...) {}
                        try {
                            auto eResp = tsm::network::FetchEventSchedule();
                            if (eResp.is_object() && eResp.contains("event_schedule") && eResp["event_schedule"].is_object()) {
                                const auto& sched = eResp["event_schedule"];
                                int64_t baseTime = sched.value("base_time", 0);
                                int64_t serverTime = sched.contains("server_time") && sched["server_time"].is_number_integer()
                                    ? sched["server_time"].get<int64_t>() : nowEpoch;
                                if (sched.contains("events") && sched["events"].is_array()) {
                                    const int64_t windowEnd = serverTime + (48 * 3600);
                                    for (const auto& ev : sched["events"]) {
                                        if (!ev.is_object() || !ev.contains("name") || !ev["name"].is_string()) continue;
                                        if (!ev.contains("when") || !ev["when"].is_array()) continue;
                                        std::string name = ev["name"].get<std::string>();
                                        int dur = 0;
                                        if (ev.contains("duration") && ev["duration"].is_number_integer()) dur = ev["duration"].get<int>();
                                        for (const auto& w : ev["when"]) {
                                            if (!w.is_number_integer()) continue;
                                            int64_t start = baseTime + w.get<int64_t>();
                                            if (start >= serverTime && start <= windowEnd) events.push_back({name, start, dur});
                                        }
                                    }
                                }
                            }
                        } catch (...) {}
                        if (quests.empty() && events.empty()) err = "Failed to load upcoming schedule";
                        std::sort(quests.begin(), quests.end(), [](const auto& a, const auto& b){ return a.second < b.second; });
                        std::sort(events.begin(), events.end(), [](const auto& a, const auto& b){ return std::get<1>(a) < std::get<1>(b); });
                        { std::lock_guard<std::mutex> lk(s_upcomingMtx); s_upcomingQuests = std::move(quests); s_upcomingEvents = std::move(events); s_upcomingError = std::move(err); }
                        s_upcomingLoading.store(false);
                    }).detach();
                };

                if (s_upcomingLoading.load()) {
                    ImGui::BeginDisabled(true);
                    ButtonCard("Refresh Upcoming", "Loading schedule...", "UiMenuQuestNew", "REFRESH");
                    ImGui::EndDisabled();
                } else {
                    if (ButtonCard("Refresh Upcoming", "Load quest schedule and events", "UiMenuQuestNew", "REFRESH")) {
                        RequestUpcomingRefresh();
                    }
                }
                { std::lock_guard<std::mutex> lk(s_upcomingMtx); if (!s_upcomingError.empty()) PaddedTextDisabled(s_upcomingError.c_str()); }
                float upH = std::max(120.0f, ImGui::GetContentRegionAvail().y);
                if (BeginPannableChild("##world_upcoming", ImVec2(0, upH))) {
                    const int64_t nowEpoch = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                    auto FormatRel = [](int64_t ep, int64_t now) -> std::string {
                        if (ep <= now) return "now";
                        int64_t d = ep - now;
                        if (d < 60) return "soon";
                        int64_t days = d / 86400; d %= 86400;
                        int64_t hrs = d / 3600; d %= 3600;
                        int64_t min = d / 60;
                        if (days > 0) return "in " + std::to_string(days) + "d " + std::to_string(hrs) + "h " + std::to_string(min) + "m";
                        if (hrs > 0) return "in " + std::to_string(hrs) + "h " + std::to_string(min) + "m";
                        return "in " + std::to_string(min) + "m";
                    };
                    if (s_upcomingSubTab == 0) {
                        std::vector<std::tuple<std::string, int64_t, int>> evs; { std::lock_guard<std::mutex> lk(s_upcomingMtx); evs = s_upcomingEvents; }
                        if (evs.empty()) PaddedTextDisabled("No upcoming events loaded.");
                        else for (const auto& e : evs) {
                            std::string timeText = "(" + FormatRel(std::get<1>(e), nowEpoch) + ")";
                            ButtonCard(std::get<0>(e).c_str(), timeText.c_str(), "UiMiscStar", "");
                        }
                    } else {
                        std::vector<std::pair<std::string, int64_t>> qs; { std::lock_guard<std::mutex> lk(s_upcomingMtx); qs = s_upcomingQuests; }
                        if (qs.empty()) PaddedTextDisabled("No upcoming dailies loaded.");
                        else for (const auto& q : qs) {
                            std::string label = tsm::network::GetQuestDescription(q.first);
                            std::string timeText = "(" + FormatRel(q.second, nowEpoch) + ")";
                            ButtonCard(label.c_str(), timeText.c_str(), "UiMenuQuestHint", "");
                        }
                    }
                }
                EndPannableChild();
                break; }
        }
        EndCard();
    }
}

}}}
