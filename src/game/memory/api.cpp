#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <Cipher/CipherUtils.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/Patch.h>
#include <game/memory/api.h>
#include <game/memory/mem.h>
#include <game/memory/offsets.h>
#include <utils/common/vec3.h>

namespace tsm::game::api {


[[nodiscard]] bool PlayerUuid::IsValid() const noexcept {
    for (int i = 0; i < 16; ++i) {
        if (bytes[i] != 0) return true;
    }
    return false;
}

[[nodiscard]] std::string PlayerUuid::ToString() const {
    char buf[64];
    std::snprintf(buf, sizeof(buf),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
    );
    return std::string(buf);
}


namespace detail {
    std::array<std::uintptr_t, kMaxAvatars> s_avatarCache{};
    bool s_avatarCacheValid = false;
}

void InvalidateAvatarCache() noexcept {
    detail::s_avatarCacheValid = false;
}

void RefreshAvatarCache() {
    auto barn = AvatarBarn();
    if (!barn) {
        detail::s_avatarCacheValid = false;
        return;
    }

    const std::uintptr_t barnAddr = reinterpret_cast<std::uintptr_t>(barn);

    for (int i = 0; i < kMaxAvatars; ++i) {
        detail::s_avatarCache[i] = barnAddr +
            Offsets::kAvatarSlotStride * i +
            Offsets::kAvatarLocalSlot;
    }

    detail::s_avatarCacheValid = true;
}

[[nodiscard]] std::uintptr_t GetCachedAvatar(int index) {
    if (index < 0 || index >= kMaxAvatars) return 0;

    if (!detail::s_avatarCacheValid) {
        RefreshAvatarCache();
    }

    return detail::s_avatarCache[index];
}


[[nodiscard]] void* Game() {
    return mem::read_ptr_rva(Offsets::Game);
}

[[nodiscard]] void* LuaState() {
    auto g = Game();
    if (!g) return nullptr;
    return *reinterpret_cast<void**>(mem::add(g, Offsets::kLuaState));
}

[[nodiscard]] void* AvatarBarn() {
    auto g = Game();
    if (!g) return nullptr;
    return *reinterpret_cast<void**>(mem::add(g, Offsets::kAvatarBarn));
}

[[nodiscard]] void* NetPlayerBarn() {
    auto g = Game();
    if (!g) return nullptr;

    void* ptr = *reinterpret_cast<void**>(mem::add(g, Offsets::kNetPlayerBarnPtr));
    if (!ptr) return nullptr;

    return *reinterpret_cast<void**>(mem::add(ptr, Offsets::kNetPlayerBarnOffset));
}

[[nodiscard]] const char* LevelName() {
    auto g = Game();
    if (!g) return nullptr;
    return *reinterpret_cast<const char**>(mem::add(g, Offsets::kLevelName));
}


[[nodiscard]] void* LocalAvatar(int index) {
    auto barn = AvatarBarn();
    if (!barn) return nullptr;

    const std::uintptr_t slot = Offsets::kAvatarLocalSlot +
        static_cast<std::uintptr_t>(index) * Offsets::kAvatarSlotStride;

    return *reinterpret_cast<void**>(mem::add(barn, slot));
}

[[nodiscard]] void* LocalAvatarOutfit(int index) {
    auto barn = AvatarBarn();
    if (!barn) return nullptr;

    const std::uintptr_t offset =
        static_cast<std::uintptr_t>(Offsets::kAvatarLocalSlot) +
        static_cast<std::uintptr_t>(index) * Offsets::kAvatarSlotStride +
        static_cast<std::uintptr_t>(Offsets::kAvatarOutfit);

    const std::uintptr_t fieldAddr = reinterpret_cast<std::uintptr_t>(barn) + offset;
    const std::uintptr_t p = *reinterpret_cast<std::uintptr_t*>(fieldAddr);

    return p ? reinterpret_cast<void*>(p) : nullptr;
}

[[nodiscard]] vec3 LocalAvatarPosition(int index) {
    auto barn = AvatarBarn();
    if (!barn) return vec3{0.0f, 0.0f, 0.0f};

    const std::uintptr_t offset =
        static_cast<std::uintptr_t>(Offsets::kAvatarLocalSlot) +
        static_cast<std::uintptr_t>(index) * Offsets::kAvatarSlotStride +
        static_cast<std::uintptr_t>(Offsets::kAvatarPosition);

    const std::uintptr_t fieldAddr = reinterpret_cast<std::uintptr_t>(barn) + offset;
    float* p = *reinterpret_cast<float**>(fieldAddr);

    if (!p) return vec3{0.0f, 0.0f, 0.0f};
    return vec3{p[0], p[1], p[2]};
}

[[nodiscard]] float LocalAvatarRotation(int index) {
    auto barn = AvatarBarn();
    if (!barn) return 0.0f;

    const std::uintptr_t offset =
        static_cast<std::uintptr_t>(Offsets::kAvatarLocalSlot) +
        static_cast<std::uintptr_t>(index) * Offsets::kAvatarSlotStride +
        static_cast<std::uintptr_t>(Offsets::kAvatarPosition);

    const std::uintptr_t fieldAddr = reinterpret_cast<std::uintptr_t>(barn) + offset;
    float* p = *reinterpret_cast<float**>(fieldAddr);

    if (!p) return 0.0f;
    return p[8];
}

[[nodiscard]] float* LocalAvatarPositionRawPtr(int index) {
    auto barn = AvatarBarn();
    if (!barn) return nullptr;

    const std::uintptr_t offset =
        static_cast<std::uintptr_t>(Offsets::kAvatarLocalSlot) +
        static_cast<std::uintptr_t>(index) * Offsets::kAvatarSlotStride +
        static_cast<std::uintptr_t>(Offsets::kAvatarPosition);

    const std::uintptr_t fieldAddr = reinterpret_cast<std::uintptr_t>(barn) + offset;
    return *reinterpret_cast<float**>(fieldAddr);
}

[[nodiscard]] bool ShouldDisplay(std::uintptr_t avatar) {
    if (avatar == 0) return false;

    using ShouldDisplayFunc = bool(*)(std::uintptr_t);
    const std::uintptr_t funcAddr = tsm::game::memory::GetBase() + Offsets::kShouldDisplay;
    auto func = reinterpret_cast<ShouldDisplayFunc>(funcAddr);

    return func(avatar);
}

[[nodiscard]] AvatarInfo GetAvatarInfo(std::uintptr_t avatar) {
    AvatarInfo info{};
    if (avatar == 0) return info;

    try {
        const std::uintptr_t outfitPtr =
            *reinterpret_cast<std::uintptr_t*>(avatar + Offsets::kAvatarOutfit);

        const std::uintptr_t posPtr =
            *reinterpret_cast<std::uintptr_t*>(avatar + Offsets::kAvatarPosition);

        if (posPtr != 0) {
            info.avatarPosition = reinterpret_cast<vec3*>(posPtr);
        }

        info.avatarShout =
            *reinterpret_cast<std::uintptr_t*>(avatar + Offsets::kAvatarShout);

        if (outfitPtr != 0) {
            info.avatarHeight = reinterpret_cast<float*>(outfitPtr + Offsets::kHeight);
            info.avatarScale = reinterpret_cast<float*>(outfitPtr + Offsets::kScale);
            info.avatarVoice = reinterpret_cast<std::uint8_t*>(outfitPtr + Offsets::kVoice);
            info.avatarStance = reinterpret_cast<std::uint8_t*>(outfitPtr + Offsets::kStance);
            info.avatarHair = reinterpret_cast<const char*>(outfitPtr + Offsets::kHair);
            info.avatarHat = reinterpret_cast<const char*>(outfitPtr + Offsets::kHat);
            info.avatarMask = reinterpret_cast<const char*>(outfitPtr + Offsets::kMask);
            info.avatarFace = reinterpret_cast<const char*>(outfitPtr + Offsets::kFace);
            info.avatarNeck = reinterpret_cast<const char*>(outfitPtr + Offsets::kNeck);
            info.avatarBody = reinterpret_cast<const char*>(outfitPtr + Offsets::kBody);
            info.avatarFeet = reinterpret_cast<const char*>(outfitPtr + Offsets::kFeet);
            info.avatarWing = reinterpret_cast<const char*>(outfitPtr + Offsets::kWing);
            info.avatarProp = reinterpret_cast<const char*>(outfitPtr + Offsets::kProp);
        }
    }
    catch (...) {
    }

    return info;
}

[[nodiscard]] AvatarInfo GetAvatarInfoByIndex(int index) {
    const std::uintptr_t avatar = GetCachedAvatar(index);
    return GetAvatarInfo(avatar);
}


[[nodiscard]] int GetPlayerIdByIndex(int index) {
    if (index < 0 || index >= kMaxAvatars) return 0;

    void* npb = NetPlayerBarn();
    if (!npb) return 0;

    const std::uintptr_t playerIdAddr = mem::add(npb,
        Offsets::kFirstPlayerIdOffset + (Offsets::kPlayerIdStride * index));

    return *reinterpret_cast<int*>(playerIdAddr);
}


[[nodiscard]] float* GameSpeedDeltaPtr() {
    void* pBarnLoc = tsm::game::memory::RvaToPtr(Offsets::AudienceBarn);
    if (!pBarnLoc) return nullptr;

    void* barn = *reinterpret_cast<void**>(pBarnLoc);
    if (!barn) return nullptr;

    void* gs = *reinterpret_cast<void**>(
        reinterpret_cast<char*>(barn) + Offsets::kGameSpeedBarn);
    if (!gs) return nullptr;

    return reinterpret_cast<float*>(
        reinterpret_cast<char*>(gs) + Offsets::kGameSpeedDelta);
}

[[nodiscard]] float GameSpeed() {
    if (float* p = GameSpeedDeltaPtr()) {
        return *p;
    }
    return 1.0f;
}

void SetGameSpeed(float speed) {
    if (float* p = GameSpeedDeltaPtr()) {
        tsm::game::memory::WriteBytes(p, &speed, sizeof(float));
    }
}


[[nodiscard]] std::uintptr_t CameraSystem() {
    void* pBarnLoc = tsm::game::memory::RvaToPtr(Offsets::AudienceBarn);
    if (!pBarnLoc) return 0;

    void* audience = *reinterpret_cast<void**>(pBarnLoc);
    if (!audience) return 0;

    void* camSys = *reinterpret_cast<void**>(mem::add(audience, Offsets::kCameraSystem));
    if (!camSys) return 0;

    return reinterpret_cast<std::uintptr_t>(camSys);
}

[[nodiscard]] std::uintptr_t WhiskerCamera() {
    const std::uintptr_t camSys = CameraSystem();
    if (camSys == 0) return 0;

    return camSys + Offsets::kWhiskerCamera;
}


namespace {
    void* GetCandleBarnInternal() {
        void* pBarnLoc = tsm::game::memory::RvaToPtr(Offsets::CandleBarn);
        if (!pBarnLoc) return nullptr;
        void* barn = *reinterpret_cast<void**>(pBarnLoc);
        return barn;
    }
}

[[nodiscard]] int CandleCount() {
    void* barn = GetCandleBarnInternal();
    if (!barn) return 0;
    int* pCount = reinterpret_cast<int*>(reinterpret_cast<char*>(barn) + 0x5A010);
    if (!pCount) return 0;
    int count = *pCount;
    if (count < 0) return 0;
    return count;
}

[[nodiscard]] std::uintptr_t FirstCandle() {
    void* barn = GetCandleBarnInternal();
    if (!barn) return 0;
    return reinterpret_cast<std::uintptr_t>(reinterpret_cast<char*>(barn) + 0x40);
}

[[nodiscard]] int CandleTypeIndex(int index) {
    void* barn = GetCandleBarnInternal();
    if (!barn) return -1;
    if (index < 0) return -1;
    int count = CandleCount();
    if (index >= count) return -1;
    char* base = reinterpret_cast<char*>(barn);
    std::uintptr_t entryOffset = 0x1F810u + static_cast<std::uintptr_t>(index) * 0xD0u;
    std::uint8_t* pType = reinterpret_cast<std::uint8_t*>(base + entryOffset + 0x9Cu);
    if (!pType) return -1;
    return static_cast<int>(*pType);
}

[[nodiscard]] bool CandleIsActive(int index) {
    void* barn = GetCandleBarnInternal();
    if (!barn) return false;
    if (index < 0) return false;
    int count = CandleCount();
    if (index >= count) return false;
    char* base = reinterpret_cast<char*>(barn);
    std::uintptr_t entryOffset = 0x1F810u + static_cast<std::uintptr_t>(index) * 0xD0u;
    char* entry = base + entryOffset;
    void* node = *reinterpret_cast<void**>(entry + 0x90u);
    if (!node) return false;
    char* nodeBase = reinterpret_cast<char*>(node);
    std::uint16_t flags = *reinterpret_cast<std::uint16_t*>(nodeBase + 97);
    return (flags & 0x30u) == 0x30u;
}

}
