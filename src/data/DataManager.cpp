#include <data/DataManager.h>
#include <data/loaders/LevelLoader.h>
#include <data/loaders/AssetLoader.h>
#include <utils/logging/log.h>
#include <mutex>

namespace tsm { namespace data {

namespace {
    std::mutex g_dataMutex;
}

DataManager& DataManager::Get() {
    static DataManager instance;
    return instance;
}

bool DataManager::LoadAll() {
    bool anySuccess = false;

    anySuccess = LoadLevels() || anySuccess;
    anySuccess = LoadOutfitDefs() || anySuccess;
    anySuccess = LoadCollectibleDefs() || anySuccess;
    anySuccess = LoadWorldQuestDefs() || anySuccess;
    anySuccess = LoadDyeColorDefs() || anySuccess;

    return anySuccess;
}

void DataManager::ClearCache() {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    m_cache.levelsLoaded = false;
    m_cache.outfitDefsLoaded = false;
    m_cache.collectibleDefsLoaded = false;
    m_cache.worldQuestDefsLoaded = false;
    m_cache.dyeColorDefsLoaded = false;

    m_cache.levels = nlohmann::json();
    m_cache.outfitDefs = nlohmann::json();
    m_cache.collectibleDefs = nlohmann::json();
    m_cache.worldQuestDefs = nlohmann::json();
    m_cache.dyeColorDefs = nlohmann::json();
}

bool DataManager::LoadLevels() {
    std::lock_guard<std::mutex> lock(g_dataMutex);

    if (m_cache.levelsLoaded) {
        return true;
    }

    nlohmann::json levels;
    if (!loaders::LoadLevelsFromLua(levels)) {
        tsm::log::e("DataManager: Failed to load levels");
        return false;
    }

    m_cache.levels = std::move(levels);
    m_cache.levelsLoaded = true;
    tsm::log::i("DataManager: Levels loaded successfully");
    return true;
}

bool DataManager::LoadOutfitDefs() {
    std::lock_guard<std::mutex> lock(g_dataMutex);

    if (m_cache.outfitDefsLoaded) {
        return true;
    }

    nlohmann::json outfits;
    if (!loaders::LoadJsonAsset("Data/Resources/OutfitDefs.json", outfits)) {
        tsm::log::e("DataManager: Failed to load outfit definitions");
        return false;
    }

    m_cache.outfitDefs = std::move(outfits);
    m_cache.outfitDefsLoaded = true;
    return true;
}

bool DataManager::LoadCollectibleDefs() {
    std::lock_guard<std::mutex> lock(g_dataMutex);

    if (m_cache.collectibleDefsLoaded) {
        return true;
    }

    nlohmann::json collectibles;
    if (!loaders::LoadJsonAsset("Data/Resources/CollectibleDefs.json", collectibles)) {
        tsm::log::e("DataManager: Failed to load collectible definitions");
        return false;
    }

    m_cache.collectibleDefs = std::move(collectibles);
    m_cache.collectibleDefsLoaded = true;
    return true;
}

bool DataManager::LoadWorldQuestDefs() {
    std::lock_guard<std::mutex> lock(g_dataMutex);

    if (m_cache.worldQuestDefsLoaded) {
        return true;
    }

    nlohmann::json quests;
    if (!loaders::LoadJsonAsset("Data/Resources/WorldQuestDefs.json", quests)) {
        tsm::log::e("DataManager: Failed to load world quest definitions");
        return false;
    }

    m_cache.worldQuestDefs = std::move(quests);
    m_cache.worldQuestDefsLoaded = true;
    return true;
}

bool DataManager::LoadDyeColorDefs() {
    std::lock_guard<std::mutex> lock(g_dataMutex);

    if (m_cache.dyeColorDefsLoaded) {
        return true;
    }

    nlohmann::json dyes;
    if (!loaders::LoadJsonAsset("Data/Resources/DyeColorDefs.json", dyes)) {
        tsm::log::e("DataManager: Failed to load dye color definitions");
        return false;
    }

    m_cache.dyeColorDefs = std::move(dyes);
    m_cache.dyeColorDefsLoaded = true;
    return true;
}

bool DataManager::IsLevelsLoaded() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.levelsLoaded;
}

bool DataManager::IsOutfitDefsLoaded() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.outfitDefsLoaded;
}

bool DataManager::IsCollectibleDefsLoaded() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.collectibleDefsLoaded;
}

bool DataManager::IsWorldQuestDefsLoaded() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.worldQuestDefsLoaded;
}

bool DataManager::IsDyeColorDefsLoaded() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.dyeColorDefsLoaded;
}

nlohmann::json DataManager::GetLevels() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.levels;
}

const nlohmann::json& DataManager::GetOutfitDefs() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.outfitDefs;
}

nlohmann::json DataManager::GetCollectibleDefs() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.collectibleDefs;
}

nlohmann::json DataManager::GetWorldQuestDefs() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.worldQuestDefs;
}

nlohmann::json DataManager::GetDyeColorDefs() const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return m_cache.dyeColorDefs;
}

std::optional<nlohmann::json> DataManager::FindLevelByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(g_dataMutex);

    if (!m_cache.levels.is_object()) return std::nullopt;

    for (auto it = m_cache.levels.begin(); it != m_cache.levels.end(); ++it) {
        const auto& realm = it.key();
        const auto& arr = it.value();

        if (!arr.is_array()) continue;

        for (const auto& levelName : arr) {
            if (levelName.is_string() && levelName.get<std::string>() == name) {
                nlohmann::json result;
                result["realm"] = realm;
                result["name"] = name;
                return result;
            }
        }
    }

    return std::nullopt;
}

namespace {
    std::optional<nlohmann::json> FindInArrayByKey(const nlohmann::json& arr, const char* key, const std::string& value) {
        if (!arr.is_array()) return std::nullopt;

        for (const auto& item : arr) {
            if (!item.is_object()) continue;

            auto keyIt = item.find(key);
            if (keyIt != item.end() && keyIt->is_string() && keyIt->get<std::string>() == value) {
                return item;
            }
        }

        return std::nullopt;
    }
}

std::optional<nlohmann::json> DataManager::FindOutfitByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return FindInArrayByKey(m_cache.outfitDefs, "name", name);
}

std::optional<nlohmann::json> DataManager::FindCollectibleByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return FindInArrayByKey(m_cache.collectibleDefs, "name", name);
}

std::optional<nlohmann::json> DataManager::FindWorldQuestByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    return FindInArrayByKey(m_cache.worldQuestDefs, "name", name);
}

}}
