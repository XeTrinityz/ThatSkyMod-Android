#pragma once

#include <string>
#include <cstdint>

namespace tsm { namespace utils { namespace hooking {

struct HookInfo {
    std::string name;
    void* target{nullptr};
    void* replacement{nullptr};
    void* original{nullptr};
    std::uintptr_t rva{0};
    bool installed{false};
};

bool install(void* target, void* replacement, void** original_out, const char* name = "");

bool install_rva(const char* name, std::uintptr_t rva, void* replacement, void** original_out);

template <typename FnPtr>
bool install_rva_typed(const char* name, std::uintptr_t rva, FnPtr replacement, FnPtr& original_out) {
    void* rep = reinterpret_cast<void*>(static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(replacement)));
    return install_rva(name, rva, rep, reinterpret_cast<void**>(&original_out));
}

template <typename FnPtr>
bool install_typed(void* target, FnPtr replacement, FnPtr& original_out, const char* name = nullptr) {
    void* rep = reinterpret_cast<void*>(static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(replacement)));
    return install(target, rep, reinterpret_cast<void**>(&original_out), name);
}

bool uninstall(void* target);

bool uninstall_name(const std::string& name);

void uninstall_all();

bool is_installed(void* target);
bool is_installed_name(const std::string& name);

}}}
