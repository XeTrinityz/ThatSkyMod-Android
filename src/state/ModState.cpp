#include <state/ModState.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <Cipher/CipherUtils.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <imgui/imgui.h>
#include <algorithm>

namespace tsm { namespace state {

namespace {
    constexpr float kDefaultAccentX = 0.00f;
    constexpr float kDefaultAccentY = 0.75f;
    constexpr float kDefaultAccentZ = 0.50f;
    constexpr float kDefaultAccentW = 1.0f;
}

ModState& ModState::Get() {
    static ModState s;
    return s;
}

namespace {
    #define SAVE_FIELD(type, name, defaultValue) j[section][#name] = state.name;
}

bool ModState::SaveToFile() const {
    nlohmann::json j;

    { const char* section = "player"; const auto& state = playerGeneral; PLAYER_GENERAL_FIELDS(SAVE_FIELD) }
    { const char* section = "ui"; const auto& state = ui; UI_PREFS_FIELDS(SAVE_FIELD) }
    { const char* section = "debug"; const auto& state = debug; DEBUG_FIELDS(SAVE_FIELD) }
    { const char* section = "camera"; const auto& state = camera; CAMERA_FIELDS(SAVE_FIELD) }
    { const char* section = "unlocks"; const auto& state = unlocks; UNLOCKS_FIELDS(SAVE_FIELD) }
    { const char* section = "world"; const auto& state = world; WORLD_SESSION_FIELDS(SAVE_FIELD) }
    { const char* section = "progression"; const auto& state = progression; PROGRESSION_FIELDS(SAVE_FIELD) }

    {
        ImVec4 acc = tsm::ui::GetAccentColor();
        j["ui"]["accent"] = { acc.x, acc.y, acc.z, acc.w };
    }

    const char* cfgDir = CipherUtils::get_ConfigsPath();
    std::string path = (cfgDir && *cfgDir) ? (std::string(cfgDir) + "/mod_state.json")
                                           : std::string("/data/data/git.artdeell.skymodloader/files/config/mod_state.json");
    try {
        std::ofstream ofs(path.c_str(), std::ios::binary);
        if (!ofs.is_open()) return false;
        ofs << j.dump(2);
        ofs.close();
        return true;
    } catch (...) {
        return false;
    }
}

namespace {
    template<typename T>
    struct LoadHelper;

    template<> struct LoadHelper<bool> {
        static void load(const nlohmann::json& obj, const char* key, bool& target) {
            if (auto it = obj.find(key); it != obj.end() && it->is_boolean()) {
                target = it->get<bool>();
            }
        }
    };

    template<> struct LoadHelper<int> {
        static void load(const nlohmann::json& obj, const char* key, int& target) {
            if (auto it = obj.find(key); it != obj.end() && it->is_number_integer()) {
                target = it->get<int>();
            }
        }
    };

    template<> struct LoadHelper<float> {
        static void load(const nlohmann::json& obj, const char* key, float& target) {
            if (auto it = obj.find(key); it != obj.end() && it->is_number()) {
                target = it->get<float>();
            }
        }
    };

    template<> struct LoadHelper<std::uint8_t> {
        static void load(const nlohmann::json& obj, const char* key, std::uint8_t& target) {
            if (auto it = obj.find(key); it != obj.end() && it->is_number_integer()) {
                int v = it->get<int>();
                target = static_cast<std::uint8_t>(std::clamp(v, 0, 255));
            }
        }
    };

    template<> struct LoadHelper<std::string> {
        static void load(const nlohmann::json& obj, const char* key, std::string& target) {
            if (auto it = obj.find(key); it != obj.end() && it->is_string()) {
                target = it->get<std::string>();
            }
        }
    };

    #define LOAD_FIELD(type, name, defaultValue) LoadHelper<type>::load(obj, #name, state.name);

    #define RESET_FIELD(type, name, defaultValue) name = defaultValue;

