#include <ui/panels/MusicPanel.h>
#include <ui/overlays/MusicPlaybackOverlay.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/widgets/WidgetInternal.h>
#include <ui/helpers/SubTabRenderer.h>
#include <ui/helpers/Toast.h>
#include <imgui/imgui.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <game/interop/LuaHelpers.h>
#include <game/hooks/HookManager.h>
#include <game/hooks/MusicKeyHook.h>
#include <game/hooks/MusicRecordHook.h>
#include <utils/storage/MusicSheetStorage.h>
#include <features/music/MusicSheetPlayer.h>
#include <fileselector/fileselector.h>
#include <algorithm>
#include <cstring>
#include <atomic>
#include <thread>
#include <mutex>
#include <state/ModState.h>
#include <cctype>
#include <set>
#include <unordered_map>
#include <fstream>

namespace tsm {
    namespace ui {
        namespace tabs {

            static bool s_importSuccess = false;
            static bool s_importError = false;
            static std::string s_importErrorMsg;
            static bool s_shouldRefreshSheets = false;

            static std::atomic<bool> s_sheetsLoading{ false };
            static std::mutex s_sheetsMutex;
            static std::vector<tsm::utils::storage::MusicSheet> s_loadedSheets;

            static std::unordered_map<std::string, std::string> s_sheetCategories;
            static std::set<std::string> s_persistedFolders;
            static bool s_categoriesLoaded = false;

            static std::string GetCategoriesFilePath() {
                return tsm::utils::storage::GetMusicSheetsDir() + "/_categories.json";
            }

            static void LoadCategories() {
                s_sheetCategories.clear();
                s_persistedFolders.clear();
                std::ifstream ifs(GetCategoriesFilePath());
                if (ifs.is_open()) {
                    try {
                        nlohmann::json j;
                        ifs >> j;
                        if (j.contains("_folders") && j["_folders"].is_array()) {
                            for (const auto& f : j["_folders"])
                                if (f.is_string()) s_persistedFolders.insert(f.get<std::string>());
                        }
                        for (auto& [key, val] : j.items()) {
                            if (key == "_folders") continue;
                            if (val.is_string()) s_sheetCategories[key] = val.get<std::string>();
                        }
                    }
                    catch (...) {}
                }
                s_categoriesLoaded = true;
            }

            static void SaveCategories() {
                nlohmann::json j = nlohmann::json::object();
                nlohmann::json folders = nlohmann::json::array();
                for (const auto& f : s_persistedFolders) folders.push_back(f);
                j["_folders"] = folders;
                for (const auto& [filename, category] : s_sheetCategories)
                    j[filename] = category;
                std::ofstream ofs(GetCategoriesFilePath());
                if (ofs.is_open()) ofs << j.dump(2);
            }

            void InvalidateCategoryCache() {
                LoadCategories();
                s_shouldRefreshSheets = true;
            }


            static const std::string& GetSheetCategory(const std::string& filename) {
                static const std::string kUncategorized = "Uncategorized";
                auto it = s_sheetCategories.find(filename);
                return (it != s_sheetCategories.end() && !it->second.empty()) ? it->second : kUncategorized;
            }

            static void SetSheetCategory(const std::string& filename, const std::string& category) {
                if (category.empty() || category == "Uncategorized")
                    s_sheetCategories.erase(filename);
                else
                    s_sheetCategories[filename] = category;
                SaveCategories();
            }

            static std::vector<std::string> GetAllCategoryNames() {
                std::set<std::string> seen(s_persistedFolders.begin(), s_persistedFolders.end());
                for (const auto& [_, cat] : s_sheetCategories)
                    if (!cat.empty() && cat != "Uncategorized") seen.insert(cat);
                return std::vector<std::string>(seen.begin(), seen.end());
            }

            static bool CategoryNameValid(const char* name) {
                if (!name || name[0] == '\0') return false;
                for (const char* p = name; *p; ++p) {
                    unsigned char c = static_cast<unsigned char>(*p);
                    if (!std::isalnum(c) && c != ' ' && c != '_' && c != '-') return false;
                }
                return true;
            }

            static void LoadSheetsAsync() {
                if (s_sheetsLoading.exchange(true)) return;

                std::thread([]() {
                    auto sheets = tsm::utils::storage::LoadAllMusicSheets();

                    std::lock_guard<std::mutex> lock(s_sheetsMutex);
                    s_loadedSheets = std::move(sheets);
                    s_sheetsLoading.store(false);
                    }).detach();
            }

            static void OnFileSelected(int fd) {
                std::string error;
                if (tsm::utils::storage::ImportMusicSheetFromFd(fd, error)) {
                    s_importSuccess = true;
                    s_shouldRefreshSheets = true;
                }
                else {
                    s_importError = true;
                    s_importErrorMsg = error;
                }
            }

            static nlohmann::json BuildRecordedSongJson(
                const std::vector<tsm::game::hooks::musicrecord::NoteEvent>& notes,
                const std::string& name,
                int bpm) {
                nlohmann::json song;
                song["name"] = name;
                song["author"] = "TSM Recorder";
                song["transcribedBy"] = "TSM Recorder";
                song["isComposed"] = true;
                song["bpm"] = bpm;
                song["bitsPerPage"] = 16;
                song["pitchLevel"] = 0;
                song["isEncrypted"] = false;

                song["songNotes"] = nlohmann::json::array();
                for (const auto& n : notes) {
                    song["songNotes"].push_back({
                        {"time", n.time},
                        {"key", std::string("1Key") + std::to_string(n.keyIndex)}
                        });
                }

                nlohmann::json wrapped = nlohmann::json::array();
                wrapped.push_back(song);
                return wrapped;
            }

            static std::string SanitizeSheetName(const char* input) {
                std::string result;
                if (!input) return result;
                for (const char* p = input; *p; ++p) {
                    unsigned char c = static_cast<unsigned char>(*p);
                    if (std::isalnum(c)) {
                        result.push_back(static_cast<char>(c));
                    }
                    else if (c == ' ') {
                        result.push_back(' ');
                    }
                    else if (c == '_' || c == '-') {
                        result.push_back('_');
                    }
                }
                while (!result.empty() && result.back() == '_') {
                    result.pop_back();
                }
                if (result.empty()) {
                    result = "recorded_song";
                }
                return result;
            }

