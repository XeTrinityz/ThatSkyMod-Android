#include <core/InitManager.h>
#include <core/UpdateChecker.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>
#include <game/hooks/HookManager.h>
#include <game/interop/LuaFunctions.h>
#include <data/DataManager.h>
#include <ui/core/App.h>
#include <utils/logging/log.h>

namespace tsm { namespace core {

InitManager& InitManager::Get() {
    static InitManager instance;
    return instance;
}

bool InitManager::IsEarlyInitialized() const {
    return m_earlyInit;
}

bool InitManager::IsLateInitialized() const {
    return m_lateInit;
}

bool InitManager::AreHooksInstalled() const {
    return m_hooksInstalled;
}

bool InitManager::IsUIInitialized() const {
    return m_uiInitialized;
}

void InitManager::PerformEarlyInit() {
    if (m_earlyInit) return;

    tsm::log::init("TSM");
    tsm::log::set_enabled(false);

    if (!tsm::game::hooks::HookManager::Get().InstallCameraHook()) {
        tsm::log::e("InitManager: CameraSystem hook install failed");
    } else {
        tsm::log::i("InitManager: CameraSystem hook installed");
    }

    m_earlyInit = true;
}

void InitManager::PerformLateInit() {
    if (m_lateInit) return;

    tsm::game::memory::InitializeBase();

    tsm::game::memory::WriteU32(tsm::game::Offsets::kStarwatchAuth, tsm::game::Signatures::kRetInstruction);

    tsm::lua::functions::InitializeCore();
    tsm::lua::functions::InitializeExtended();

    tsm::data::DataManager::Get().LoadAll();

    if (!m_hooksInstalled) {
        InstallHooks();
    }

    m_lateInit = true;
}

bool InitManager::InstallHooks() {
    if (m_hooksInstalled) return true;

    if (tsm::game::memory::GetBase() == 0) {
        tsm::game::memory::InitializeBase();
    }

    if (!tsm::game::hooks::HookManager::Get().InstallAll()) {
        tsm::log::e("InitManager: Game hooks install failed");
        return false;
    }

    tsm::log::i("InitManager: Game hooks installed successfully");
    m_hooksInstalled = true;
    return true;
}

void InitManager::InitializeUI() {
    if (m_uiInitialized) return;

    tsm::ui::Initialize();
    m_uiInitialized = true;
    tsm::log::i("InitManager: UI system initialized");

    tsm::core::UpdateChecker::Get().CheckForUpdates();
}

}}
