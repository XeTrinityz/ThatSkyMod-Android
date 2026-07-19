#ifndef TSM_V2_FEATURES_FEATUREMANAGER_H
#define TSM_V2_FEATURES_FEATUREMANAGER_H

#include <string>

namespace tsm { namespace features {

enum class FeatureId : unsigned char {
    Invincibility = 0,
    AntiRainDrain,
    AntiKrill,
    AntiAFK,
    AutoCharge,
    SuperRun,
    SuperSlidey,
    SuperFlight,
    SuperLaunch,
    FastFlap,
    UnlimitedFireworks,
    UnlockAll,
    UnlockEmoteLevels,
    UnlockRelationshipAbilities,
    AutoCollectWax,
    AutoBurnCandles,
    AutoBurnPlants,
    FastHome,
    RevealAllPlayers,
    LightAllPlayers,
    ShowModdedOutfits,
    EspPlayers,
    EspNPCs,
    EspWingLights,
    EspDyes,
    EspCandles
};

class FeatureManager {
public:
    static FeatureManager& Get();

    void ApplyFeature(FeatureId id, bool enabled);

    void ApplyPlayerFeatures();

    void ApplyCameraFeatures();

    void ApplyUnlockFeatures();

    void ApplyWorldFeatures();

    void ApplyShoutEditorFeatures();

    void ApplyAllFeatures();

private:
    FeatureManager() = default;
    FeatureManager(const FeatureManager&) = delete;
    FeatureManager& operator=(const FeatureManager&) = delete;
};

}}

#endif
