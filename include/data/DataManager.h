#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace tsm { namespace data {

class DataManager {
public:
    static DataManager& Get();

    bool LoadAll();

    bool LoadLevels();
    bool LoadOutfitDefs();
    bool LoadCollectibleDefs();
    bool LoadWorldQuestDefs();
    bool LoadDyeColorDefs();
    void ClearCache();

    bool IsLevelsLoaded() const;
    bool IsOutfitDefsLoaded() const;
    bool IsCollectibleDefsLoaded() const;
    bool IsWorldQuestDefsLoaded() const;
    bool IsDyeColorDefsLoaded() const;

    nlohmann::json GetLevels() const;
    const nlohmann::json& GetOutfitDefs() const;
    nlohmann::json GetCollectibleDefs() const;
    nlohmann::json GetWorldQuestDefs() const;
    nlohmann::json GetDyeColorDefs() const;

    std::optional<nlohmann::json> FindLevelByName(const std::string& name) const;
    std::optional<nlohmann::json> FindOutfitByName(const std::string& name) const;
    std::optional<nlohmann::json> FindCollectibleByName(const std::string& name) const;
    std::optional<nlohmann::json> FindWorldQuestByName(const std::string& name) const;

private:
    DataManager() = default;
    ~DataManager() = default;
    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;

    struct DataCache {
        nlohmann::json levels;
        nlohmann::json outfitDefs;
        nlohmann::json collectibleDefs;
        nlohmann::json worldQuestDefs;
        nlohmann::json dyeColorDefs;

        bool levelsLoaded = false;
        bool outfitDefsLoaded = false;
        bool collectibleDefsLoaded = false;
        bool worldQuestDefsLoaded = false;
        bool dyeColorDefsLoaded = false;
    };

    mutable DataCache m_cache;
};

}}
