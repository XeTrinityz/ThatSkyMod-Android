#include <ui/panels/PlayerPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/helpers/ImGuiHelpers.h>
#include <ui/helpers/SubTabRenderer.h>
#include <ui/helpers/Toast.h>
#include <imgui/imgui.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <game/memory/api.h>
#include <utils/common/vec3.h>
#include <utils/strings/StringUtils.h>
#include <utils/strings/SpellNameFormatter.h>
#include <state/ModState.h>
#include <features/manager/FeatureManager.h>
#include <data/DataManager.h>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <game/hooks/HookManager.h>
#include <game/hooks/ShoutHook.h>
#include <network/ApiClient.h>
#include <features/camera/CameraControls.h>
#include <utils/storage/MessageStorage.h>
#include <nlohmann/json.hpp>
#include "../../../resources/data/animations_json.h"
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <array>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace tsm { namespace ui { namespace tabs {

static int s_playerSubIndex = 0;

void DrawPlayerTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;
    using namespace tsm::utils;

    static const char* kSubIcons[] = {
        "UiMenuShield",
        "UiMenuCamera",
        "UiEmoteAP16ArmWave0",
        "UiSocialDuetBowA",
        "UiMenuBuffSpellShop",
        "UiEmoteCallDefault",
        "UiMenuBuffStarBoat",
        "UiMenuSwitchMisc",
    };
    constexpr int sub_count = 8;

    DrawSubTabs(kSubIcons, sub_count, s_playerSubIndex);

    if (BeginCard("##player_card", 8.0f, false)) {
        switch (s_playerSubIndex) {

            case 0: {
                auto& ms = tsm::state::ModState::Get();
                if (BeginPannableChild("##general_scroll")) {
                    auto& pg = ms.playerGeneral;
                    bool inv         = pg.invincibility;
                    bool rain        = pg.antiRainDrain;
                    bool krill       = pg.antiKrill;
                    bool afk         = pg.antiAFK;
                    bool autoCharge  = pg.autoCharge;
                    bool superRun    = pg.superRun;
                    bool superSlidey = pg.superSlidey;
                    bool fastFlap = pg.fastFlap;

                    static bool s_superFlight = false;
                    static bool s_superFlightInit = false;
                    static bool s_superFlightPendingSync = false;

                    bool superFlightCanonical = pg.superFlight;
                    if (!s_superFlightInit) {
                        s_superFlight = superFlightCanonical;
                        s_superFlightInit = true;
                    } else if (!s_superFlightPendingSync) {
                        s_superFlight = superFlightCanonical;
                    }

                    if (s_superFlightPendingSync) {
                        s_superFlight = pg.superFlight;
                        s_superFlightPendingSync = false;
                    }

                    bool superLaunch = pg.superLaunch;

                    static int s_fakeCapeLevel = 0;

                    CenterSeparator("Protections");
                    if (ToggleCardIcon("Invincibility", "Prevents damage", "UiMenuShield", &inv)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::Invincibility, inv);
                    }
                    if (ToggleCardIcon("Anti Rain Drain", "Blocks rain damage", "UiMenuStageFallingRain", &rain)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::AntiRainDrain, rain);
                    }
                    if (ToggleCardIcon("Anti Krill", "Avoids Krill detection", "UiMenuBuffKrillRepellent", &krill)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::AntiKrill, krill);
                    }
                    if (ToggleCardIcon("Anti AFK", "Prevents idle timeout", "UiMenuTimer", &afk)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::AntiAFK, afk);
                    }

                    CenterSeparator("Abilities");
                    if (ToggleCardIcon("Infinite Energy", "Automatically refills energy", "UiMenuBattery03", &autoCharge)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::AutoCharge, autoCharge);
                    }
                    if (ToggleCardIcon("Super Run", "Increases run speed", "UiHudChevron", &superRun)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::SuperRun, superRun);
                    }
                    if (ToggleCardIcon("Super Slidey", "Extremely slippery surfaces", "UiMenuBlur", &superSlidey)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::SuperSlidey, superSlidey);
                    }

                    if (ToggleCardIcon("Super Flight", "Increased flight power", "UiSocialTeleport", &s_superFlight)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::SuperFlight, s_superFlight);
                        s_superFlightPendingSync = true;
                    }

                    if (ToggleCardIcon("Super Launch", "Increased launch power", "UiMenuBuffAntiGravity", &superLaunch)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::SuperLaunch, superLaunch);
                    }

                    if (ToggleCardIcon("Fast Flap", "Flap your wings faster", "UiMenuBuffFastCharge", &fastFlap)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::FastFlap, fastFlap);
                    }

                    static bool spellEmitter = false;
                    static std::uint32_t s_spellEmitterOriginal = 0;
                    if (ToggleCardIcon("Spell Emitter", "Spams spell VFX", "UiMenuBuffSpellShop", &spellEmitter)) {
                        if (spellEmitter) {
                            s_spellEmitterOriginal = tsm::game::memory::ReadU32(tsm::game::Offsets::kSpellEmitter);
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kSpellEmitter, 0x52800028u);

                            tsm::lua::helpers::GrantSpell("birthdayhat_krill");
                        } else {
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kSpellEmitter, s_spellEmitterOriginal);
                            tsm::lua::helpers::RemoveSpell("birthdayhat_krill");
                        }
                    }

                    static bool scooterMode = false;
                    static std::uint32_t s_scooterModeOriginal = 0;
                    if (ToggleCardIcon("Scooter Mode", "Disables movement animation", "UiEmoteCold", &scooterMode)) {
                        if (scooterMode) {
                            s_scooterModeOriginal = tsm::game::memory::ReadU32(tsm::game::Offsets::kScooterMode);
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kScooterMode, 0x52800008u);
                        } else {
                            tsm::game::memory::WriteU32(tsm::game::Offsets::kScooterMode, s_scooterModeOriginal);
                        }
                    }

                    static bool rainbowGlow = false;
                    if (ToggleCardIcon("Rainbow Glow", "Enables rainbow glow effect", "UiMenuBuffRainbow", &rainbowGlow)) {
                        std::uint8_t v = rainbowGlow ? 1 : 0;
                        void* p = tsm::game::memory::RvaToPtr(tsm::game::Offsets::kRainbowGlow);
                        tsm::game::memory::WriteBytes(p, &v, 1);
                    }

                    static bool rainbowTrails = false;
                    if (ToggleCardIcon("Rainbow Trails", "Enables rainbow trail effect", "UiMenuBuffTrail", &rainbowTrails)) {
                        std::uint8_t v = rainbowTrails ? 1 : 0;
                        void* p = tsm::game::memory::RvaToPtr(tsm::game::Offsets::kRainbowTrails);
                        tsm::game::memory::WriteBytes(p, &v, 1);
                    }

                    static bool bubbleTrails = false;
                    if (ToggleCardIcon("Bubble Trails", "Enables bubble trail effect", "UiMiscBubbles", &bubbleTrails)) {
                        std::uint8_t v = bubbleTrails ? 1 : 0;
                        void* p = tsm::game::memory::RvaToPtr(tsm::game::Offsets::kBubbleTrails);
                        tsm::game::memory::WriteBytes(p, &v, 1);
                    }

                    if (ToggleCardIcon("Unlimited Fireworks", "Removes fireworks cooldown", "UiMenuEventCurrencyFireworks", &ms.playerGeneral.unlimitedFireworks)) {
                        tsm::features::FeatureManager::Get().ApplyFeature(tsm::features::FeatureId::UnlimitedFireworks, ms.playerGeneral.unlimitedFireworks);
                    }

                    static bool crawlMode = false;
                    if (ToggleCardIcon("Crawl Mode", "Enable crawl animation", "UiEmoteAP04DontGo", &crawlMode)) {
                        tsm::lua::helpers::EnableCrawl(crawlMode);
                    }

                    if (IntCardIcon("Fake Cape Level", "Override wing level", "UiMenuWingBuff", &s_fakeCapeLevel, 0, 100)) {
                        int level = std::clamp(s_fakeCapeLevel, 0, 100);
                        tsm::game::memory::EnsureInitialized();
                        std::uint8_t en = (level > 0) ? 1 : 0;
                        tsm::game::memory::WriteByte(tsm::game::Offsets::kFakeCapeLevelEnabled, en);
                        tsm::game::memory::WriteFloat(tsm::game::Offsets::kFakeCapeLevel, static_cast<float>(level));
                    }

                    CenterSeparator("Transformations");
                    struct Item { const char* title; const char* vis; const char* desc; const char* icon; };
                    static const Item kItems[] = {
                        { "Transform into a Jellyfish",   "Jelly",       "Become a graceful jellyfish",     "UiEmoteCallJellyfish" },
                        { "Transform into a Crab",        "Crab",        "Scuttle around as a crab",        "UiEmoteCallCrab" },
                        { "Transform into a Broomstick",  "Broomstick",  "Ride a magical broomstick",       "UiOutfitPropMischiefBroom" },
                        { "Transform into a Fish",        "FishH",       "Swim the skies as a fish",        "UiEmoteCallFish" },
                        { "Transform into a Bird",        "Bird",        "Spread your wings like a bird",   "UiEmoteCallBird" },
                        { "Transform into a Manta",       "Manta",       "Glide with the manta",            "UiEmoteCallManta" },
                        { "Transform into a Butterfly",   "Butterfly",   "Flutter as a butterfly",          "UiEmoteButterfly" },
                        { "Transform into Crumpled Paper", "CrumpledPaper", "Become crumpled paper",        "UiEmoteCallFish" },
                        { "Transform into Stone Bust",    "StoneBust",   "Become a stone bust",             "UiEmoteCallFish" },
                        { "Transform into a Sky Kid [Flying]", "SkyKidFlying",        "Become a flying Sky Kid",      "UiEmoteCallDefault" },
                        { "Transform into a Sky Kid [Masked]",  "SkyKidAP18MaskedSerow", "Become a masked Sky Kid",     "UiEmoteCallDefault" },
                    };
                    static bool s_transformState[11] = {false,false,false,false,false,false,false,false,false,false,false};
                    static int s_activeTransform = -1;

                    for (size_t i = 0; i < 11; ++i) {
                        const auto& it = kItems[i];
                        if (ToggleCardIcon(it.title, it.desc, it.icon, &s_transformState[i])) {
                            if (s_transformState[i]) {
                                for (size_t j = 0; j < 11; ++j) {
                                    if (j != i) s_transformState[j] = false;
                                }
                                s_activeTransform = static_cast<int>(i);
                                tsm::lua::helpers::SetAudienceSettings(false, true, false, it.vis);
                            } else {
                                s_activeTransform = -1;
                                tsm::lua::helpers::SetAudienceSettings(false, false, false, "");
                            }
                        }
                    }
                }
                EndPannableChild();
                break; }

            case 1: {
                using namespace tsm::ui::widgets;

                if (BeginPannableChild("##camera_scroll")) {
                    CenterSeparator("Camera Tweaks");
                    static bool s_disableSnap = false;
                    static bool s_lockCamPos  = false;
                    static bool s_freeZoom = false;
                    static bool s_cameraInit = false;
                    if (!s_cameraInit) {
                        auto& ms0 = tsm::state::ModState::Get();
                        s_disableSnap = ms0.camera.disableSnap;
                        s_lockCamPos  = ms0.camera.lockCamPos;
                        s_freeZoom    = ms0.camera.freeZoom;
                    }

                    if (ToggleCardIcon("Disable Camera Snap", "Prevent camera snapping", "UiMiscView", &s_disableSnap)) {
                        auto& ms1 = tsm::state::ModState::Get();
                        ms1.camera.disableSnap = s_disableSnap;
                        tsm::features::FeatureManager::Get().ApplyCameraFeatures();
                    }

                    if (ToggleCardIcon("Lock Camera Position", "Disable camera position updates", "UiMenuLock", &s_lockCamPos)) {
                        auto& ms2 = tsm::state::ModState::Get();
                        ms2.camera.lockCamPos = s_lockCamPos;
                        tsm::features::FeatureManager::Get().ApplyCameraFeatures();
                    }

                    if (ToggleCardIcon("Free Zoom", "Unrestricted camera zoom", "UiMiscTarget", &s_freeZoom)) {
                        auto& ms3 = tsm::state::ModState::Get();
                        ms3.camera.freeZoom = s_freeZoom;
                        tsm::features::FeatureManager::Get().ApplyCameraFeatures();
                    }

                    if (!s_cameraInit) {
                        tsm::features::FeatureManager::Get().ApplyCameraFeatures();
                        s_cameraInit = true;
                    }

                    CenterSeparator("Camera Controls");

                    static float s_angleX = 0.0f;
                    static float s_angleY = 0.0f;
                    static float s_rotation = 0.0f;
                    static float s_fov = 0.8988445997f;
                    static float s_zoom = 1.0f;

                    if (!ImGui::IsAnyItemActive()) {
                        tsm::features::camera::ReadCameraAngleX(s_angleX);
                        tsm::features::camera::ReadCameraAngleY(s_angleY);
                        tsm::features::camera::ReadCameraRotation(s_rotation);
                        tsm::features::camera::ReadCameraFOV(s_fov);
                        tsm::features::camera::ReadCameraZoom(s_zoom);
                    }

                    if (FloatCardIcon("Camera Angle X", "Horizontal camera angle (radians)", "UiMiscArrowRight", &s_angleX, -3.14159265f, 3.14159265f, 1.0f, "%.3f")) {
                        tsm::features::camera::WriteCameraAngleX(s_angleX);
                    }

                    if (FloatCardIcon("Camera Angle Y", "Vertical camera angle (radians)", "UiMenuArrowCollapse", &s_angleY, -1.100f, 1.100f, 1.0f, "%.3f")) {
                        tsm::features::camera::WriteCameraAngleY(s_angleY);
                    }

                    if (FloatCardIcon("Camera Rotation", "Camera roll rotation (radians)", "UiMenuRotateHandle", &s_rotation, -3.14159265f, 3.14159265f, 0.0f, "%.3f")) {
                        tsm::features::camera::WriteCameraRotation(s_rotation);
                    }

                    if (FloatCardIcon("Field of View", "Camera FOV", "UiMiscView", &s_fov, 0.0f, 2.0f, 0.8988445997f, "%.3f")) {
                        tsm::features::camera::WriteCameraFOV(s_fov);
                    }

                    if (FloatCardIcon("Zoom", "Camera zoom level", "UiMiscTarget", &s_zoom, 0.5f, 4.0f, 1.0f, "%.2f")) {
                        tsm::features::camera::WriteCameraZoom(s_zoom);
                    }

                    CenterSeparator("Filters");
                    static float s_filterIntensity = 0.0f;
                    static int   s_filterIndex     = 0;
                    static const char* FILTER_TYPES[] = {
                        "Standard", "Red", "Green", "Blue", "Yellow",
                        "Orange", "Cyan", "Magenta", "Infrared", "Silver"
                    };

                    bool changedA = FloatCardIcon("Intensity", "Strength of the effect", "UiMenuBattery03", &s_filterIntensity, -10.0f, 10.0f, 0.0f, "%.2f");
                    bool changedB = SelectCardIcon("Filter Type", "Choose a color filter", "UiMenuBlur", &s_filterIndex, FILTER_TYPES, (int)(sizeof(FILTER_TYPES)/sizeof(FILTER_TYPES[0])));
                    if (changedA || changedB) {
                        float inten = std::clamp(s_filterIntensity, -1.0f, 100.0f);
                        int type = std::clamp(s_filterIndex, 0, (int)(sizeof(FILTER_TYPES)/sizeof(FILTER_TYPES[0])) - 1);
                        tsm::lua::helpers::SetCameraDesaturation(inten, type);
                    }

                }
                EndPannableChild();                break; }

            case 2: {
                auto& ms = tsm::state::ModState::Get();
                const auto& defs = tsm::data::DataManager::Get().GetCollectibleDefs();

                static int   s_emoteLevel = 1;
                static float s_emoteSpeed = 1.0f;
                static float s_emoteLoop  = 1.0f;

                CenterSeparator("Configuration");
                IntCardIcon("Level", "Choose the emote level", "UiMenuWingBuff", &s_emoteLevel, 1, 6);
                FloatCardIcon("Speed", "Playback speed multiplier", "UiMenuMemoryRecord", &s_emoteSpeed, 0.1f, 3.0f, 1.0f, "%.2f");
                FloatCardIcon("Loop (s)", "Loop duration in seconds", "UiOutfitPropAP24Clock", &s_emoteLoop, 0.0f, 10.0f, 1.0f, "%.1f");

                CenterSeparator("Emotes");
                if (BeginPannableChild("##emotes_scroll")) {
                    const nlohmann::json* arr = nullptr;
                    if (defs.is_array()) arr = &defs;
                    else if (defs.is_object()) {
                        if (auto it = defs.find("collectibles"); it != defs.end() && it->is_array()) arr = &(*it);
                        else if (auto it2 = defs.find("items"); it2 != defs.end() && it2->is_array()) arr = &(*it2);
                    }
                    if (arr && arr->is_array()) {
                        for (const auto& it : *arr) {
                            if (!it.is_object()) continue;
                            const auto* ty = it.contains("type") ? &it["type"] : nullptr;
                            if (!ty || !ty->is_string()) continue;
                            std::string tval = ToLower(ty->get<std::string>());
                            if (tval != "emote") continue;
                            std::string name  = it.contains("name") && it["name"].is_string() ? it["name"].get<std::string>() : std::string("?");
                            std::string icon  = it.contains("icon") && it["icon"].is_string() ? it["icon"].get<std::string>() : std::string();
                            {
                                std::string lname = ToLower(name);
                                if (lname == "sit" || lname == "beacon")
                                    continue;
                            }
                            if (icon.size() >= 4 && icon.compare(icon.size() - 4, 4, "Anim") == 0) {
                                icon.replace(icon.size() - 4, 4, "0");
                            }
                            int maxLv = it.contains("max_level") && it["max_level"].is_number_integer() ? it["max_level"].get<int>() : 1;
                            char desc[64];
                            const char* fmt = tsm::ui::i18n::Tr("Max level: %d");
                            std::snprintf(desc, sizeof(desc), fmt, maxLv);
                            if (ButtonCard(name.c_str(), desc, icon.empty() ? nullptr : icon.c_str(), "PLAY")) {
                                int lvl = std::clamp(s_emoteLevel, 1, 6);
                                float spd = std::clamp(s_emoteSpeed, 0.1f, 3.0f);
                                float loop = std::clamp(s_emoteLoop, 0.0f, 10.0f);
                                tsm::lua::helpers::PlayEmote(name, spd, loop, lvl);
                            }
                        }
                    } else {
                        PaddedTextDisabled("No CollectibleDefs loaded.");
                    }
                }
                tsm::ui::widgets::EndPannableChild();
            break; }

            case 3: {
                using namespace tsm::ui::widgets;
                static bool s_animParsed = false;
                static std::vector<std::string> s_animations;
                if (!s_animParsed) {
                    s_animParsed = true;
                    try {
                        const char* begin = reinterpret_cast<const char*>(kAnimationsData);
                        const char* end   = begin + (sizeof(kAnimationsData) / sizeof(kAnimationsData[0]));
                        nlohmann::json arr = nlohmann::json::parse(begin, end);
                        if (arr.is_array()) {
                            for (const auto& it : arr) {
                                if (it.is_string()) s_animations.push_back(it.get<std::string>());
                            }
                            std::sort(s_animations.begin(), s_animations.end());
                        }
                    } catch (...) {
                    }
                }

                CenterSeparator("Animations");
                static char s_search[128] = "";
                SearchCard("Search...", s_search, sizeof(s_search));
                if (BeginPannableChild("##animations_scroll")) {
                    std::string q = ToLower(s_search);
                    const bool doFilter = !q.empty();

                    for (const std::string& name : s_animations) {
                        if (doFilter) {
                            std::string lname = ToLower(name);
                            if (lname.find(q) == std::string::npos) continue;
                        }
                        if (ButtonCard(name.c_str(), "Play animation", "UiMenuMemoryRecord", "PLAY")) {
                            tsm::lua::helpers::PlayAnimation(name);
                        }
                    }
                }
                EndPannableChild();                break; }
            case 4: {
                using namespace tsm::ui::widgets;
                static bool s_spellsLoaded = false;
                static bool s_spellsLoading = false;
                static std::mutex s_spellsMutex;
                static std::vector<tsm::network::SpellDef> s_spellDefs;
                static char s_spellSearch[128] = "";

                const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
                const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
                const bool loggedIn = !uid.empty() && !sid.empty();

                if (!loggedIn) {
                    CenterSeparator("Login required");
                    PaddedTextDisabled("No user/session detected.");
                    if (StandardButton("Check Again")) {
                        s_spellsLoaded = false;
                        std::lock_guard<std::mutex> lk(s_spellsMutex);
                        s_spellDefs.clear();
                    }
                    break;
                }

                bool shouldStart = false;
                {
                    std::lock_guard<std::mutex> lk(s_spellsMutex);
                    shouldStart = (!s_spellsLoaded && !s_spellsLoading);
                    if (shouldStart) s_spellsLoading = true;
                }
                if (shouldStart) {
                    std::thread([&]() {
                        static tsm::network::ApiClient client;
                        auto defs = client.GetSpellDefinitions();
                        {
                            std::lock_guard<std::mutex> lk(s_spellsMutex);
                            s_spellDefs = std::move(defs);
                            s_spellsLoaded = true;
                            s_spellsLoading = false;
                        }
                    }).detach();
                }

                CenterSeparator("Quick Actions");
                if (StandardButton("Remove All Active Spells")) {
                    tsm::lua::helpers::RemoveSpell("");
                }

                SearchCard("Search...", s_spellSearch, sizeof(s_spellSearch));
                if (BeginPannableChild("##spells_scroll")) {
                    CenterSeparator("Placeables");
                    struct PlaceItem { const char* title; const char* icon; int id; };
                    static const PlaceItem kPlaceables[] = {
                        { "Social Recording",    "UiMenuRecordCandle",  0 },
                        { "Crane Message",       "UiMenuBuffCraneBoat", 1 },
                        { "Shared Space Candle", "UiMenuBuffCandle",    2 },
                        { "Gondola Message",     "UiMenuBuffGondola",   3 },
                        { "Boat Message",        "UiMenuBuffBoat",      4 },
                        { "Lantern Message",     "UiMenuBuffLantern",   5 },
                        { "Message Stone",       "UiMenuBuffStarBoat",  6 },
                    };

                    std::string placeQuery = ToLower(s_spellSearch);
                    const bool placeFilter = !placeQuery.empty();
                    for (const auto& it : kPlaceables) {
                        if (placeFilter) {
                            std::string lname = ToLower(it.title);
                            if (lname.find(placeQuery) == std::string::npos) continue;
                        }

                        if (ButtonCard(it.title, nullptr, it.icon, "GRANT")) {
                            switch (it.id) {
                                case 0: tsm::lua::helpers::TriggerSocialRecording(); break;
                                case 1: tsm::lua::helpers::TriggerToy("message_crane"); break;
                                case 2: tsm::lua::helpers::TriggerToy("sharedspace_candle"); break;
                                case 3: tsm::lua::helpers::TriggerToy("message_gondola"); break;
                                case 4: tsm::lua::helpers::TriggerToy("message_boat"); break;
                                case 5: tsm::lua::helpers::TriggerToy("message_lantern"); break;
                                case 6: tsm::lua::helpers::TriggerMessageStones(); break;
                            }
                        }
                    }

                    CenterSeparator("Spells");
                    std::string q = ToLower(s_spellSearch);
                    const bool doFilter = !q.empty();

                    for (const auto& def : s_spellDefs) {
                        std::string displayName = FormatSpellName(def.name);
                        if (doFilter) {
                            std::string lname = ToLower(displayName);
                            std::string rid = ToLower(def.name);
                            if (lname.find(q) == std::string::npos && rid.find(q) == std::string::npos) continue;
                        }
                        const char* iconC = def.icon.empty() ? nullptr : def.icon.c_str();
                        if (ButtonCard(displayName.c_str(), nullptr, iconC, "GRANT")) {
                            tsm::lua::helpers::GrantSpell(def.name);
                        }
                    }

                    if (s_spellsLoaded && s_spellDefs.empty()) {
                        ImGui::TextDisabled("No spells available.");
                        if (StandardButton("Refresh spell list")) {
                            static tsm::network::ApiClient client;
                            s_spellDefs = client.GetSpellDefinitions();
                            s_spellsLoaded = true;
                        }
                    }
                }
                EndPannableChild();                break; }
            case 5: {
                using namespace tsm::ui::widgets;

                if (BeginPannableChild("##shout_scroll")) {
                    CenterSeparator("Shout Editor");

                    bool enabled = tsm::game::hooks::shout::IsShoutEditorEnabled();
                    if (ToggleCardIcon("Enable Shout Editor", "Modify shout color and voice", "UiEmoteCallDefault", &enabled)) {
                        tsm::game::hooks::shout::SetShoutEditorEnabled(enabled);
                    }

                    if (enabled) {
                        bool rainbowMode = tsm::game::hooks::shout::IsRainbowModeEnabled();
                        if (ToggleCardIcon("Rainbow Mode", "Randomly change shout color", "UiMenuBuffRainbow", &rainbowMode)) {
                            tsm::game::hooks::shout::SetRainbowMode(rainbowMode);
                        }

                        static const char* kVoiceNames[] = {
                            "Default", "Bird", "Manta", "Whale", "Ghost", "Crab", "Jellyfish",
                            "Kizuna Ai", "Baby", "Night Bird", "Blue Bird", "Aurora [Star Pin]", "Manatee", "Manatee [Star Pin]", "Lighthorn", "Count"
                        };
                        const int kVoiceCount = 16;

                        int voiceTypeInt = static_cast<int>(tsm::game::hooks::shout::GetVoiceType());
                        if (voiceTypeInt < 0) voiceTypeInt = 0;
                        if (voiceTypeInt >= kVoiceCount) voiceTypeInt = 0;

                        if (SelectCardIcon("Voice Type", "Change your shout voice", "UiMenuSwitchMisc",
                                          &voiceTypeInt, kVoiceNames, kVoiceCount)) {
                            if (voiceTypeInt >= 0 && voiceTypeInt < kVoiceCount) {
                                tsm::game::hooks::shout::SetVoiceType(static_cast<std::uint8_t>(voiceTypeInt));
                                try {
                                    tsm::lua::helpers::SetVoice(std::to_string(voiceTypeInt));
                                } catch (...) {
                                }
                            }
                        }

                        CenterSeparator("Color Presets");
                        struct ColorPreset { const char* name; float r, g, b, a; const char* icon; };
                        static const ColorPreset kPresets[] = {
                            { "White",   1.0f, 1.0f, 1.0f, 1.0f, "UiMenuSystemCurrencyColorWhite" },
                            { "Red",     1.0f, 0.0f, 0.0f, 1.0f, "UiMenuSystemCurrencyColorRed" },
                            { "Green",   0.0f, 1.0f, 0.0f, 1.0f, "UiMenuSystemCurrencyColorGreen" },
                            { "Blue",    0.0f, 0.5f, 1.0f, 1.0f, "UiMenuSystemCurrencyColorBlue" },
                            { "Yellow",  1.0f, 1.0f, 0.0f, 1.0f, "UiMenuSystemCurrencyColorYellow" },
                            { "Magenta", 1.0f, 0.0f, 1.0f, 1.0f, "UiMenuSystemCurrencyColorMagenta" },
                            { "Cyan",    0.0f, 1.0f, 1.0f, 1.0f, "UiMenuSystemCurrencyColorCyan" },
                            { "Black",   0.0f, 0.0f, 0.0f, 1.0f, "UiMenuSystemCurrencyColorBlack" },
                        };

                        for (const auto& preset : kPresets) {
                            if (ButtonCard(preset.name, "Apply this color", preset.icon, "APPLY")) {
                                tsm::game::hooks::shout::SetShoutColor(preset.r, preset.g, preset.b, preset.a);
                                tsm::game::hooks::shout::SetRainbowMode(false);
                            }
                        }
                    }
                }
                EndPannableChild();                break; }
            case 6: {
                using namespace tsm::ui::widgets;

                static std::vector<tsm::utils::storage::SavedMessage> s_savedMessages;
                static bool s_messagesLoaded = false;
                static bool s_showCreateModal = false;
                static bool s_showEditModal = false;
                static int s_editMessageIdx = -1;
                static char s_messageName[128] = "";
                static char s_messageText[512] = "";

                auto hasDuplicateName = [&](const std::string& name, int excludeIdx = -1) {
                    for (size_t i = 0; i < s_savedMessages.size(); ++i) {
                        if ((int)i == excludeIdx) continue;
                        if (std::string(s_savedMessages[i].name) == name) {
                            return true;
                        }
                    }
                    return false;
                };

                auto previewText = [](const char* msg) -> std::string {
                    if (!msg) return std::string();
                    std::string s = msg;
                    const size_t kMax = 80;
                    if (s.size() > kMax) {
                        s.resize(kMax);
                        s += "...";
                    }
                    return s;
                };

                if (!s_messagesLoaded) {
                    tsm::utils::storage::LoadMessagesFromFile(s_savedMessages);
                    s_messagesLoaded = true;
                }

                if (BeginPannableChild("##messages_scroll")) {
                    CenterSeparator("Quick Save");
                    if (ButtonCard("Create New Message", "Save a custom avatar speak message", "UiMiscPlusMedium", "CREATE")) {
                        s_showCreateModal = true;
                        std::snprintf(s_messageName, sizeof(s_messageName), "Message %d", (int)s_savedMessages.size() + 1);
                        s_messageText[0] = '\0';
                    }

                    ImGui::Dummy(ImVec2(0, DP(8.0f)));
                    CenterSeparator("Saved Messages");

                    if (s_savedMessages.empty()) {
                        PaddedTextDisabled("No saved messages yet");
                    } else {
                        for (size_t i = 0; i < s_savedMessages.size(); ++i) {
                            const auto& msg = s_savedMessages[i];
                            std::string desc = previewText(msg.message);

                            ImGui::PushID((int)i);
                            auto result = EditableCard(msg.name, desc.empty() ? nullptr : desc.c_str(), "UiEmoteCallDefault");
                            if (result.main) {
                                tsm::lua::helpers::AvatarSpeak(msg.message);
                                tsm::ui::helpers::ShowToastSuccess("Message sent");
                            }
                            if (result.edit) {
                                s_editMessageIdx = (int)i;
                                s_showEditModal = true;
                                std::strncpy(s_messageName, msg.name, sizeof(s_messageName) - 1);
                                s_messageName[sizeof(s_messageName) - 1] = '\0';
                                std::strncpy(s_messageText, msg.message, sizeof(s_messageText) - 1);
                                s_messageText[sizeof(s_messageText) - 1] = '\0';
                            }
                            ImGui::PopID();
                        }
                    }
                }
                EndPannableChild();

                if (s_showCreateModal) {
                    ImGui::OpenPopup("##CreateMessageModal");
                    s_showCreateModal = false;
                }

                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(DP(320.0f), 0), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                if (ImGui::BeginPopupModal("##CreateMessageModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                    const char* title = tsm::ui::i18n::Tr("Create Message");
                    float title_w = ImGui::CalcTextSize(title).x;
                    float window_w = ImGui::GetContentRegionAvail().x;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                    ImGui::Text("%s", title);
                    ImGui::Separator();

                    ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                    ImGui::PushItemWidth(-1);
                    InputWithPlaceholder("##createname", s_messageName, sizeof(s_messageName), "Message name");
                    ImGui::PopItemWidth();

                    ImGui::Text("%s", tsm::ui::i18n::Tr("Message:"));
                    ImGui::PushItemWidth(-1);
                    InputWithPlaceholder("##createmsg", s_messageText, sizeof(s_messageText), "Say something");
                    ImGui::PopItemWidth();

                    ImGui::Dummy(ImVec2(0, DP(8.0f)));
                    float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;
                    if (AccentButton("Save", ImVec2(btn_w, 0))) {
                        if (s_messageName[0] == '\0' || s_messageText[0] == '\0') {
                            tsm::ui::helpers::ShowToastError("Name and message are required");
                        } else if (hasDuplicateName(std::string(s_messageName))) {
                            tsm::ui::helpers::ShowToastError("A message with this name already exists");
                        } else {
                            tsm::utils::storage::SavedMessage msg{};
                            std::strncpy(msg.name, s_messageName, sizeof(msg.name) - 1);
                            msg.name[sizeof(msg.name) - 1] = '\0';
                            std::strncpy(msg.message, s_messageText, sizeof(msg.message) - 1);
                            msg.message[sizeof(msg.message) - 1] = '\0';
                            s_savedMessages.push_back(msg);
                            tsm::utils::storage::SaveMessagesToFile(s_savedMessages);
                            tsm::ui::helpers::ShowToastSuccess("Message saved");
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::SameLine();
                    if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                if (s_showEditModal) {
                    ImGui::OpenPopup("##EditMessageModal");
                    s_showEditModal = false;
                }

                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(DP(320.0f), 0), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                if (ImGui::BeginPopupModal("##EditMessageModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                    if (s_editMessageIdx >= 0 && s_editMessageIdx < (int)s_savedMessages.size()) {
                        const char* title = tsm::ui::i18n::Tr("Edit Message");
                        float title_w = ImGui::CalcTextSize(title).x;
                        float window_w = ImGui::GetContentRegionAvail().x;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                        ImGui::Text("%s", title);
                        ImGui::Separator();

                        ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                        ImGui::PushItemWidth(-1);
                        InputWithPlaceholder("##editname", s_messageName, sizeof(s_messageName), "Message name");
                        ImGui::PopItemWidth();

                        ImGui::Text("%s", tsm::ui::i18n::Tr("Message:"));
                        ImGui::PushItemWidth(-1);
                        InputWithPlaceholder("##editmsg", s_messageText, sizeof(s_messageText), "Say something");
                        ImGui::PopItemWidth();

                        ImGui::Dummy(ImVec2(0, DP(8.0f)));
                        float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;
                        if (AccentButton("Save", ImVec2(btn_w, 0))) {
                            std::string newName = std::string(s_messageName);
                            if (newName.empty() || s_messageText[0] == '\0') {
                                tsm::ui::helpers::ShowToastError("Name and message are required");
                            } else if (hasDuplicateName(newName, s_editMessageIdx)) {
                                tsm::ui::helpers::ShowToastError("A message with this name already exists");
                            } else {
                                std::strncpy(s_savedMessages[s_editMessageIdx].name, s_messageName,
                                             sizeof(s_savedMessages[s_editMessageIdx].name) - 1);
                                s_savedMessages[s_editMessageIdx].name[sizeof(s_savedMessages[s_editMessageIdx].name) - 1] = '\0';
                                std::strncpy(s_savedMessages[s_editMessageIdx].message, s_messageText,
                                             sizeof(s_savedMessages[s_editMessageIdx].message) - 1);
                                s_savedMessages[s_editMessageIdx].message[sizeof(s_savedMessages[s_editMessageIdx].message) - 1] = '\0';
                                tsm::utils::storage::SaveMessagesToFile(s_savedMessages);
                                tsm::ui::helpers::ShowToastSuccess("Message updated");
                                ImGui::CloseCurrentPopup();
                            }
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
                        const char* delLabel = tsm::ui::i18n::Tr("Delete Message");
                        if (ImGui::Button(delLabel, ImVec2(-1, 0))) {
                            s_savedMessages.erase(s_savedMessages.begin() + s_editMessageIdx);
                            tsm::utils::storage::SaveMessagesToFile(s_savedMessages);
                            tsm::ui::helpers::ShowToastSuccess("Message deleted");
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopStyleColor(3);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                break; }
            case 7: {
                using namespace tsm::ui::widgets;
                if (BeginPannableChild("##misc_scroll")) {
                    CenterSeparator("Quick Actions");
                    if (ButtonCard("Social Gift", "Show white candle", "UiSocialOfferCandle2", "SHOW")) {
                        tsm::lua::helpers::SocialGift();
                    }
                    if (ButtonCard("Join Random", "Join a random game session", "UiMenuRandomGenerator", "JOIN")) {
                        tsm::lua::helpers::SocialRandomGame();
                    }
                    if (ButtonCard("Launch Firework", "Launch a firework effect", "UiOutfitPropFireworks", "LAUNCH")) {
                        tsm::lua::helpers::SocialFireworks();
                    }
                    if (ButtonCard("Resume Checkpoint", "Resume the latest checkpoint", "UiMenuCheckpoint", "RESUME")) {
                        tsm::lua::queue::Enqueue("CheckpointResumeLatest(game)");
                    }
                    if (ButtonCard("Start Snake Game", "Begin the debug snake mini-game", "UiMenuSnakeGame0", "START")) {
                        tsm::lua::queue::Enqueue("Debug_StartSnakeGame(game)");
                    }
                    if (ButtonCard("Add Snake Joint", "Add a segment to the snake", "UiMenuSnakeGame2", "ADD")) {
                        tsm::lua::queue::Enqueue("Debug_AddSnakeJoint(game)");
                    }

                    CenterSeparator("Wing Management");
                    if (ButtonCard("Charge Wings", "Charge wing energy", "UiMenuBattery03", "CHARGE")) {
                        tsm::lua::queue::Enqueue("Charge(game)");
                    }
                    if (ButtonCard("Drain Wings", "Drain wing energy", "UiMenuBattery", "DRAIN")) {
                        tsm::lua::queue::Enqueue("Drain(game)");
                    }
                    if (ButtonCard("Drop 5 Wings", "Reduce wing level by five", "UiMenuWingBuff", "DROP")) {
                        tsm::lua::queue::Enqueue("Wound(game)");
                    }
                    if (ButtonCard("Check Star Shrine", "Check if eden has reset", "UiMenuQuestHint", "CHECK")) {
                        tsm::lua::queue::Enqueue("TWingBuffDepositShrineResetHint()");
                    }

                    CenterSeparator("Avatar Status");
                    static const char* kPersonalityTypes[] = {
                        "None",
                        "Inspector",
                        "Supervisor",
                        "Mastermind",
                        "Marshall",
                        "Protector",
                        "Provider",
                        "Counselor",
                        "Teacher",
                        "Operator",
                        "Promoter",
                        "Architect",
                        "Inventor",
                        "Composer",
                        "Performer",
                        "Healer",
                        "Champion"
                    };

                    static const char* kPersonalityIcons[] = {
                        "UiOutfitNone",
                        "UiPersonalityGear",
                        "UiPersonalityStopwatch",
                        "UiPersonalityTelescope",
                        "UiPersonalityCrown",
                        "UiPersonalityNurseCap",
                        "UiPersonalityUmbrella",
                        "UiPersonalityCrystalBall",
                        "UiPersonalityShepherdStaff",
                        "UiPersonalityWrench",
                        "UiPersonalityGoggles",
                        "UiPersonalityWitchBottle",
                        "UiPersonalityMicrophone",
                        "UiPersonalityDesign01Brush",
                        "UiPersonalitySaxophone",
                        "UiPersonalityButterfly",
                        "UiPersonalitySpeaker"
                    };

                    static int s_personalityIndex = 0;
                    constexpr int kPersonalityCount = sizeof(kPersonalityTypes) / sizeof(kPersonalityTypes[0]);
                    if (s_personalityIndex < 0 || s_personalityIndex >= kPersonalityCount) {
                        s_personalityIndex = 0;
                    }
                    if (StringSliderCard("Set Personality Type",
                                         "Updates your current personality type",
                                         kPersonalityIcons[s_personalityIndex],
                                         kPersonalityTypes,
                                         kPersonalityCount,
                                         &s_personalityIndex)) {
                        if (s_personalityIndex == 0) {
                            tsm::lua::queue::Enqueue("ClearPersonalityType(game)");
                        } else {
                            tsm::lua::queue::Enqueue("TSetPersonalityType(" + std::to_string(s_personalityIndex - 1) + ")");
                        }
                    }
                    if (ButtonCard("Kill Avatar", "Trigger avatar death", "UiEmoteStanceInjured", "KILL")) {
                        tsm::lua::queue::Enqueue("KillAvatar(game)");
                    }
                    if (ButtonCard("Revive Avatar", "Revive from death state", "UiSocialOfferHeal3", "REVIVE")) {
                        tsm::lua::queue::Enqueue("ReviveAvatar(game)");
                    }
                    if (ButtonCard("Repair Elder Armor", "Bless the elder armor", "UiMenuShield", "REPAIR")) {
                        tsm::lua::queue::Enqueue("BlessShield(game)");
                    }
                    if (ButtonCard("Break Elder Armor", "Break the elder armor", "UiMenuBrokenShield", "BREAK")) {
                        tsm::lua::queue::Enqueue("BreakElderArmor(game)");
                    }
                    if (ButtonCard("Unlight Avatar", "Remove avatar light", "UiHudWoundedPlayer", "UNLIGHT")) {
                        tsm::lua::queue::Enqueue("BreakShield(game)");
                    }

                    CenterSeparator("Entity Interactions");
                    if (ButtonCard("Spawn Trampoline", "Spawn a trampoline at your position", "UiMenuBuffAntiGravity", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        std::ostringstream script;
                        script << "TProtoTrampoline(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
                        tsm::lua::queue::Enqueue(script.str());
                    }
                    if (ButtonCard("Spawn Bouncy Balloon", "Spawn a balloon at your position to bounce off", "UiMiscBubbles", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        std::ostringstream script;
                        script << "TBouncyBalloon(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
                        tsm::lua::queue::Enqueue(script.str());
                    }
                    if (ButtonCard("Spawn Farmers Market", "Spawn the farmers market at your position", "UiMenuBuffSpellShop", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::queue::Enqueue("TFarmersMarket({" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "})");
                    }
                    if (ButtonCard("Star Fall Shower", "Trigger star fall shower effect", "UiMenuStageFallingSparkle", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        std::ostringstream script;
                        script << "TStarFallShower(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
                        tsm::lua::queue::Enqueue(script.str());
                    }
                    if (ButtonCard("Spawn Wax Chunks", "Drop wax chunks at position", "UiMenuBuffCandle", "SPAWN")) {
                        tsm::lua::queue::Enqueue("SpawnWaxChunk(game)");
                    }
                    if (ButtonCard("Spawn Random Dye Wax", "Spawn a random dye wax at your position", "UiMenuDye", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::queue::Enqueue("TWaxPickupSpawnerZone({" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "})");
                    }
                    if (ButtonCard("Spawn Bubble Zone", "Create a bubble zone around you", "UiMiscBubbles", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::queue::Enqueue("THubbaBubbaZone({" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "}, 20.0)");
                    }
                    if (ButtonCard("Spawn Butterflies", "Spawn butterflies at your position", "UiEmoteButterfly", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::queue::Enqueue("TMusicalMoteTarget({" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "})");
                    }
                    if (ButtonCard("Spawn Giant Crab", "Summon a giant crab nearby", "UiEmoteCallCrab", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::queue::Enqueue("TSpawnCrabs(1, 0.0, {" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "}, 0.5)");
                    }
                    if (ButtonCard("Spawn Shardnado", "Summon a shardnado nearby", "UiMenuStageFallingRain", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::helpers::SpawnShardnado(pos);
                    }
                    if (ButtonCard("Spawn Shard", "Spawn a shard at your position", "UiMenuStageFallingRain", "SPAWN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::helpers::ManualShardFall(pos);
                    }

                    CenterSeparator("Companions");
                    if (ButtonCard("Summon Bird Whisperer", "Summon the bird whisperer", "UiEmoteAP28AirplaneArms", "SUMMON")) {
                        tsm::lua::queue::Enqueue("ChooseAirplanearms(game)");
                    }

                    if (ButtonCard("Summon Butterfly Whisperer", "Summon the butterfly whisperer", "UiEmoteAP28Dizzy", "SUMMON")) {
                        tsm::lua::queue::Enqueue("ChooseDizzy(game)");
                    }

                    if (ButtonCard("Summon Manta Whisperer", "Summon the manta whisperer", "UiEmoteAP28Flag", "SUMMON")) {
                        tsm::lua::queue::Enqueue("ChooseFlag(game)");
                    }

                    if (ButtonCard("Summon Jelly Whisperer", "Summon the jelly whisperer", "UiEmoteAP28JellyDance", "SUMMON")) {
                        tsm::lua::queue::Enqueue("ChooseJellydance(game)");
                    }

                    if (ButtonCard("Summon Bellmaker", "Summon the bellmaker", "UiEmoteCallLighthorn", "SUMMON")) {
                        tsm::lua::queue::Enqueue("ChooseLighthorn(game)");
                    }
                    if (ButtonCard("Dismiss Companion", "Dismiss your current companion", "UiSocialDeleteFriend", "DISMISS")) {
                        tsm::lua::helpers::AddFloatToStat("spend_companion_time", 30.0f);
                    }

                    CenterSeparator("Expressions");
                    if (ButtonCard("Big Shout", "Perform a powerful shout", "UiEmoteCallDefault", "SHOUT")) {
                        tsm::lua::queue::Enqueue("ShoutBig(game)");
                    }
                    if (ButtonCard("Shout Cry", "Shout with cry reaction", "UiEmoteCallDefault", "SHOUT")) {
                        tsm::lua::helpers::LocalDoAudienceReact(0.5f, 1);
                    }
                    if (ButtonCard("Shout Heart", "Shout with heart reaction", "UiEmoteCallDefault", "SHOUT")) {
                        tsm::lua::helpers::LocalDoAudienceReact(0.5f, 3);
                    }
                    if (ButtonCard("Shout Shocked", "Shout with shocked reaction", "UiEmoteCallDefault", "SHOUT")) {
                        tsm::lua::helpers::LocalDoAudienceReact(0.5f, 2);
                    }

                    CenterSeparator("Visual Effects");
                    if (ButtonCard("Purchase Effect", "Play purchase VFX", "UiMenuShop", "PLAY")) {
                        tsm::lua::queue::Enqueue("TPlayPurchaseFX()");
                    }
                    if (ButtonCard("Level Up Effect", "Play level-up VFX", "UiMenuStageFallingSparkle", "PLAY")) {
                        tsm::lua::queue::Enqueue("LevelUp(game)");
                    }
                    if (ButtonCard("Star Bling Effect", "Play star bling VFX", "UiMenuStageFallingSparkle", "PLAY")) {
                        tsm::lua::queue::Enqueue("DEBUG_TestStarBlingFx(game)");
                    }
                    if (ButtonCard("Explosion Effect", "Play explosion VFX", "UiMenuStageFallingSparkle", "PLAY")) {
                        tsm::lua::queue::Enqueue("DEBUG_TestExplosionFx(game)");
                    }
                    if (ButtonCard("Cape Upgrade Effect", "Play cape upgrade VFX", "UiSharedSpaceVolumeUpgrade", "PLAY")) {
                        tsm::lua::queue::Enqueue("CapeUpgrade(game)");
                    }
                    if (ButtonCard("Enter Micro Mode", "Shrink your avatar temporarily", "UiMenuBlur", "ENTER")) {
                        tsm::lua::helpers::SetMicroMode();
                    }
                    if (ButtonCard("Toggle Antler Helm Orb", "Toggle antler helm orb", "UiOutfitFaceAP28AntlerHelm", "TOGGLE")) {
                        tsm::lua::queue::Enqueue("TSocialToggleAntlerHelmOrb()");
                    }
                    if (ButtonCard("Browse Emitters", "Browse particle emitters", "UiMenuStageFallingSparkle", "BROWSE")) {
                        tsm::lua::queue::Enqueue("BrowseEmitters(game)");
                    }
                    if (ButtonCard("Next Page", "Go to next page of emitters", "UiHudChevron", "NEXT")) {
                        tsm::lua::queue::Enqueue("NextPage(game)");
                    }

                    CenterSeparator("Menus");
                    if (ButtonCard("Grappling Hook", "Activate grapple hook to pull to surfaces", "UiSocialTeleport", "ACTIVATE")) {
                        tsm::lua::queue::Enqueue("TSocialGrapplingHookMenu()");
                    }
                    if (ButtonCard("Graphics Settings", "Open graphics settings menu", "UiMenuGraphicsSettings", "OPEN")) {
                        tsm::lua::queue::Enqueue("TGraphicsSettingsMenu()");
                    }
                    if (ButtonCard("Audience Reactions", "Open audience reaction settings", "UiMenuReactionShock", "OPEN")) {
                        tsm::lua::helpers::SetAudienceSettings(true, false, false);
                    }
                    if (ButtonCard("Firework Prop", "Open fireworks menu", "UiOutfitPropFireworks", "OPEN")) {
                        tsm::lua::helpers::SocialFireworksMenu();
                    }
                    if (ButtonCard("Social Buttons", "Open social menu", "UiMenuBadgeChatLog", "OPEN")) {
                        tsm::lua::helpers::OpenMenu("OpenSocialMenu");
                    }
                    if (ButtonCard("Star Pins", "Open star pins menu", "UiMenuStarScan", "OPEN")) {
                        tsm::lua::queue::Enqueue("OpenStarMenu(game)");
                    }
                    if (ButtonCard("Dye Crafting", "Open dye crafting menu", "UiMenuDyeCrafting", "OPEN")) {
                        tsm::lua::queue::Enqueue("OpenCraftingMenu(game)");
                    }
                    if (ButtonCard("Outfit Debug Menu", "Open outfit debug tools", "UiMenuEdit", "OPEN")) {
                        tsm::lua::queue::Enqueue("DebugBodyDef(game)");
                    }
                    if (ButtonCard("Season Pass", "Open season pass commerce menu", "UiMenuSeasonCandle", "OPEN")) {
                        tsm::lua::helpers::OpenMenu("OpenSeasonPassCommerceMenu");
                    }
                    if (ButtonCard("Nest Directory", "Open nest directory near you", "UiMenuConstellationHome", "OPEN")) {
                        vec3 pos = tsm::game::api::LocalAvatarPosition();
                        tsm::lua::queue::Enqueue("THomeDirectoryButton({" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "})");
                    }
                }
                EndPannableChild();
                break; }
        }
        EndCard();
    }
}

}}}
