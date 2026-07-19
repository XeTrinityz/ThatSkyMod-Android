#pragma once

#include <cstdint>
#include <string>

namespace tsm { namespace game { namespace hooks {

class HookManager {
public:
    static HookManager& Get();

    bool InstallAll();

    bool InstallAccountHooks();
    bool InstallProgressionHooks();
    bool InstallShoutHook();
    bool InstallCameraHook();
    bool InstallLuaHook();
    bool InstallMusicKeyHook();
    bool InstallPianoFrameHook();
    bool InstallMusicRecordHook();

    bool AreAllInstalled() const;
    bool IsAccountHooksInstalled() const;
    bool IsProgressionHooksInstalled() const;
    bool IsShoutHookInstalled() const;
    bool IsCameraHookInstalled() const;
    bool IsLuaHookInstalled() const;
    bool IsMusicKeyHookInstalled() const;
    bool IsPianoFrameHookInstalled() const;
    bool IsMusicRecordHookInstalled() const;

    const std::string& GetCapturedUserId() const;
    const std::string& GetCapturedSessionId() const;
    const std::string& GetCapturedUserAgent() const;

    std::uintptr_t GetCameraSystem() const;
    std::uintptr_t GetPlayerShout() const;
    std::uintptr_t GetWingBuffBarn() const;
    std::uintptr_t GetFirstWing() const;
    int* GetWingCountPtr() const;
    std::uintptr_t GetRadianceBarn() const;
    std::uintptr_t GetFirstDye() const;
    int* GetDyeCountPtr() const;

private:
    HookManager() = default;
    ~HookManager() = default;
    HookManager(const HookManager&) = delete;
    HookManager& operator=(const HookManager&) = delete;

    bool m_accountHooksInstalled = false;
    bool m_progressionHooksInstalled = false;
    bool m_shoutHookInstalled = false;
    bool m_cameraHookInstalled = false;
    bool m_luaHookInstalled = false;
    bool m_musicKeyHookInstalled = false;
    bool m_pianoFrameHookInstalled = false;
    bool m_musicRecordHookInstalled = false;
};

}}}