            void DrawMusicTab()
            {
                using namespace tsm::ui::widgets;
                using namespace tsm::ui::helpers;

                tsm::game::memory::EnsureInitialized();

                static int s_musicSubIndex = 0;
                static float s_musicVolume = 1.0f;

                static const char* kSubIcons[] = {
                    "UiMiscMusicNote",
                    "UiMenuSoundPreviewOn",
                    "UiMusicFlat",
                    "UiMenuSheetMusicD",
                };
                constexpr int sub_count = 4;

                DrawSubTabs(kSubIcons, sub_count, s_musicSubIndex);

                if (BeginCard("##music_card", 8.0f, false)) {
                    switch (s_musicSubIndex) {
                    case 0: {
                        if (BeginPannableChild("##features_scroll")) {

                            CenterSeparator("Instrument Features");

                            static bool s_enableReverb = false;
                            if (ToggleCardIcon("Enable Reverb", "Adds reverb effect to instruments", "UiMusicSustain", &s_enableReverb)) {
                                tsm::game::memory::WriteByte(tsm::game::Offsets::kEnableReverb, s_enableReverb ? 1 : 0);
                            }

                            static bool s_autoPlaySheets = false;
                            if (ToggleCardIcon("Auto Play Sheets", "Automatically plays sheet music", "UiMenuSheetMusicD", &s_autoPlaySheets)) {
                                tsm::game::memory::WriteByte(tsm::game::Offsets::kInstrumentAutoPlaySheets, s_autoPlaySheets ? 1 : 0);
                            }

                            static bool s_easyMode = false;
                            if (ToggleCardIcon("Easy Mode", "Enables easy mode for instruments", "UiEmoteStanceHappy", &s_easyMode)) {
                                tsm::game::memory::WriteByte(tsm::game::Offsets::kInstrumentEasyMode, s_easyMode ? 1 : 0);
                            }

                            static bool s_radialLayout = false;
                            if (ToggleCardIcon("Radial Layout", "Switch to the radial key layout", "UiButtonCirclePS", &s_radialLayout)) {
                                tsm::game::memory::WriteByte(tsm::game::Offsets::kInstrumentRadialLayout, s_radialLayout ? 1 : 0);
                            }
                        }
                        EndPannableChild();
                        break;
                    }

                    case 1: {
                        if (BeginPannableChild("##sounds_scroll")) {

                            CenterSeparator("Sound Effects");

                            static float s_soundVolume = 1.0f;
                            if (FloatCardIcon("Volume", "Sound effect volume", "UiMenuSoundPreviewOn", &s_soundVolume, 0.0f, 1.0f, 1.0f, "%.2f")) {
                                s_soundVolume = std::clamp(s_soundVolume, 0.0f, 1.0f);
                            }

                            static const char* kSoundNames[] = {
                                "error",
                                "AP15RedCrystalBurst",
                                "AP15RedCrystalCrash_Large",
                                "AP15RedCrystalCrash_Small",
                                "AP15RedCrystalTravel",
                                "AP15RedCrystalWhoosh_Small",
                                "AP15RedCrystal_Shock",
                                "AP15RedExplosionDistant",
                                "AP15RedPreExplosionDistant",
                                "AP15RedRumble",
                                "AP15RedShardEnergy",
                                "AP15Shardnado",
                                "AP16ConcertCheerOneshot2D",
                                "AP24BellJostleLP",
                                "AP24ClockChimeSingle",
                                "AP24ClockChimeTune",
                                "AP24ClockTickTock",
                                "AP24ClockWhirr",
                                "AP25DyeCollect",
                                "AP25DyeGather",
                                "AP25DyeRelease",
                                "AP27FQ01TakeToy",
                                "AP28AntlerHelmOff",
                                "AP28AntlerHelmOn",
                                "AliceFlipFailure",
                                "AliceFlipSuccessNoPuff",
                                "AlicePortalOpen",
                                "AlicePortalShrink",
                                "Anniversary2025_AirBubbleEnter",
                                "AnniversaryCannon",
                                "AuroraOrbLight",
                                "AvatarFlame",
                                "BellActivate",
                                "BlessingGain",
                                "BoomHit",
                                "BoostLiftOrbit",
                                "BoundaryBlast",
                                "BreathChargeLP",
                                "ButterflyLift",
                                "Buy",
                                "Camera",
                                "CameraExposure",
                                "CameraFocus",
                                "CameraFocusPin",
                                "CameraIndicator",
                                "CameraPictureNormal",
                                "CameraPictureRemote",
                                "CameraZoom",
                                "CastSpell",
                                "CauldronPropLP",
                                "ChangeColor",
                                "ChangeHair",
                                "CloudOutHop",
                                "ConstellationGateLP",
                                "ConstellationGateOpen",
                                "ConstellationGateUnlock",
                                "Consumable",
                                "CountDownTock",
                                "CreatureFragmentFlight",
                                "CreatureFragmentWander",
                                "CurrencyLP",
                                "DailyBonus",
                                "Damage8bit",
                                "DarkCreatureAlert",
                                "DarkStoneMeltLP",
                                "DarkStoneMeltStart",
                                "DarkStoneRegeneration",
                                "DarkVineLeavesShake",
                                "DarkVineMeltLP",
                                "DarkVineRegenerationLP",
                                "DawnBell3",
                                "DoCChesetMove",
                                "DoFRace_Goal",
                                "DoM2025_PressurePlateOff",
                                "DoM2025_PressurePlateOn",
                                "DoM2025_TimerLP",
                                "DoM2025_TimerStop",
                                "DoMUnlock",
                                "DoNatureWaterPlantPluck",
                                "DoTreasureChestDig",
                                "DoTreasureCollect",
                                "DuskCreatureAttack",
                                "DuskCreatureHitNPC",
                                "DuskCreatureIdleLP",
                                "DuskCreatureReveal",
                                "DuskCreatureWindup",
                                "DynamicRain",
                                "EelReward",
                                "ElderMantaMoveLP2D",
                                "ElderShieldBreak",
                                "ElderShieldEquip",
                                "ElderShieldHit",
                                "ElderShieldIconBreak",
                                "ElderShieldRestore",
                                "EmberCarry",
                                "EmberCharge",
                                "EmberSpawn",
                                "EmoteDepositL",
                                "EnergyFull",
                                "EnergyNew",
                                "EnergySpend",
                                "EnergyballHit",
                                "EngineFlameLP",
                                "EngineIdleLP",
                                "EquipMask",
                                "FireworkBurst8bit",
                                "FireworkBurstSmall",
                                "FireworkComet",
                                "FireworkHorsetailBang",
                                "Flourish",
                                "Flourish2",
                                "FragmentCoreDisappear",
                                "FragmentCoreLoop",
                                "FragmentFlight",
                                "FragmentGet",
                                "FragmentWander",
                                "FragmentsConvergence",
                                "FuseLP",
                                "GainedFlame",
                                "GainedFlight",
                                "GeneralMuralSmall",
                                "HackySackBounce",
                                "HackySackHitGround",
                                "HackySackKickUp",
                                "HandHoldingHeartBeatLP",
                                "HeartCount",
                                "HeartbeatSlow",
                                "HideSeekCountDownStart",
                                "HideSeekStart",
                                "HideSeekWin",
                                "Hint2D",
                                "HuskBreak",
                                "HuskBurnLP",
                                "HuskBurnStart",
                                "HuskCrack",
                                "ImportantButtonDetach",
                                "ImportantButtonTouch",
                                "JellyLight",
                                "JellyRide",
                                "JumpToFlight",
                                "KingTarget",
                                "LastWingbuffLost",
                                "LevelGate_opn",
                                "LevelUp",
                                "LightCircleGoal",
                                "LightCobweb",
                                "LightMote",
                                "LightPop",
                                "LightSeekLightLoop",
                                "LockonGuide",
                                "LockonIcon",
                                "LostFlame",
                                "LoveBoatAppear",
                                "LoveBoatLP",
                                "LurkerBigReturn",
                                "LurkerMovingLP",
                                "LurkerPeeking",
                                "LurkerSmallReturn",
                                "LurkerSurprised",
                                "LurkerThrashing",
                                "LurkerThrowing",
                                "LurkerTracking",
                                "MainstreetTowerBellDistant",
                                "MantaFlip",
                                "MantaFloatIdleLP",
                                "MemoryPoseDisappear",
                                "MessageBoatLanding",
                                "MeteorHitAvatar",
                                "MicroDarkAccumulate",
                                "MicroDarkCleanse",
                                "MoonlightLanternBell",
                                "MusicShellLP",
                                "ObjectRemove",
                                "ObjectSelect",
                                "OrbitEndAmbiLP",
                                "OrbitPunchRock",
                                "PayHeart",
                                "PinWheelRotate",
                                "PrayProgressed",
                                "PremiumCandle",
                                "ProjectionMemoryLP",
                                "PurchaseFailure",
                                "PurchaseLight",
                                "PurchaseOk",
                                "RainRipplesLP",
                                "ScepterWandFireworkFuse",
                                "ScepterWandFireworkShoot",
                                "SealBurn",
                                "SearchlightFind",
                                "SearchlightLP",
                                "ShopMenuOpn",
                                "ShoutFragment",
                                "ShoutLanternAudience",
                                "ShroomLight",
                                "ShroomRide",
                                "SlideBase2GlacierLocal",
                                "SlideBase2Local",
                                "SlideBase2LocalInitial",
                                "SlideBase2SurfLocal",
                                "SlideCurve",
                                "SnowGlobeLP",
                                "SnowballHit",
                                "SnowballHitBoard",
                                "SoccerScoreExplosion1-2",
                                "SoccerScoreWhistle1-2",
                                "SparklerProLP",
                                "Spell_CrabTrickIn",
                                "Spell_CrabTrickOut",
                                "Spinning",
                                "SpiritDayAppear",
                                "SpiritDayDisappear",
                                "SpiritFlyToTemple",
                                "SplashOutShallow",
                                "StarLampLP",
                                "StarLampTouch",
                                "StartVideo",
                                "StormExplosion",
                                "StreamSummon",
                                "SuffocationBubble",
                                "SurfTurningSplash",
                                "SwimDeep",
                                "TR_BowTieOrnament",
                                "TR_Hourglass_Reset",
                                "TR_TV_Shake",
                                "TR_TV_Transform",
                                "TableAppear",
                                "TableDisappear",
                                "TapBeamHint",
                                "TeaKettleSteam",
                                "Toggle",
                                "TouchFX",
                                "TrustCompanionToOrb",
                                "TrustCompanionToSpirit",
                                "TurnDial",
                                "TutorialCorrect",
                                "TutorialFinish",
                                "UICapeLevelUp",
                                "UICollectibleDeposit",
                                "UIConstellationOpen",
                                "UIDailyQuestProgress",
                                "UIMapEnter01-1",
                                "UIMapEnter01-2",
                                "UIMapEnter02",
                                "UIMapExit",
                                "UIMapLandmarkReveal",
                                "UIMapTrailDraw",
                                "UIMapTrailStop",
                                "UIQuestProgressMax",
                                "UIQuestStart",
                                "UISharedSpaceEnter",
                                "UISharedSpaceExit",
                                "UI_ChevronDeplete",
                                "UI_DailyLightReset",
                                "UI_MessageOpen",
                                "UiMenuGate",
                                "UnequipHorn",
                                "UnequipNeck",
                                "UnlockNew",
                                "UnmeltableDarkstone",
                                "UnseenSpiritAppear",
                                "VideoStop",
                                "WallHitCymbalSup",
                                "WaxFull",
                                "WaxGain2D",
                                "WhiffJump",
                                "WindShield",
                                "WingbuffDeposit",
                                "WingbuffDrop",
                                "WingbuffIdleLP",
                                "WingbuffPickup",
                                "ZeroG",
                                "anim_catAggro",
                                "auraclosetone",
                                "auraopentone",
                                "beacon",
                                "bell",
                                "blazearrive",
                                "blazedepart",
                                "blazeon",
                                "bubbletouch",
                                "bulletlaunch",
                                "burnout",
                                "cancel",
                                "crabAttack",
                                "crabAttackLocal",
                                "crabBurrow",
                                "crabDamage",
                                "crabHit",
                                "crabWindup",
                                "crabWindupLocal",
                                "crawlerstepLocalLP",
                                "duskstart2lockon",
                                "explode",
                                "fireextinguish",
                                "fireloopNew",
                                "firestart",
                                "firestart8bit",
                                "flapLocalNew",
                                "galleryview",
                                "keydrag",
                                "keysmash",
                                "keywhoosh",
                                "menuOn",
                                "menuon",
                                "menutouch",
                                "noticeGeneral",
                                "noticeHappy",
                                "noticeNewMessage",
                                "orbitChild",
                                "orbitChildVM1",
                                "podsmash",
                                "raindrop",
                                "rainpossess",
                                "social_hide_n_seek_going",
                                "social_hide_n_seek_starting",
                                "social_hide_n_seek_starting_seeking",
                                "tapDebugClick",
                                "turretlight",
                                "umbrella",
                                "whippastObject",
                                "wsh_ascent",
                                "wsh_descent",
                                "wsh_freefall"
                            };

                            const int soundCount = static_cast<int>(sizeof(kSoundNames) / sizeof(kSoundNames[0]));
                            for (int i = 0; i < soundCount; ++i) {
                                if (StandardButton(kSoundNames[i])) {
                                    tsm::lua::helpers::PlaySound2D(kSoundNames[i], s_soundVolume);
                                }
                            }
                        }
                        EndPannableChild();
                        break;
                    }

                    case 2: {
                        if (BeginPannableChild("##player_scroll")) {

                            CenterSeparator("Music Player");

                            if (FloatCardIcon("Volume", "Music playback volume", "UiMenuSoundPreviewOn", &s_musicVolume, 0.0f, 1.0f, 1.0f, "%.2f")) {
                                s_musicVolume = std::clamp(s_musicVolume, 0.0f, 1.0f);
                            }

                            if (StandardButton("Stop Music")) {
                                tsm::lua::helpers::PlayMusic("", true, 0.0f);
                                tsm::lua::helpers::PlayMusic("", false, 0.0f);
                            }

                            CenterSeparator("Aurora");

                            struct SongInfo {
                                const char* displayName;
                                const char* musicId;
                            };

                            static const SongInfo auroraGongs[] = {
                                {"Star Calm", "starcalm"},
                                {"All is Soft Inside", "allissoftinside"},
                                {"Exhale Inhale", "exhaleinhale"},
                                {"Queendom", "queendom"},
                                {"Runaway", "runaway"},
                                {"The Seed", "theseed"},
                                {"The Seed Concert", "theseedconcert"},
                                {"Through the Eyes of a Child", "throughtheeyesofachild"},
                                {"Warrior", "warrior"}
                            };

                            for (const auto& song : auroraGongs) {
                                if (StandardButton(song.displayName)) {
                                    tsm::lua::helpers::PlayMusic(song.musicId, true, s_musicVolume);
                                }
                            }

                            CenterSeparator("Sky");

                            static const SongInfo skySongs[] = {
                                {"Aviary Village", "mainstreetBGM"},
                                {"Rain Transform", "raintransform"},
                                {"Dawn Boat", "dawnboat"},
                                {"Quest Funny", "questfunny"},
                                {"Quest Funny 2", "questfunny2"},
                                {"Quest Funny 3", "questfunny3"},
                                {"Quest Mystery", "questmystery"},
                                {"Quest Mystery 2", "questmystery2"},
                                {"Sky Memorial", "ap11theme3"}
                            };

                            for (const auto& song : skySongs) {
                                if (StandardButton(song.displayName)) {
                                    tsm::lua::helpers::PlayMusic(song.musicId, true, s_musicVolume);
                                }
                            }
                        }
                        EndPannableChild();
                        break;
                    }

                    case 3: {
                        const auto& recordedKeys = tsm::game::hooks::musickey::GetRecordedKeys();
                        int keyCount = tsm::game::hooks::musickey::GetRecordedKeyCount();
                        auto& player = tsm::features::music::MusicSheetPlayer::Get();
                        const auto& recordedNotes = tsm::game::hooks::musicrecord::GetRecordedNotes();
                        static bool s_showRecordingJson = false;
                        static bool s_showExportModal = false;
                        static char s_exportName[128] = "Recorded Song";
                        static int s_exportBpm = 220;
                        static int s_customSubIndex = 0;

                        static std::vector<tsm::utils::storage::MusicSheet> s_musicSheets;
                        static bool s_sheetsLoaded = false;
                        static int s_selectedSheetIndex = -1;
                        static char s_searchBuffer[128] = "";
                        static bool s_showEditModal = false;
                        static int s_editSheetIndex = -1;
                        static char s_newSheetName[128] = "";

                        static std::string s_activeCategory;
                        static bool s_showNewCategoryModal = false;
                        static char s_newCategoryNameBuf[64] = "";
                        static bool s_showManageCategoryModal = false;
                        static std::string s_manageCategoryName;
                        static char s_renameCategoryBuf[64] = "";
                        static bool s_showMoveModal = false;
                        static int  s_moveSheetIndex = -1;

                        if (!s_sheetsLoaded) {
                            LoadSheetsAsync();
                            s_sheetsLoaded = true;
                        }

                        if (!s_categoriesLoaded) {
                            LoadCategories();
                        }

                        if (!s_sheetsLoading.load()) {
                            std::lock_guard<std::mutex> lock(s_sheetsMutex);
                            s_musicSheets.clear();
                            for (const auto& sheet : s_loadedSheets) {
                                if (!sheet.filename.empty() && sheet.filename[0] != '_')
                                    s_musicSheets.push_back(sheet);
                            }
                        }

                        if (s_shouldRefreshSheets) {
                            LoadSheetsAsync();
                            s_shouldRefreshSheets = false;
                        }

                        if (s_importSuccess) {
                            ShowToastSuccess("Music sheet imported successfully!");
                            s_importSuccess = false;
                        }

                        if (s_importError) {
                            ShowToastError(s_importErrorMsg);
                            s_importError = false;
                        }

                        bool hasValidSheet = s_selectedSheetIndex >= 0 &&
                            s_selectedSheetIndex < static_cast<int>(s_musicSheets.size());

                        const char* kCustomSubIcons[] = {
                            "UiMenuSheetMusicD",
                            "UiMenuMemoryRecord"
                        };

                        constexpr int custom_sub_count = 2;

                        const float base_d = DP(44.0f);
                        const ImVec4 acc = tsm::ui::GetAccentColor();
                        const ImU32  bg_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
                        const ImU32  brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
                        const ImU32  bg_nrm = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.10f));
                        const ImU32  brd_nrm = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.18f));
                        float base_gap = ImGui::GetStyle().ItemSpacing.x;

