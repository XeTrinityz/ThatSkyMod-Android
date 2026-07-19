#include <features/manager/FeatureManager.h>
#include <state/ModState.h>
#include <game/memory/Patch.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <vector>

namespace tsm { namespace features {

namespace {
    using namespace tsm::game::memory;
    using namespace tsm::game;

    std::vector<tsm::game::memory::Patch> g_unlockAllP1;
    std::vector<tsm::game::memory::Patch> g_unlockAllP2;
    tsm::game::memory::Patch g_emoteLevelsPatch{};

    bool g_prevUnlockAll = false;
    bool g_prevEmoteLevels = false;

    void apply_patches(std::vector<tsm::game::memory::Patch>& patches) {
        for (auto& p : patches) {
            if (!p.applied) p.Apply();
        }
    }

    void restore_patches(std::vector<tsm::game::memory::Patch>& patches) {
        for (auto& p : patches) {
            if (p.address) p.Restore();
        }
        patches.clear();
    }

    bool apply_unlock_all() {
        EnsureInitialized();

        if (!g_unlockAllP1.empty() || !g_unlockAllP2.empty()) {
            apply_patches(g_unlockAllP1);
            apply_patches(g_unlockAllP2);
            return true;
        }

        using namespace tsm::game::Signatures;
        std::vector<const char*> pats1 = { kUnlockAllPattern1 };
        if (tsm::game::memory::CreateNopPatchesForPattern(pats1[0], g_unlockAllP1, 1) == 0 || g_unlockAllP1.empty()) {
            return false;
        }
        apply_patches(g_unlockAllP1);

        std::vector<const char*> pats2 = { kUnlockAllPattern2 };
        if (tsm::game::memory::CreateNopPatchesForPattern(pats2[0], g_unlockAllP2, 1) == 0 || g_unlockAllP2.empty()) {
            restore_patches(g_unlockAllP1);
            return false;
        }
        apply_patches(g_unlockAllP2);
        return true;
    }

    void restore_unlock_all() {
        restore_patches(g_unlockAllP1);
        restore_patches(g_unlockAllP2);
    }

    bool apply_emote_levels() {
        EnsureInitialized();

        if (g_emoteLevelsPatch.address) {
            if (!g_emoteLevelsPatch.applied) {
                g_emoteLevelsPatch.Apply();
            }
            return true;
        }

        std::vector<tsm::game::memory::Patch> hits;
        using namespace tsm::game::Signatures;
        std::vector<const char*> pats = { kEmoteLevelsPattern };
        if (tsm::game::memory::CreateNopPatchesForPattern(pats[0], hits, 1) > 0 && !hits.empty()) {
            g_emoteLevelsPatch = hits.front();
            g_emoteLevelsPatch.Apply();
            return true;
        }
        return false;
    }

    void restore_emote_levels() {
        if (g_emoteLevelsPatch.address) {
            g_emoteLevelsPatch.Restore();
        }
        g_emoteLevelsPatch = tsm::game::memory::Patch{};
    }
}

void FeatureManager::ApplyUnlockFeatures() {
    auto& state = tsm::state::ModState::Get();
    auto& unlocks = state.unlocks;

    bool wantUnlockAll = unlocks.unlockAll;
    bool wantEmotes = unlocks.unlockEmoteLevels;

    if (!wantUnlockAll && wantEmotes) {
        if (g_prevUnlockAll && g_prevEmoteLevels) {
            unlocks.unlockEmoteLevels = false;
            wantEmotes = false;
        } else {
            if (apply_unlock_all()) {
                unlocks.unlockAll = true;
                wantUnlockAll = true;
            } else {
                unlocks.unlockEmoteLevels = false;
                wantEmotes = false;
            }
        }
    }

    if (wantEmotes) {
        if (!apply_emote_levels()) {
            unlocks.unlockEmoteLevels = false;
            wantEmotes = false;
        }
    } else {
        restore_emote_levels();
    }

    if (wantUnlockAll) {
        if (!apply_unlock_all()) {
            unlocks.unlockAll = false;
            wantUnlockAll = false;
        }
    } else {
        restore_unlock_all();
    }

    EnsureInitialized();
    WriteByte(Offsets::kEnableAllRelationshipAbilities,
               unlocks.unlockRelationshipAbilities ? 1 : 0);

    g_prevUnlockAll = unlocks.unlockAll;
    g_prevEmoteLevels = unlocks.unlockEmoteLevels;
}

}}
