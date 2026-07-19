#include <game/hooks/HookManager.h>
#include <game/hooks/AccountHooks.h>
#include <game/hooks/ProgressionHooks.h>
#include <game/hooks/ShoutHook.h>
#include <game/hooks/LuaHook.h>
#include <game/hooks/MusicKeyHook.h>
#include <game/hooks/PianoFrameHook.h>
#include <game/hooks/MusicRecordHook.h>
#include <game/memory/api.h>
#include <utils/logging/log.h>

namespace tsm { namespace game { namespace hooks {

HookManager& HookManager::Get() {
    static HookManager instance;
    return instance;
}

bool HookManager::InstallAll() {
    bool allSuccess = true;

    allSuccess = InstallAccountHooks() && allSuccess;
    allSuccess = InstallProgressionHooks() && allSuccess;
    allSuccess = InstallShoutHook() && allSuccess;
    allSuccess = InstallLuaHook() && allSuccess;
    allSuccess = InstallMusicKeyHook() && allSuccess;
    allSuccess = InstallPianoFrameHook() && allSuccess;
    allSuccess = InstallMusicRecordHook() && allSuccess;

    return allSuccess;
}

bool HookManager::InstallAccountHooks() {
    if (m_accountHooksInstalled) return true;

    bool success = account::Install();
    if (success) {
        m_accountHooksInstalled = true;
    }
    return success;
}

bool HookManager::InstallProgressionHooks() {
    if (m_progressionHooksInstalled) return true;

    bool success = progression::Install();
    if (success) {
        m_progressionHooksInstalled = true;
    }
    return success;
}

bool HookManager::InstallShoutHook() {
    if (m_shoutHookInstalled) return true;

    bool success = shout::Install();
    if (success) {
        m_shoutHookInstalled = true;
    }
    return success;
}

bool HookManager::InstallCameraHook() {
    if (m_cameraHookInstalled) return true;
    m_cameraHookInstalled = true;
    return true;
}

bool HookManager::InstallLuaHook() {
    if (m_luaHookInstalled) return true;

    bool success = lua::Install();
    if (success) {
        m_luaHookInstalled = true;
    }
    return success;
}

bool HookManager::InstallMusicKeyHook() {
    if (m_musicKeyHookInstalled) return true;

    bool success = musickey::Install();
    if (success) {
        m_musicKeyHookInstalled = true;
    }
    return success;
}

bool HookManager::InstallPianoFrameHook() {
    if (m_pianoFrameHookInstalled) return true;

    bool success = pianoframe::Install();
    if (success) {
        m_pianoFrameHookInstalled = true;
    }
    return success;
}

bool HookManager::InstallMusicRecordHook() {
    if (m_musicRecordHookInstalled) return true;

    bool success = musicrecord::Install();
    if (success) {
        m_musicRecordHookInstalled = true;
    }
    return success;
}

bool HookManager::AreAllInstalled() const {
    return m_accountHooksInstalled && m_progressionHooksInstalled &&
           m_shoutHookInstalled && m_cameraHookInstalled && m_luaHookInstalled &&
           m_musicKeyHookInstalled && m_pianoFrameHookInstalled && m_musicRecordHookInstalled;
}

bool HookManager::IsAccountHooksInstalled() const {
    return m_accountHooksInstalled;
}

bool HookManager::IsProgressionHooksInstalled() const {
    return m_progressionHooksInstalled;
}

bool HookManager::IsShoutHookInstalled() const {
    return m_shoutHookInstalled;
}

bool HookManager::IsCameraHookInstalled() const {
    return m_cameraHookInstalled;
}

bool HookManager::IsLuaHookInstalled() const {
    return m_luaHookInstalled;
}

bool HookManager::IsMusicKeyHookInstalled() const {
    return m_musicKeyHookInstalled;
}

bool HookManager::IsPianoFrameHookInstalled() const {
    return m_pianoFrameHookInstalled;
}

bool HookManager::IsMusicRecordHookInstalled() const {
    return m_musicRecordHookInstalled;
}

const std::string& HookManager::GetCapturedUserId() const {
    return account::GetCapturedUserId();
}

const std::string& HookManager::GetCapturedSessionId() const {
    return account::GetCapturedSessionId();
}

const std::string& HookManager::GetCapturedUserAgent() const {
    return account::GetCapturedUserAgent();
}

std::uintptr_t HookManager::GetCameraSystem() const {
    return tsm::game::api::CameraSystem();
}

std::uintptr_t HookManager::GetPlayerShout() const {
    return shout::GetPlayerShout();
}

std::uintptr_t HookManager::GetWingBuffBarn() const {
    return progression::GetWingBuffBarn();
}

std::uintptr_t HookManager::GetFirstWing() const {
    return progression::GetFirstWing();
}

int* HookManager::GetWingCountPtr() const {
    return progression::GetWingCountPtr();
}

std::uintptr_t HookManager::GetRadianceBarn() const {
    return progression::GetRadianceBarn();
}

std::uintptr_t HookManager::GetFirstDye() const {
    return progression::GetFirstDye();
}

int* HookManager::GetDyeCountPtr() const {
    return progression::GetDyeCountPtr();
}

}}}