                        ImGui::Dummy(ImVec2(0, DP(4.0f)));
                        {
                            float slot_d_sub = base_d * 0.75f;
                            float icon_sz_sub = slot_d_sub * 0.62f;
                            float gap2 = base_gap;
                            float cont_w2 = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
                            float row_w2 = custom_sub_count * slot_d_sub + (custom_sub_count - 1) * gap2;
                            float left_pad2 = std::max(0.0f, (cont_w2 - row_w2) * 0.5f);

                            ImVec2 start2 = ImGui::GetCursorPos();
                            for (int i = 0; i < custom_sub_count; ++i) {
                                ImGui::SetCursorPos(ImVec2(start2.x + left_pad2 + i * (slot_d_sub + gap2), start2.y));
                                ImGui::PushID(i + 5000);
                                ImVec2 p2 = ImGui::GetCursorScreenPos();
                                ImGui::InvisibleButton("##musicsubico", ImVec2(slot_d_sub, slot_d_sub));
                                bool clicked2 = ImGui::IsItemClicked();

                                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                                float r2 = slot_d_sub * 0.5f;
                                ImVec2 c2 = ImVec2(p2.x + r2, p2.y + r2);
                                dl2->AddCircleFilled(c2, r2, (s_customSubIndex == i) ? bg_sel : bg_nrm, 48);
                                dl2->AddCircle(c2, r2, (s_customSubIndex == i) ? brd_sel : brd_nrm, 48, 1.0f);

                                ImVec2 icon_pos2 = ImVec2(c2.x - icon_sz_sub * 0.5f, c2.y - icon_sz_sub * 0.5f);
                                ImGui::SetCursorScreenPos(icon_pos2);
                                Icon(kCustomSubIcons[i], icon_sz_sub, ImVec4(1, 1, 1, 1));

                                if (clicked2) s_customSubIndex = i;
                                ImGui::PopID();
                            }
                        }
                        ImGui::Dummy(ImVec2(0, DP(6.0f)));

