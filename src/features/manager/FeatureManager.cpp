#include <features/manager/FeatureManager.h>
#include <state/ModState.h>
#include <game/memory/Patch.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <game/interop/LuaHelpers.h>
#include <progression/AutoWaxTools.h>
#include <Cipher/Cipher.h>
#include <vector>

namespace tsm { namespace features {

namespace {
    using namespace tsm::game::memory;
    using namespace tsm::game;
    using namespace tsm::game::Signatures;

    constexpr float kSuperRunSpeedEnabled = 20.0f;
    constexpr float kSuperRunSpeedDisabled = 3.25f;
    constexpr int kSuperFlightScanLimit = 1;
	constexpr std::uint64_t kSuperFlightPatchValue = 0xB900A26D52A8590DULL;

    std::vector<tsm::game::memory::Patch> g_superFlightPatches;
    bool g_superFlightPatchesCreated = false;

    void restore_patches(std::vector<tsm::game::memory::Patch>& patches) {
        for (auto& p : patches) {
            if (p.address) p.Restore();
        }
    }

    bool apply_super_flight() {
        EnsureInitialized();

        if (!g_superFlightPatchesCreated) {
            g_superFlightPatches.clear();

			std::uintptr_t addr = Cipher::CipherScanIdaPattern(kSuperFlight);
			if (addr == 0) {
				return false;
			}

			g_superFlightPatches.emplace_back(CreatePatch(reinterpret_cast<void*>(addr), &kSuperFlightPatchValue, sizeof(kSuperFlightPatchValue)));
			g_superFlightPatchesCreated = true;
        }

        bool anyApplied = false;
        for (auto& p : g_superFlightPatches) {
            if (p.Apply()) {
                anyApplied = true;
            }
        }

        if (!anyApplied) {
            return false;
        }
        return true;
    }

