#pragma once

#include <cstdint>
#include <string>

namespace tsm { namespace game { namespace memory {

bool InitializeBase(const std::string& module_name = "libil2cpp.so");

std::uintptr_t GetBase();

inline void* RvaToPtr(std::uintptr_t rva) {
    return reinterpret_cast<void*>(GetBase() + rva);
}

template <typename T>
inline T* RvaToPtr(std::uintptr_t rva) {
    return reinterpret_cast<T*>(GetBase() + rva);
}

inline void EnsureInitialized() {
    if (GetBase() == 0) {
        InitializeBase();
    }
}

}}}