                        if (s_customSubIndex == 1) {
                            CenterSeparator("Recorder");

                            bool isRecordingSession = tsm::game::hooks::musicrecord::IsRecording();

                            if (isRecordingSession) {
                                if (ButtonCard("Stop Recording", "Stop capturing notes from shared memories", "UiMiscBoxButton")) {
                                    tsm::game::hooks::musicrecord::StopRecording();
                                }
                            }
                            else {
                                if (ButtonCard("Start Recording", "Start capturing notes from shared memories", "UiMiscCircle", "START")) {
                                    tsm::game::hooks::musicrecord::StartRecording();
                                    tsm::game::hooks::musicrecord::ClearRecording();
                                }
                            }

                            if (ButtonCard("Reset Recording", "Clear current recording and stop", "UiMenuSheetMusicD", "RESET")) {
                                tsm::game::hooks::musicrecord::StopRecording();
                                tsm::game::hooks::musicrecord::ClearRecording();
                            }

                            bool canExport = !recordedNotes.empty();
                            if (!canExport) ImGui::BeginDisabled();
                            if (ButtonCard("Export Recording", "Create a custom sheet from this recording", "UiMenuSheetMusicD")) {
                                std::snprintf(s_exportName, sizeof(s_exportName), "Recording_%d", (int)recordedNotes.size());
                                s_exportBpm = 220;
                                s_showExportModal = true;
                            }
                            if (!canExport) ImGui::EndDisabled();

                            ToggleCardIcon("Show Recording Details", "View details for this recording", "UiMenuSheetMusicD", &s_showRecordingJson);
                            if (s_showRecordingJson) {
                                float json_child_h = ImGui::GetContentRegionAvail().y;
                                if (json_child_h < 0.0f) json_child_h = 0.0f;
                                if (BeginPannableChild("##recording_json", ImVec2(0, json_child_h))) {
                                    if (recordedNotes.empty()) {
                                        ImGui::Dummy(ImVec2(0, DP(6.0f)));
                                        PaddedTextDisabled("No notes recorded yet.");
                                    }
                                    else {
                                        int noteCount = static_cast<int>(recordedNotes.size());
                                        int durationMs = recordedNotes.back().time;
                                        float durationSec = durationMs / 1000.0f;
                                        float beatLenMs = (s_exportBpm > 0) ? (60000.0f / static_cast<float>(s_exportBpm)) : 0.0f;
                                        float beats = (beatLenMs > 0.0f) ? (durationMs / beatLenMs) : 0.0f;

                                        ImGui::Dummy(ImVec2(0, DP(6.0f)));
                                        std::string line;

                                        line = std::string(tsm::ui::i18n::Tr("Name:")) + " " + s_exportName;
                                        PaddedText(line.c_str());

                                        line = std::string(tsm::ui::i18n::Tr("BPM:")) + " " + std::to_string(s_exportBpm);
                                        PaddedText(line.c_str());

                                        line = std::string(tsm::ui::i18n::Tr("Notes:")) + " " + std::to_string(noteCount);
                                        PaddedText(line.c_str());

                                        line = std::string(tsm::ui::i18n::Tr("Length:")) + " " + std::to_string(durationSec) + "s (" + std::to_string(beats) + " " + tsm::ui::i18n::Tr("beats") + ")";
                                        PaddedText(line.c_str());
                                    }
                                    EndPannableChild();
                                }
                            }

                            if (s_showExportModal) {
                                ImGui::OpenPopup("##ExportRecordingModal");
                                s_showExportModal = false;
                            }

                            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                            ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                            if (ImGui::BeginPopupModal("##ExportRecordingModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                                const char* title = tsm::ui::i18n::Tr("Export Recording");
                                float title_w = ImGui::CalcTextSize(title).x;
                                float window_w = ImGui::GetContentRegionAvail().x;
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                                ImGui::Text("%s", title);
                                ImGui::Separator();

                                ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                                ImGui::PushItemWidth(-1);
                                InputWithPlaceholder("##exportname", s_exportName, sizeof(s_exportName), "Recording name");
                                ImGui::PopItemWidth();

                                ImGui::Dummy(ImVec2(0, DP(8.0f)));

                                ImGui::Text("%s", tsm::ui::i18n::Tr("BPM:"));
                                ImGui::PushItemWidth(-1);
                                ImGui::InputInt("##exportbpm", &s_exportBpm);
                                ImGui::PopItemWidth();
                                if (s_exportBpm < 40) s_exportBpm = 40;
                                if (s_exportBpm > 400) s_exportBpm = 400;

                                ImGui::Dummy(ImVec2(0, DP(8.0f)));

                                float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;
                                if (AccentButton("Save", ImVec2(btn_w, 0))) {
                                    if (recordedNotes.empty()) {
                                        ShowToastError("No notes recorded");
                                    }
                                    else {
                                        std::string safeName = SanitizeSheetName(s_exportName);
                                        std::string musicDir = tsm::utils::storage::GetMusicSheetsDir();
                                        if (!tsm::utils::storage::EnsureDirectoryExists(musicDir)) {
                                            ShowToastError("Failed to create Music Sheets directory");
                                        }
                                        else {
                                            std::string filepath = musicDir + "/" + safeName + ".json";
                                            nlohmann::json songJson = BuildRecordedSongJson(recordedNotes, safeName, s_exportBpm);
                                            std::ofstream ofs(filepath);
                                            if (!ofs.is_open()) {
                                                ShowToastError("Failed to create sheet file");
                                            }
                                            else {
                                                ofs << songJson.dump(2);
                                                ofs.close();
                                                ShowToastSuccess("Recording exported");
                                                s_shouldRefreshSheets = true;
                                                ImGui::CloseCurrentPopup();
                                            }
                                        }
                                    }
                                }
                                ImGui::SameLine();
                                if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                                    ImGui::CloseCurrentPopup();
                                }

                                ImGui::Dummy(ImVec2(0, DP(4.0f)));
                                ImGui::Separator();
                                ImGui::Dummy(ImVec2(0, DP(4.0f)));

                                ImGui::EndPopup();
                            }
                            ImGui::PopStyleVar();

                            break;
                        }