    void restore_super_flight() {
        restore_patches(g_superFlightPatches);
    }
}

FeatureManager& FeatureManager::Get() {
    static FeatureManager instance;
    return instance;
}

void FeatureManager::ApplyFeature(FeatureId id, bool enabled) {
    auto& state = tsm::state::ModState::Get();
    EnsureInitialized();

    switch (id) {
        case FeatureId::Invincibility:
            state.playerGeneral.invincibility = enabled;
            WriteByte(Offsets::kInvincibility, enabled ? 1 : 0);
            break;
        case FeatureId::AntiRainDrain:
            state.playerGeneral.antiRainDrain = enabled;
            WriteByte(Offsets::kDisableRainDrain, enabled ? 1 : 0);
            break;
        case FeatureId::AntiKrill:
            state.playerGeneral.antiKrill = enabled;
            WriteByte(Offsets::kDarkCreatureTame, enabled ? 1 : 0);
            break;
        case FeatureId::AntiAFK:
            state.playerGeneral.antiAFK = enabled;
            WriteByte(Offsets::kAllowAfk, enabled ? 0 : 1);
            break;
        case FeatureId::AutoCharge:
            state.playerGeneral.autoCharge = enabled;
            WriteByte(Offsets::kAutoCharge, enabled ? 1 : 0);
            break;
        case FeatureId::SuperRun:
            state.playerGeneral.superRun = enabled;
            WriteFloat(Offsets::kRunSpeed, enabled ? kSuperRunSpeedEnabled : kSuperRunSpeedDisabled);
            break;
        case FeatureId::SuperLaunch:
            state.playerGeneral.superLaunch = enabled;
            WriteFloat(Offsets::kSuperLaunch, enabled ? 2.0f : 1.0f);
            break;
        case FeatureId::SuperSlidey:
            state.playerGeneral.superSlidey = enabled;
            WriteByte(Offsets::kSuperSlidey, enabled ? 1 : 0);
            break;
        case FeatureId::SuperFlight:
            if (enabled) {
                if (!apply_super_flight()) {
                    state.playerGeneral.superFlight = false;
                    return;
                }
            } else {
                restore_super_flight();
            }
            state.playerGeneral.superFlight = enabled;
            break;
        case FeatureId::FastFlap: {
            state.playerGeneral.fastFlap = enabled;
            constexpr std::uint32_t kFastFlapPatch = 0x1E349000u;
            static std::uint32_t s_fastFlapOriginal = 0;

            if (enabled) {
                if (s_fastFlapOriginal == 0) {
                    s_fastFlapOriginal = ReadU32(Offsets::kFastFlap);
                }
                WriteU32(Offsets::kFastFlap, kFastFlapPatch);
            } else {
                if (s_fastFlapOriginal != 0) {
                    WriteU32(Offsets::kFastFlap, s_fastFlapOriginal);
                }
            }
            break;
        }
        case FeatureId::UnlimitedFireworks: {
            state.playerGeneral.unlimitedFireworks = enabled;
            constexpr std::uint32_t kFireworksNOP = 0xD503201Fu;
            static std::uint32_t s_fireworksOriginal = 0;
            static bool s_fireworksOrigSet = false;

            if (enabled) {
                if (!s_fireworksOrigSet) {
                    s_fireworksOriginal = ReadU32(Offsets::kFireworksCooldown);
                    s_fireworksOrigSet = true;
                }
                WriteU32(Offsets::kFireworksCooldown, kFireworksNOP);
            } else {
                if (s_fireworksOrigSet) {
                    WriteU32(Offsets::kFireworksCooldown, s_fireworksOriginal);
                    s_fireworksOrigSet = false;
                }
            }
            break;
        }
        case FeatureId::UnlockAll:
            state.unlocks.unlockAll = enabled;
            ApplyUnlockFeatures();
            break;
        case FeatureId::UnlockEmoteLevels:
            state.unlocks.unlockEmoteLevels = enabled;
            ApplyUnlockFeatures();
            break;
        case FeatureId::UnlockRelationshipAbilities:
            state.unlocks.unlockRelationshipAbilities = enabled;
            ApplyUnlockFeatures();
            break;
        case FeatureId::AutoCollectWax:
            tsm::progression::SetAutoCollectWaxEnabled(enabled);
            break;
        case FeatureId::AutoBurnCandles:
            tsm::progression::SetAutoBurnCandlesEnabled(enabled);
            break;
        case FeatureId::AutoBurnPlants:
            tsm::progression::SetAutoBurnPlantsEnabled(enabled);
            break;
        case FeatureId::FastHome:
            state.world.fastHome = enabled;
            ApplyWorldFeatures();
            break;
        case FeatureId::ShowModdedOutfits:
            state.world.showModdedOutfits = enabled;
            ApplyWorldFeatures();
            break;
        case FeatureId::LightAllPlayers: {
            std::uint8_t b = enabled ? 0 : 1;
            WriteByte(Offsets::kAvatarCharcoaling, b);
            break;
        }
        case FeatureId::RevealAllPlayers: {
            static std::uint32_t s_revealPlayersOriginal = 0;
            static bool s_revealPlayersOrigSet = false;

            if (enabled) {
                if (!s_revealPlayersOrigSet) {
                    s_revealPlayersOriginal = ReadU32(Offsets::kRevealPlayers);
                    s_revealPlayersOrigSet = true;
                }
                WriteU32(Offsets::kRevealPlayers, Signatures::kNopInstruction);
            } else {
                if (s_revealPlayersOrigSet) {
                    WriteU32(Offsets::kRevealPlayers, s_revealPlayersOriginal);
                    s_revealPlayersOrigSet = false;
                }
            }
            break;
        }
        case FeatureId::EspPlayers:
            state.debug.espShowPlayers = enabled;
            break;
        case FeatureId::EspNPCs:
            state.debug.espShowNPCs = enabled;
            break;
        case FeatureId::EspWingLights:
            state.debug.espShowWingLights = enabled;
            break;
        case FeatureId::EspDyes:
            state.debug.espShowDyes = enabled;
            break;
        case FeatureId::EspCandles:
            state.debug.espShowCandles = enabled;
            break;
    }
}

void FeatureManager::ApplyPlayerFeatures() {
    auto& pg = tsm::state::ModState::Get().playerGeneral;
    ApplyFeature(FeatureId::Invincibility, pg.invincibility);
    ApplyFeature(FeatureId::AntiRainDrain, pg.antiRainDrain);
    ApplyFeature(FeatureId::AntiKrill, pg.antiKrill);
    ApplyFeature(FeatureId::AntiAFK, pg.antiAFK);
    ApplyFeature(FeatureId::AutoCharge, pg.autoCharge);
    ApplyFeature(FeatureId::SuperRun, pg.superRun);
    ApplyFeature(FeatureId::SuperSlidey, pg.superSlidey);
    ApplyFeature(FeatureId::SuperFlight, pg.superFlight);
    ApplyFeature(FeatureId::SuperLaunch, pg.superLaunch);
    ApplyFeature(FeatureId::FastFlap, pg.fastFlap);
    ApplyFeature(FeatureId::UnlimitedFireworks, pg.unlimitedFireworks);
}

void FeatureManager::ApplyAllFeatures() {
    ApplyPlayerFeatures();
    ApplyCameraFeatures();
    ApplyUnlockFeatures();
    ApplyWorldFeatures();
}

}}
