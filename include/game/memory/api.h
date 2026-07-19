#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <utils/common/vec3.h>

namespace tsm::game::api {


constexpr int kMaxAvatars = 60;
constexpr int kMaxPlayers = 8;

struct PlayerUuid;
struct AvatarInfo;


struct PlayerUuid {
    uint8_t bytes[16];

    [[nodiscard]] bool IsValid() const noexcept;

    [[nodiscard]] std::string ToString() const;
};

struct AvatarInfo {
    vec3* avatarPosition{};
    std::uintptr_t avatarShout{};
    float* avatarHeight{};
    float* avatarScale{};
    std::uint8_t* avatarVoice{};
    std::uint8_t* avatarStance{};
    const char* avatarHair{};
    const char* avatarHat{};
    const char* avatarMask{};
    const char* avatarFace{};
    const char* avatarNeck{};
    const char* avatarBody{};
    const char* avatarFeet{};
    const char* avatarWing{};
    const char* avatarProp{};
};


void InvalidateAvatarCache() noexcept;

void RefreshAvatarCache();

[[nodiscard]] std::uintptr_t GetCachedAvatar(int index);


[[nodiscard]] void* Game();

[[nodiscard]] void* LuaState();

[[nodiscard]] void* AvatarBarn();

[[nodiscard]] void* NetPlayerBarn();

[[nodiscard]] const char* LevelName();


[[nodiscard]] void* LocalAvatar(int index = 0);

[[nodiscard]] void* LocalAvatarOutfit(int index = 0);

[[nodiscard]] vec3 LocalAvatarPosition(int index = 0);

[[nodiscard]] float LocalAvatarRotation(int index = 0);

[[nodiscard]] float* LocalAvatarPositionRawPtr(int index = 0);

[[nodiscard]] bool ShouldDisplay(std::uintptr_t avatar);

[[nodiscard]] AvatarInfo GetAvatarInfo(std::uintptr_t avatar);

[[nodiscard]] AvatarInfo GetAvatarInfoByIndex(int index = 0);


[[nodiscard]] int GetPlayerIdByIndex(int index);


[[nodiscard]] float* GameSpeedDeltaPtr();

[[nodiscard]] float GameSpeed();

void SetGameSpeed(float speed);


[[nodiscard]] std::uintptr_t CameraSystem();

[[nodiscard]] std::uintptr_t WhiskerCamera();

[[nodiscard]] int CandleCount();

[[nodiscard]] std::uintptr_t FirstCandle();

[[nodiscard]] int CandleTypeIndex(int index);

[[nodiscard]] bool CandleIsActive(int index);

}