                        s_selectedSheetIndex = tsm::ui::overlays::GetMusicPlaybackOverlaySelectedIndex();
                        if (s_selectedSheetIndex >= static_cast<int>(s_musicSheets.size())) {
                            s_selectedSheetIndex = -1;
                        }
                        hasValidSheet = s_selectedSheetIndex >= 0 && s_selectedSheetIndex < static_cast<int>(s_musicSheets.size());

                        tsm::ui::overlays::UpdateMusicPlaybackState(s_musicSheets, s_selectedSheetIndex);

                        bool showOverlay = tsm::state::ModState::Get().ui.showMusicPlaybackOverlay;
                        if (ToggleCardIcon("Playback Overlay", "Show floating playback controls", "UiMenuSoundPreviewOn", &showOverlay)) {
                            tsm::state::ModState::Get().ui.showMusicPlaybackOverlay = showOverlay;
                        }

                        if (s_activeCategory.empty()) {
                            CenterSeparator("Music Library");
                        }
                        else {
                            CenterSeparator(s_activeCategory.c_str());
                        }

                        float cardPad = GetCardPadding();
                        float availWidth = ImGui::GetContentRegionAvail().x - (cardPad * 2.0f);
                        float halfBtnW = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cardPad);

                        if (!s_activeCategory.empty()) {
                            if (SecondaryButton("< Back", ImVec2(halfBtnW, 0))) {
                                s_activeCategory.clear();
                                std::memset(s_searchBuffer, 0, sizeof(s_searchBuffer));
                            }
                            ImGui::SameLine();
                            if (SecondaryButton("Import Sheet", ImVec2(halfBtnW, 0))) {
                                requestFile("*/*", OnFileSelected, false);
                            }
                        }
                        else {
                            bool isLoading = s_sheetsLoading.load();
                            if (isLoading) ImGui::BeginDisabled();
                            if (SecondaryButton(isLoading ? "Loading..." : "Refresh Library", ImVec2(halfBtnW, 0))) {
                                LoadSheetsAsync();
                                s_selectedSheetIndex = -1;
                            }
                            if (isLoading) ImGui::EndDisabled();
                            ImGui::SameLine();
                            if (SecondaryButton("Import Sheet", ImVec2(halfBtnW, 0))) {
                                requestFile("*/*", OnFileSelected, false);
                            }

                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cardPad);
                            if (SecondaryButton("+ New Category", ImVec2(availWidth, 0))) {
                                std::memset(s_newCategoryNameBuf, 0, sizeof(s_newCategoryNameBuf));
                                s_showNewCategoryModal = true;
                            }
                        }

