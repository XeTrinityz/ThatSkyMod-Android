#include <features/manager/FeatureManager.h>
#include <state/ModState.h>
#include <game/memory/Address.h>
#include <game/memory/Patch.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <game/memory/api.h>
#include <vector>

namespace tsm { namespace features {

namespace {
    using namespace tsm::game::memory;
    using namespace tsm::game;

    tsm::game::memory::Patch g_camLockPatch{};
    tsm::game::memory::Patch g_freeZoomPatch{};
}

namespace camera {

bool ReadCameraAngleX(float& outValue) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        outValue = *reinterpret_cast<float*>(whisker + Offsets::kCameraAngleX);
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteCameraAngleX(float value) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        void* addr = reinterpret_cast<void*>(whisker + Offsets::kCameraAngleX);
        return WriteBytes(addr, &value, sizeof(float));
    } catch (...) {
        return false;
    }
}

bool ReadCameraAngleY(float& outValue) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        outValue = *reinterpret_cast<float*>(whisker + Offsets::kCameraAngleY);
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteCameraAngleY(float value) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        void* addr = reinterpret_cast<void*>(whisker + Offsets::kCameraAngleY);
        return WriteBytes(addr, &value, sizeof(float));
    } catch (...) {
        return false;
    }
}

bool ReadCameraRotation(float& outValue) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        outValue = *reinterpret_cast<float*>(whisker + Offsets::kCameraRotation);
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteCameraRotation(float value) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        void* addr = reinterpret_cast<void*>(whisker + Offsets::kCameraRotation);
        return WriteBytes(addr, &value, sizeof(float));
    } catch (...) {
        return false;
    }
}

bool ReadCameraFOV(float& outValue) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        outValue = *reinterpret_cast<float*>(whisker + Offsets::kCameraFOV);
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteCameraFOV(float value) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        void* addr = reinterpret_cast<void*>(whisker + Offsets::kCameraFOV);
        return WriteBytes(addr, &value, sizeof(float));
    } catch (...) {
        return false;
    }
}

bool ReadCameraZoom(float& outValue) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        outValue = *reinterpret_cast<float*>(whisker + Offsets::kCameraZoom);
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteCameraZoom(float value) {
    try {
        std::uintptr_t whisker = tsm::game::api::WhiskerCamera();
        if (whisker == 0) return false;
        void* addr = reinterpret_cast<void*>(whisker + Offsets::kCameraZoom);
        return WriteBytes(addr, &value, sizeof(float));
    } catch (...) {
        return false;
    }
}

}

void FeatureManager::ApplyCameraFeatures() {
    auto& cam = tsm::state::ModState::Get().camera;
    EnsureInitialized();

    WriteByte(Offsets::kEnableGameCamSnap, cam.disableSnap ? 0 : 1);

    if (cam.lockCamPos) {
        if (!g_camLockPatch.address) {
            std::vector<tsm::game::memory::Patch> hits;
            if (tsm::game::memory::CreateNopPatchesForPattern(kLockCamPositionPattern, hits, 1) > 0 && !hits.empty()) {
                g_camLockPatch = hits.front();
            }
        }
        g_camLockPatch.Apply();
    } else {
        if (g_camLockPatch.address) g_camLockPatch.Restore();
    }

    if (cam.freeZoom) {
        if (!g_freeZoomPatch.address) {
            std::vector<tsm::game::memory::Patch> hits;
            if (tsm::game::memory::CreateNopPatchesForPattern(kFreeZoomPattern, hits, 1) > 0 && !hits.empty()) {
                g_freeZoomPatch = hits.front();
            }
        }
        g_freeZoomPatch.Apply();
    } else {
        if (g_freeZoomPatch.address) g_freeZoomPatch.Restore();
    }
}

}}
