#include <features/manager/FeatureManager.h>
#include <state/ModState.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <game/interop/LuaScriptQueue.h>

namespace tsm { namespace features {

namespace {
    using namespace tsm::game::memory;
    using namespace tsm::game;

    std::uint32_t g_fastHomeOrig = 0;
    bool g_fastHomeOrigSet = false;
    std::uint32_t g_windWallsOrig = 0;
    bool g_windWallsOrigSet = false;
    std::uint32_t g_readTableMessagesOrig = 0;
    bool g_readTableMessagesOrigSet = false;
}

void FeatureManager::ApplyWorldFeatures() {
    auto& world = tsm::state::ModState::Get().world;
    EnsureInitialized();

    WriteByte(Offsets::kDisableGates, world.disableSpiritGates ? 1 : 0);
    WriteByte(Offsets::kTguiPauseAnimation, world.pauseUiAnimations ? 1 : 0);

    if (world.fastHome) {
        if (!g_fastHomeOrigSet) {
            g_fastHomeOrig = ReadU32(Offsets::kFastHome);
            g_fastHomeOrigSet = true;
        }
        WriteU32(Offsets::kFastHome, Signatures::kNopInstruction);
    } else {
        if (g_fastHomeOrigSet) {
            WriteU32(Offsets::kFastHome, g_fastHomeOrig);
            g_fastHomeOrigSet = false;
        }
    }

    if (world.disableCutscenes) {
        tsm::lua::queue::Enqueue("Vars.Timeline.kAutoSkipAllTimelines(true)");
    } else {
        tsm::lua::queue::Enqueue("Vars.Timeline.kAutoSkipAllTimelines(false)");
    }

    if (world.disableWindWalls) {
        if (!g_windWallsOrigSet) {
            g_windWallsOrig = ReadU32(Offsets::kDisableWindWall);
            g_windWallsOrigSet = true;
        }
        WriteU32(Offsets::kDisableWindWall, Signatures::kMovW13Zero);
    } else {
        if (g_windWallsOrigSet) {
            WriteU32(Offsets::kDisableWindWall, g_windWallsOrig);
            g_windWallsOrigSet = false;
        }
    }

    if (world.readTableMessages) {
        if (!g_readTableMessagesOrigSet) {
            g_readTableMessagesOrig = ReadU32(Offsets::kReadTableMessages);
            g_readTableMessagesOrigSet = true;
        }
        WriteU32(Offsets::kReadTableMessages, Signatures::kNopInstruction);
    } else {
        if (g_readTableMessagesOrigSet) {
            WriteU32(Offsets::kReadTableMessages, g_readTableMessagesOrig);
            g_readTableMessagesOrigSet = false;
        }
    }

    WriteByte(Offsets::kDebugShowSpiritLocations, world.showSpirits ? 1 : 0);
    WriteByte(Offsets::kShowRadarForPreviousWingBuffs, world.showWingBuffs ? 1 : 0);
    WriteByte(Offsets::kEnableShrineRadar, world.showMapShrines ? 1 : 0);
    WriteByte(Offsets::kShowAllFeedback, world.showMapItems ? 1 : 0);
    WriteByte(Offsets::kDisableRemoteOutfitCache, world.showModdedOutfits ? 1 : 0);
}

}}