                        SearchCard("Search...", s_searchBuffer, sizeof(s_searchBuffer));

                        bool isSearching = std::strlen(s_searchBuffer) > 0;
                        std::string searchLower;
                        if (isSearching) {
                            searchLower = s_searchBuffer;
                            std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                        }

                        if (BeginPannableChild("##sheet_list_scroll")) {
                            if (s_musicSheets.empty()) {
                                PaddedTextDisabled("No music sheets found");

                            }
                            else if (isSearching && s_activeCategory.empty()) {
                                int visibleCount = 0;
                                for (size_t i = 0; i < s_musicSheets.size(); ++i) {
                                    const auto& sheet = s_musicSheets[i];

                                    std::string nameLower = sheet.name;
                                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                                    if (nameLower.find(searchLower) == std::string::npos) continue;

                                    visibleCount++;
                                    bool isSelected = (s_selectedSheetIndex == static_cast<int>(i));

                                    char noteInfo[128];
                                    const std::string& cat = GetSheetCategory(sheet.filename);
                                    const char* noteFmt = tsm::ui::i18n::Tr("%d notes - %d BPM - %s");
                                    snprintf(noteInfo, sizeof(noteInfo), noteFmt,
                                        static_cast<int>(sheet.notes.size()), sheet.bpm, cat.c_str());

                                    ImGui::PushID(static_cast<int>(i));
                                    auto result = EditableCard(sheet.name.c_str(), noteInfo, "UiMenuSheetMusicD", isSelected);

                                    if (result.main) {
                                        s_selectedSheetIndex = static_cast<int>(i);
                                        if (keyCount >= 15) {
                                            if (player.IsPlaying()) player.Stop();
                                            player.Play(s_musicSheets[s_selectedSheetIndex]);
                                        }
                                    }
                                    if (result.edit) {
                                        s_editSheetIndex = static_cast<int>(i);
                                        s_showEditModal = true;
                                        std::strncpy(s_newSheetName, sheet.name.c_str(), sizeof(s_newSheetName) - 1);
                                    }

                                    ImGui::PopID();
                                }
                                if (visibleCount == 0) PaddedTextDisabled("No sheets match your search");

                            }
                            else if (s_activeCategory.empty()) {
                                std::vector<std::string> categories = GetAllCategoryNames();

                                bool hasUncategorized = false;
                                for (const auto& sheet : s_musicSheets) {
                                    if (GetSheetCategory(sheet.filename) == "Uncategorized") {
                                        hasUncategorized = true;
                                        break;
                                    }
                                }

                                auto countInCategory = [&](const std::string& cat) -> int {
                                    int n = 0;
                                    for (const auto& sheet : s_musicSheets)
                                        if (GetSheetCategory(sheet.filename) == cat) n++;
                                    return n;
                                    };

                                if (categories.empty() && !hasUncategorized) {
                                    PaddedTextDisabled("No folders yet — create one or import a sheet");
                                }
                                else {
                                    for (size_t i = 0; i < categories.size(); ++i) {
                                        const auto& cat = categories[i];
                                        int cnt = countInCategory(cat);
                                        char subtitle[64];
                                        snprintf(subtitle, sizeof(subtitle),
                                            cnt == 1 ? "%d sheet" : "%d sheets", cnt);

                                        ImGui::PushID(static_cast<int>(i));
                                        auto result = EditableCard(cat.c_str(), subtitle, "UiMenuSheetMusicD", false);
                                        if (result.main) {
                                            s_activeCategory = cat;
                                            std::memset(s_searchBuffer, 0, sizeof(s_searchBuffer));
                                        }
                                        if (result.edit) {
                                            s_manageCategoryName = cat;
                                            std::strncpy(s_renameCategoryBuf, cat.c_str(), sizeof(s_renameCategoryBuf) - 1);
                                            s_renameCategoryBuf[sizeof(s_renameCategoryBuf) - 1] = '\0';
                                            s_showManageCategoryModal = true;
                                        }
                                        ImGui::PopID();
                                    }

                                    if (hasUncategorized) {
                                        int cnt = countInCategory("Uncategorized");
                                        char subtitle[64];
                                        snprintf(subtitle, sizeof(subtitle),
                                            cnt == 1 ? "%d sheet" : "%d sheets", cnt);
                                        if (ButtonCard("Uncategorized", subtitle, "UiMenuSheetMusicD", "OPEN")) {
                                            s_activeCategory = "Uncategorized";
                                            std::memset(s_searchBuffer, 0, sizeof(s_searchBuffer));
                                        }
                                    }
                                }

                            }
                            else {
                                bool categoryHasSheets = false;
                                int visibleCount = 0;

                                for (size_t i = 0; i < s_musicSheets.size(); ++i) {
                                    const auto& sheet = s_musicSheets[i];
                                    if (GetSheetCategory(sheet.filename) != s_activeCategory) continue;
                                    categoryHasSheets = true;

                                    if (isSearching) {
                                        std::string nameLower = sheet.name;
                                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                                        if (nameLower.find(searchLower) == std::string::npos) continue;
                                    }

                                    visibleCount++;
                                    bool isSelected = (s_selectedSheetIndex == static_cast<int>(i));

                                    char noteInfo[64];
                                    const char* noteFmt = tsm::ui::i18n::Tr("%d notes - %d BPM");
                                    snprintf(noteInfo, sizeof(noteInfo), noteFmt,
                                        static_cast<int>(sheet.notes.size()), sheet.bpm);

                                    ImGui::PushID(static_cast<int>(i));
                                    auto result = EditableCard(sheet.name.c_str(), noteInfo, "UiMenuSheetMusicD", isSelected);

                                    if (result.main) {
                                        s_selectedSheetIndex = static_cast<int>(i);
                                        if (keyCount >= 15) {
                                            if (player.IsPlaying()) player.Stop();
                                            player.Play(s_musicSheets[s_selectedSheetIndex]);
                                        }
                                    }
                                    if (result.edit) {
                                        s_editSheetIndex = static_cast<int>(i);
                                        s_showEditModal = true;
                                        std::strncpy(s_newSheetName, sheet.name.c_str(), sizeof(s_newSheetName) - 1);
                                    }

                                    ImGui::PopID();
                                }

                                if (!categoryHasSheets) {
                                    s_activeCategory.clear();
                                }
                                else if (visibleCount == 0) {
                                    PaddedTextDisabled("No sheets match your search");
                                }
                            }
                        }
                        EndPannableChild();

