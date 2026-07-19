#ifndef TSM_V2_STATE_MODSTATE_H
#define TSM_V2_STATE_MODSTATE_H

#include <cstdint>
#include <string>

namespace tsm { namespace state {


#define PLAYER_GENERAL_FIELDS(X) \
    X(bool, invincibility, false) \
    X(bool, antiRainDrain, false) \
    X(bool, antiKrill, false) \
    X(bool, antiAFK, false) \
    X(bool, autoCharge, false) \
    X(bool, superRun, false) \
    X(bool, superSlidey, false) \
    X(bool, superFlight, false) \
    X(bool, superLaunch, false) \
    X(bool, fastFlap, false) \
    X(bool, unlimitedFireworks, false)

#define UI_PREFS_FIELDS(X) \
    X(bool, dockRight, true) \
    X(float, handleYNorm, 0.5f) \
    X(int, activeTab, 0) \
    X(int, panelWidthDp, 500) \
    X(int, scrollbarWidthPx, 20) \
    X(bool, scrollbarVisible, false) \
    X(bool, scrollbarAutoHide, false) \
    X(bool, accentRainbow, false) \
    X(int, languageId, 0) \
    X(int, backgroundEffectType, 0) \
    X(int, neuronNodeCount, 30) \
    X(float, neuronSpeed, 30.0f) \
    X(float, neuronConnectionDistance, 200.0f) \
    X(float, neuronGlowIntensity, 0.35f) \
    X(int, particleCount, 100) \
    X(float, particleSpeed, 20.0f) \
    X(float, particleSize, 3.0f) \
    X(int, waveCount, 5) \
    X(float, waveSpeed, 15.0f) \
    X(float, waveAmplitude, 30.0f) \
    X(float, waveThickness, 2.0f) \
    X(int, starCount, 150) \
    X(float, starTwinkleSpeed, 1.0f) \
    X(float, starSizeMax, 3.0f) \
    X(int, fireflyCount, 50) \
    X(float, fireflySpeed, 15.0f) \
    X(float, fireflyGlowIntensity, 0.6f) \
    X(int, snowflakeCount, 150) \
    X(float, snowFallSpeed, 30.0f) \
    X(float, snowDriftAmount, 20.0f) \
    X(int, collapseHandleStyle, 0) \
    X(int, collapseHandleIconIndex, 0) \
    X(std::string, collapseHandleIconName, std::string("UiMenuArrowCollapse")) \
    X(bool, showFavoritesRadial, false) \
    X(std::string, favouritesRadialSlots, std::string()) \
    X(bool, showMusicPlaybackOverlay, false) \
    X(float, musicBpmMultiplier, 1.0f) \
    X(float, musicSustainDuration, 0.0f) \
    X(bool, musicAutoPlayNext, false) \
    X(bool, musicShuffle, false)

#define DEBUG_FIELDS(X) \
    X(bool, showOverlay, false) \
    X(float, overlayPosX, -1.0f) \
    X(float, overlayPosY, -1.0f) \
    X(bool, showSkyClock, false) \
    X(int, skyClockPosition, 1) \
    X(bool, espEnabled, true) \
    X(bool, espShowIcons, true) \
    X(bool, espShowName, true) \
    X(bool, espShowDistance, true) \
    X(bool, espShowLine, true) \
    X(bool, espCloseInfo, false) \
    X(float, espMaxDistance, 100.0f) \
    X(bool, espShowPlayers, false) \
    X(bool, espShowNPCs, false) \
    X(bool, espShowWingLights, false) \
    X(bool, espShowDyes, false) \
    X(bool, espShowCandles, false)

#define CAMERA_FIELDS(X) \
    X(bool, disableSnap, false) \
    X(bool, lockCamPos, false) \
    X(bool, freeZoom, false)

#define UNLOCKS_FIELDS(X) \
    X(bool, unlockAll, false) \
    X(bool, unlockEmoteLevels, false) \
    X(bool, unlockRelationshipAbilities, false) \
    X(bool, autoCollectQuests, false)

#define WORLD_SESSION_FIELDS(X) \
    X(bool, disableCutscenes, false) \
    X(bool, disableSpiritGates, false) \
    X(bool, fastHome, false) \
    X(bool, showSpirits, false) \
    X(bool, showWingBuffs, false) \
    X(bool, showMapShrines, false) \
    X(bool, showMapItems, false) \
    X(bool, pauseUiAnimations, false) \
    X(bool, disableWindWalls, false) \
    X(bool, showModdedOutfits, false) \
    X(bool, readTableMessages, false)

#define PROGRESSION_FIELDS(X) \
    X(int, prePlantDelaySec, 10) \
    X(int, waitForDyeInitSec, 10) \
    X(int, plantIntervalSec, 1) \
    X(int, dyeIntervalSec, 3) \
    X(int, betweenLevelsSec, 5)

#define DECLARE_FIELD(type, name, defaultValue) type name{defaultValue};

struct PlayerGeneralState {
    PLAYER_GENERAL_FIELDS(DECLARE_FIELD)
};

struct UiPrefsState {
    UI_PREFS_FIELDS(DECLARE_FIELD)
};

struct DebugState {
    DEBUG_FIELDS(DECLARE_FIELD)
};

struct CameraState {
    CAMERA_FIELDS(DECLARE_FIELD)
};

struct UnlocksState {
    UNLOCKS_FIELDS(DECLARE_FIELD)
};

struct WorldSessionState {
    WORLD_SESSION_FIELDS(DECLARE_FIELD)
};

struct ProgressionState {
    PROGRESSION_FIELDS(DECLARE_FIELD)
};

class ModState {
public:
    static ModState& Get();

    PlayerGeneralState playerGeneral;
    UiPrefsState       ui;
    DebugState         debug;
    CameraState        camera;
    UnlocksState       unlocks;
    WorldSessionState  world;
    ProgressionState   progression;

    bool SaveToFile() const;
    bool LoadFromFile();
    void ResetToDefaults();
};

}}

#endif