    void load_accent_color(const nlohmann::json& obj) {
        if (auto it = obj.find("accent"); it != obj.end() && it->is_array() && it->size() >= 3) {
            float x = kDefaultAccentX, y = kDefaultAccentY, z = kDefaultAccentZ, w = kDefaultAccentW;
            try {
                x = (*it)[0].get<float>();
                y = (*it)[1].get<float>();
                z = (*it)[2].get<float>();
                if (it->size() >= 4) w = (*it)[3].get<float>();
            } catch (...) {}
            tsm::ui::SetAccentColor(ImVec4(x, y, z, w));
        }
    }
}

bool ModState::LoadFromFile() {
    const char* cfgDir = CipherUtils::get_ConfigsPath();
    std::string path = (cfgDir && *cfgDir) ? (std::string(cfgDir) + "/mod_state.json")
                                           : std::string("/data/data/git.artdeell.skymodloader/files/config/mod_state.json");
    nlohmann::json j;
    try {
        std::ifstream ifs(path.c_str(), std::ios::binary);
        if (!ifs.is_open()) return false;
        ifs >> j;
        ifs.close();
    } catch (...) {
        return false;
    }

    if (auto it = j.find("ui"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = ui;
        UI_PREFS_FIELDS(LOAD_FIELD)
        load_accent_color(obj);

        tsm::ui::i18n::SetActiveLanguageIndex(ui.languageId);

        ImGuiStyle& st = ImGui::GetStyle();
        st.ScrollbarSize = ui.scrollbarVisible ? (float)std::clamp(ui.scrollbarWidthPx, 20, 40) : 0.0f;
    }

    if (auto it = j.find("player"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = playerGeneral;
        PLAYER_GENERAL_FIELDS(LOAD_FIELD)
    }

    if (auto it = j.find("unlocks"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = unlocks;
        UNLOCKS_FIELDS(LOAD_FIELD)
    }

    if (auto it = j.find("debug"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = debug;
        DEBUG_FIELDS(LOAD_FIELD)
    }

    if (auto it = j.find("camera"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = camera;
        CAMERA_FIELDS(LOAD_FIELD)
    }

    if (auto it = j.find("world"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = world;
        WORLD_SESSION_FIELDS(LOAD_FIELD)
    }

    if (auto it = j.find("progression"); it != j.end() && it->is_object()) {
        auto& obj = *it;
        auto& state = progression;
        PROGRESSION_FIELDS(LOAD_FIELD)
    }

    return true;
}

void ModState::ResetToDefaults() {
    #define RESET_UI(type, name, defaultValue) ui.name = defaultValue;
    #define RESET_DEBUG(type, name, defaultValue) debug.name = defaultValue;
    #define RESET_CAMERA(type, name, defaultValue) camera.name = defaultValue;
    #define RESET_UNLOCKS(type, name, defaultValue) unlocks.name = defaultValue;
    #define RESET_WORLD(type, name, defaultValue) world.name = defaultValue;
    #define RESET_PLAYER(type, name, defaultValue) playerGeneral.name = defaultValue;
    #define RESET_PROGRESSION(type, name, defaultValue) progression.name = defaultValue;

    UI_PREFS_FIELDS(RESET_UI)
    DEBUG_FIELDS(RESET_DEBUG)
    CAMERA_FIELDS(RESET_CAMERA)
    UNLOCKS_FIELDS(RESET_UNLOCKS)
    WORLD_SESSION_FIELDS(RESET_WORLD)
    PLAYER_GENERAL_FIELDS(RESET_PLAYER)
    PROGRESSION_FIELDS(RESET_PROGRESSION)
    tsm::ui::i18n::SetActiveLanguageIndex(0);

    #undef RESET_UI
    #undef RESET_DEBUG
    #undef RESET_CAMERA
    #undef RESET_UNLOCKS
    #undef RESET_WORLD
    #undef RESET_PLAYER
    #undef RESET_PROGRESSION

    ImGui::GetStyle().ScrollbarSize = 0.0f;
    tsm::ui::SetDockedRight(ui.dockRight);
    tsm::ui::SetAccentColor(ImVec4(kDefaultAccentX, kDefaultAccentY, kDefaultAccentZ, kDefaultAccentW));
}

}}