                        if (s_showEditModal) {
                            ImGui::OpenPopup("##EditSheetModal");
                            s_showEditModal = false;
                        }

                        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                        ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                        if (ImGui::BeginPopupModal("##EditSheetModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                            if (s_editSheetIndex >= 0 && s_editSheetIndex < static_cast<int>(s_musicSheets.size())) {
                                const auto& editSheet = s_musicSheets[s_editSheetIndex];

                                const char* title = tsm::ui::i18n::Tr("Edit Music Sheet");
                                float title_w = ImGui::CalcTextSize(title).x;
                                float window_w = ImGui::GetContentRegionAvail().x;
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                                ImGui::Text("%s", title);
                                ImGui::Separator();

                                ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                                ImGui::PushItemWidth(-1);
                                InputWithPlaceholder("##editname", s_newSheetName, sizeof(s_newSheetName), "Sheet name");
                                ImGui::PopItemWidth();

                                ImGui::Dummy(ImVec2(0, DP(8.0f)));

                                float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;

                                if (AccentButton("Save", ImVec2(btn_w, 0))) {
                                    std::string error;
                                    std::string oldFilename = editSheet.filename;
                                    std::string newNameCopy = s_newSheetName;
                                    std::string safeName = SanitizeSheetName(s_newSheetName);
                                    int editIdx = s_editSheetIndex;

                                    if (tsm::utils::storage::RenameMusicSheet(oldFilename, newNameCopy, error)) {
                                        std::string oldCat = GetSheetCategory(oldFilename);
                                        s_sheetCategories.erase(oldFilename);
                                        std::string newFilename = safeName + ".json";
                                        if (oldCat != "Uncategorized")
                                            SetSheetCategory(newFilename, oldCat);
                                        else
                                            SaveCategories();

                                        if (s_selectedSheetIndex == editIdx) s_selectedSheetIndex = -1;
                                        LoadSheetsAsync();
                                        ShowToastSuccess("Sheet renamed");
                                        ImGui::CloseCurrentPopup();
                                    }
                                    else {
                                        ShowToastError(error);
                                    }
                                }
                                ImGui::SameLine();
                                if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                                    ImGui::CloseCurrentPopup();
                                }

                                ImGui::Dummy(ImVec2(0, DP(4.0f)));
                                ImGui::Separator();
                                ImGui::Dummy(ImVec2(0, DP(4.0f)));

                                const std::string& currentCat = GetSheetCategory(editSheet.filename);
                                ImGui::Text("%s", tsm::ui::i18n::Tr("Category:"));
                                ImGui::SameLine();
                                ImGui::TextDisabled("%s", currentCat.c_str());
                                ImGui::Dummy(ImVec2(0, DP(4.0f)));
                                if (SecondaryButton("Move to Category...", ImVec2(-1, 0))) {
                                    s_moveSheetIndex = s_editSheetIndex;
                                    s_showMoveModal = true;
                                }

                                ImGui::Dummy(ImVec2(0, DP(4.0f)));
                                ImGui::Separator();
                                ImGui::Dummy(ImVec2(0, DP(4.0f)));

                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                                const char* delLabel = tsm::ui::i18n::Tr("Delete Sheet");
                                if (ImGui::Button(delLabel, ImVec2(-1, 0))) {
                                    std::string filename = editSheet.filename;
                                    int editIdx = s_editSheetIndex;

                                    if (tsm::utils::storage::DeleteMusicSheet(filename)) {
                                        s_sheetCategories.erase(filename);
                                        SaveCategories();

                                        if (s_selectedSheetIndex == editIdx) {
                                            player.Stop();
                                            s_selectedSheetIndex = -1;
                                        }
                                        else if (s_selectedSheetIndex > editIdx) {
                                            s_selectedSheetIndex--;
                                        }

                                        LoadSheetsAsync();
                                        ShowToastSuccess("Sheet deleted");
                                        ImGui::CloseCurrentPopup();
                                    }
                                    else {
                                        ShowToastError("Failed to delete sheet");
                                    }
                                }
                                ImGui::PopStyleColor(3);
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleVar();

                        if (s_showMoveModal) {
                            ImGui::OpenPopup("##MoveSheetModal");
                            s_showMoveModal = false;
                        }

                        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                        ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                        if (ImGui::BeginPopupModal("##MoveSheetModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                            const char* title = tsm::ui::i18n::Tr("Move to Category");
                            float title_w = ImGui::CalcTextSize(title).x;
                            float window_w = ImGui::GetContentRegionAvail().x;
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                            ImGui::Text("%s", title);
                            ImGui::Separator();
                            ImGui::Dummy(ImVec2(0, DP(4.0f)));

                            if (s_moveSheetIndex >= 0 && s_moveSheetIndex < static_cast<int>(s_musicSheets.size())) {
                                const auto& moveSheet = s_musicSheets[s_moveSheetIndex];
                                const std::string& currentCat = GetSheetCategory(moveSheet.filename);

                                std::vector<std::string> cats = GetAllCategoryNames();
                                if (cats.empty()) {
                                    PaddedTextDisabled("No categories yet");
                                    PaddedTextDisabled("Close and create a category first");
                                }
                                else {
                                    for (const auto& cat : cats) {
                                        bool isCurrent = (currentCat == cat);
                                        if (isCurrent) ImGui::BeginDisabled();
                                        if (AccentButton(cat.c_str(), ImVec2(-1, 0))) {
                                            SetSheetCategory(moveSheet.filename, cat);
                                            ShowToastSuccess("Sheet moved");
                                            ImGui::CloseCurrentPopup();
                                        }
                                        if (isCurrent) ImGui::EndDisabled();
                                        ImGui::Dummy(ImVec2(0, DP(2.0f)));
                                    }
                                }

                                bool isAlreadyUncategorized = (currentCat == "Uncategorized");
                                if (isAlreadyUncategorized) ImGui::BeginDisabled();
                                if (SecondaryButton("Uncategorized", ImVec2(-1, 0))) {
                                    SetSheetCategory(moveSheet.filename, "Uncategorized");
                                    ShowToastSuccess("Sheet moved to Uncategorized");
                                    ImGui::CloseCurrentPopup();
                                }
                                if (isAlreadyUncategorized) ImGui::EndDisabled();
                            }

                            ImGui::Dummy(ImVec2(0, DP(4.0f)));
                            ImGui::Separator();
                            ImGui::Dummy(ImVec2(0, DP(4.0f)));

                            if (SecondaryButton("Cancel", ImVec2(-1, 0))) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::Dummy(ImVec2(0, DP(2.0f)));

                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleVar();

                        if (s_showNewCategoryModal) {
                            ImGui::OpenPopup("##NewCategoryModal");
                            s_showNewCategoryModal = false;
                        }

                        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                        ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                        if (ImGui::BeginPopupModal("##NewCategoryModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                            const char* title = tsm::ui::i18n::Tr("New Category");
                            float title_w = ImGui::CalcTextSize(title).x;
                            float window_w = ImGui::GetContentRegionAvail().x;
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                            ImGui::Text("%s", title);
                            ImGui::Separator();

                            ImGui::Dummy(ImVec2(0, DP(4.0f)));
                            ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                            ImGui::PushItemWidth(-1);
                            InputWithPlaceholder("##newcatname", s_newCategoryNameBuf, sizeof(s_newCategoryNameBuf), "Category name");
                            ImGui::PopItemWidth();
                            ImGui::TextDisabled("%s", tsm::ui::i18n::Tr("Letters, numbers, spaces, - and _ only"));
                            ImGui::Dummy(ImVec2(0, DP(8.0f)));

                            float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;
                            bool nameValid = CategoryNameValid(s_newCategoryNameBuf);
                            if (!nameValid) ImGui::BeginDisabled();
                            if (AccentButton("Create", ImVec2(btn_w, 0))) {
                                std::string newCat = s_newCategoryNameBuf;
                                s_persistedFolders.insert(newCat);
                                SaveCategories();
                                s_activeCategory = newCat;
                                std::memset(s_newCategoryNameBuf, 0, sizeof(s_newCategoryNameBuf));
                                ImGui::CloseCurrentPopup();
                            }
                            if (!nameValid) ImGui::EndDisabled();
                            ImGui::SameLine();
                            if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::Dummy(ImVec2(0, DP(2.0f)));

                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleVar();

                        if (s_showManageCategoryModal) {
                            ImGui::OpenPopup("##ManageCategoryModal");
                            s_showManageCategoryModal = false;
                        }

                        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                        ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                        if (ImGui::BeginPopupModal("##ManageCategoryModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                            const char* title = tsm::ui::i18n::Tr("Manage Category");
                            float title_w = ImGui::CalcTextSize(title).x;
                            float window_w = ImGui::GetContentRegionAvail().x;
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                            ImGui::Text("%s", title);
                            ImGui::Separator();

                            ImGui::Dummy(ImVec2(0, DP(4.0f)));
                            ImGui::Text("%s", tsm::ui::i18n::Tr("Rename:"));
                            ImGui::PushItemWidth(-1);
                            InputWithPlaceholder("##renamecatinput", s_renameCategoryBuf, sizeof(s_renameCategoryBuf), "Category name");
                            ImGui::PopItemWidth();
                            ImGui::Dummy(ImVec2(0, DP(8.0f)));

                            float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;
                            bool renameChanged = (s_renameCategoryBuf != s_manageCategoryName);
                            bool renameValid = CategoryNameValid(s_renameCategoryBuf) && renameChanged;
                            if (!renameValid) ImGui::BeginDisabled();
                            if (AccentButton("Rename", ImVec2(btn_w, 0))) {
                                std::string newCat = s_renameCategoryBuf;
                                for (auto& [filename, cat] : s_sheetCategories)
                                    if (cat == s_manageCategoryName) cat = newCat;
                                s_persistedFolders.erase(s_manageCategoryName);
                                s_persistedFolders.insert(newCat);
                                SaveCategories();
                                if (s_activeCategory == s_manageCategoryName) s_activeCategory = newCat;
                                s_manageCategoryName = newCat;
                                ShowToastSuccess("Category renamed");
                                ImGui::CloseCurrentPopup();
                            }
                            if (!renameValid) ImGui::EndDisabled();
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
                            if (ImGui::Button(tsm::ui::i18n::Tr("Delete Category"), ImVec2(-1, 0))) {
                                std::vector<std::string> toErase;
                                for (auto& [filename, cat] : s_sheetCategories)
                                    if (cat == s_manageCategoryName) toErase.push_back(filename);
                                for (const auto& k : toErase) s_sheetCategories.erase(k);
                                s_persistedFolders.erase(s_manageCategoryName);
                                SaveCategories();
                                if (s_activeCategory == s_manageCategoryName) s_activeCategory.clear();
                                ShowToastSuccess("Category deleted");
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::PopStyleColor(3);
                            ImGui::Dummy(ImVec2(0, DP(2.0f)));
                            ImGui::TextDisabled("%s", tsm::ui::i18n::Tr("Sheets will be moved to Uncategorized"));
                            ImGui::Dummy(ImVec2(0, DP(2.0f)));

                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleVar();

                        break;
                    }

                    }
                    EndCard();
                }
            }

        }
    }
}