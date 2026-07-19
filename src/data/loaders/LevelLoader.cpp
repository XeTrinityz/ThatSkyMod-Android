#include <data/loaders/LevelLoader.h>
#include <game/interop/lua_bridge.h>
#include <utils/logging/log.h>
#include <Cipher/Cipher.h>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace tsm { namespace data { namespace loaders {

namespace {
    std::string GetFilesDirectory() {
        const char* cfgPath = Cipher::getConfigPath();
        std::string filesDir = "/data/data/git.artdeell.skymodloader/files";

        if (cfgPath && *cfgPath) {
            filesDir.assign(cfgPath);
            auto pos = filesDir.find_last_of("/\\");
            if (pos != std::string::npos) {
                filesDir.erase(pos);
            }
        }

        return filesDir;
    }

    std::string GenerateLuaExportScript(const std::string& tsvPath) {
        std::string lua;
        lua.reserve(512 + tsvPath.size());

        lua += "function TSM_PrintLevels()\n";
        lua += "    local ok = (type(ArcLevels) == 'table') and (io ~= nil)\n";
        lua += "    if not ok then return end\n";
        lua += "    local file = assert(io.open(\"" + tsvPath + "\", \"w\"))\n";
        lua += "    for i, level in ipairs(ArcLevels) do\n";
        lua += "        local realm = tostring(level.realm or \"\")\n";
        lua += "        local name  = tostring(level.name  or \"\")\n";
        lua += "        file:write(realm .. \"\\t\" .. name .. \"\\n\")\n";
        lua += "    end\n";
        lua += "    file:close()\n";
        lua += "end\n";
        lua += "TSM_PrintLevels()\n";

        return lua;
    }

    bool ParseTsvToJson(const std::string& tsvPath, nlohmann::json& outLevels) {
        std::ifstream file(tsvPath);
        if (!file.is_open()) {
            tsm::log::e("LevelLoader: Failed to open TSV file: %s", tsvPath.c_str());
            return false;
        }

        nlohmann::json levels = nlohmann::json::object();
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream lineStream(line);
            std::string realm, name;

            if (std::getline(lineStream, realm, '\t') && std::getline(lineStream, name)) {
                if (!levels.contains(realm)) {
                    levels[realm] = nlohmann::json::array();
                }
                levels[realm].push_back(name);
            }
        }

        file.close();
        outLevels = std::move(levels);
        return !outLevels.empty();
    }
}

bool LoadLevelsFromLua(nlohmann::json& outLevels) {
    const std::string filesDir = GetFilesDirectory();
    const std::string tsvPath = filesDir + "/LevelNames.tsv";

    const std::string luaScript = GenerateLuaExportScript(tsvPath);
    std::string luaOutput;
    int luaStatus = -1;

    (void)tsm::lua::bridge::Run(luaScript.c_str(), luaOutput, &luaStatus);

    bool success = ParseTsvToJson(tsvPath, outLevels);

    std::remove(tsvPath.c_str());

    if (!success) {
        tsm::log::e("LevelLoader: Failed to load levels from Lua");
    }

    return success;
}

}}}
