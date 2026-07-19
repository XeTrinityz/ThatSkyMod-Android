#pragma once

#include <imgui/imgui.h>
#include <string>
#include <vector>

namespace tsm { namespace ui { namespace helpers {


enum class ToastType {
    Success,
    Info,
    Warning,
    Error,
    Loading
};

struct ToastConfig {
    ToastType type = ToastType::Info;
    float duration = 3.0f;
    bool show_icon = true;
    bool auto_dismiss = true;
    float slide_distance = 100.0f;

    static ToastConfig Quick() { return {ToastType::Info, 1.5f, true, true, 100.0f}; }
    static ToastConfig Standard() { return {ToastType::Info, 3.0f, true, true, 100.0f}; }
    static ToastConfig Long() { return {ToastType::Info, 5.0f, true, true, 100.0f}; }
};

struct Toast {
    std::string message;
    std::string title;
    ToastConfig config;
    float lifetime;
    float max_lifetime;
    bool active;
    float slide_progress;
};

class ToastManager {
public:
    static ToastManager& Get();

    void Show(const std::string& message, ToastType type = ToastType::Info, float duration = 3.0f);

    void ShowWithTitle(const std::string& title, const std::string& message,
                       ToastType type = ToastType::Info, float duration = 3.0f);

    void ShowCustom(const std::string& message, const ToastConfig& config);
    void ShowCustomWithTitle(const std::string& title, const std::string& message,
                             const ToastConfig& config);

    void Success(const std::string& message, float duration = 3.0f);
    void Info(const std::string& message, float duration = 3.0f);
    void Warning(const std::string& message, float duration = 3.0f);
    void Error(const std::string& message, float duration = 3.0f);

    void Update(float dt);
    void Render();

    void Clear();

    void SetMaxToasts(int max) { max_toasts_ = max; }
    void SetPosition(const ImVec2& position) { position_ = position; }
    void SetSpacing(float spacing) { spacing_ = spacing; }

private:
    ToastManager();

    void AddToast(const Toast& toast);
    ImVec4 GetTypeColor(ToastType type) const;
    const char* GetTypeIcon(ToastType type) const;

    std::vector<Toast> toasts_;
    int max_toasts_ = 5;
    ImVec2 position_ = ImVec2(-1, -1);
    float spacing_ = 10.0f;
};

inline void ShowToast(const std::string& message, ToastType type = ToastType::Info, float duration = 3.0f) {
    ToastManager::Get().Show(message, type, duration);
}

inline void ShowToastSuccess(const std::string& message, float duration = 3.0f) {
    ToastManager::Get().Success(message, duration);
}

inline void ShowToastInfo(const std::string& message, float duration = 3.0f) {
    ToastManager::Get().Info(message, duration);
}

inline void ShowToastWarning(const std::string& message, float duration = 3.0f) {
    ToastManager::Get().Warning(message, duration);
}

inline void ShowToastError(const std::string& message, float duration = 3.0f) {
    ToastManager::Get().Error(message, duration);
}

}}}
