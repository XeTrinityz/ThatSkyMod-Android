#pragma once

namespace tsm { namespace core {

class InitManager {
public:
    static InitManager& Get();

    bool IsEarlyInitialized() const;
    bool IsLateInitialized() const;
    bool AreHooksInstalled() const;
    bool IsUIInitialized() const;

    void PerformEarlyInit();
    void PerformLateInit();
    bool InstallHooks();
    void InitializeUI();

private:
    InitManager() = default;
    ~InitManager() = default;
    InitManager(const InitManager&) = delete;
    InitManager& operator=(const InitManager&) = delete;

    bool m_earlyInit = false;
    bool m_lateInit = false;
    bool m_hooksInstalled = false;
    bool m_uiInitialized = false;
};

}}
