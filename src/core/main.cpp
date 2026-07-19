#include "core/main.h"
#include <core/InitManager.h>
#include <ui/core/App.h>
#include <ui/overlays/DebugOverlay.h>
#include <ui/overlays/SkyClockOverlay.h>
#include <ui/overlays/ESPOverlay.h>

namespace {
    void RenderPersistentOverlays() {
        tsm::ui::overlays::RenderDebugOverlay();
        tsm::ui::overlays::RenderSkyClock();
        tsm::ui::overlays::RenderESP();
    }
}

void Menu(bool* menu_open) {
    auto& initMgr = tsm::core::InitManager::Get();

    if (!initMgr.IsEarlyInitialized() || !initMgr.IsLateInitialized()) {
        return;
    }

    if (!initMgr.IsUIInitialized()) {
        initMgr.InitializeUI();

        if (!initMgr.AreHooksInstalled()) {
            initMgr.InstallHooks();
        }
    }

    RenderPersistentOverlays();

    if (menu_open && *menu_open) {
        tsm::ui::Draw(menu_open);
    }
}

void Init() {
    tsm::core::InitManager::Get().PerformEarlyInit();
}

void InitLate() {
    tsm::core::InitManager::Get().PerformLateInit();
}

func Start() {
    Init();
    return &Menu;
}
