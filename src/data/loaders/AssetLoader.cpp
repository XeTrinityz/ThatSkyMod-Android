#include <data/loaders/AssetLoader.h>
#include <utils/logging/log.h>
#include <Cipher/Cipher.h>
#include <cstring>

namespace tsm { namespace data { namespace loaders {

bool LoadJsonAsset(const char* assetPath, nlohmann::json& outJson) {
    if (!assetPath || !*assetPath) {
        tsm::log::e("AssetLoader: Invalid asset path (null or empty)");
        return false;
    }

    char pathBuffer[512];
    std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", assetPath);
    char* assetData = Cipher::read_asset(pathBuffer);
    if (!assetData) {
        tsm::log::e("AssetLoader: Failed to read asset: %s", assetPath);
        return false;
    }

    try {
        outJson = nlohmann::json::parse(assetData, nullptr, true);
        return true;
    } catch (const std::exception& e) {
        tsm::log::e("AssetLoader: JSON parse error for %s: %s", assetPath, e.what());
        return false;
    }
}

}}}
