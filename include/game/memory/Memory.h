#pragma once

#include <cstdint>
#include <cstddef>

namespace tsm { namespace game { namespace memory {

std::uintptr_t GetBase();


std::uint8_t ReadByte(std::uintptr_t rva);

std::uint32_t ReadU32(std::uintptr_t rva);

std::uint64_t ReadU64(std::uintptr_t rva);

float ReadFloat(std::uintptr_t rva);

double ReadDouble(std::uintptr_t rva);

template <typename T>
inline T Read(std::uintptr_t rva) {
    return *reinterpret_cast<T*>(GetBase() + rva);
}


bool WriteByte(std::uintptr_t rva, std::uint8_t value);

bool WriteU32(std::uintptr_t rva, std::uint32_t value);

bool WriteU64(std::uintptr_t rva, std::uint64_t value);

bool WriteFloat(std::uintptr_t rva, float value);

bool WriteDouble(std::uintptr_t rva, double value);

bool WriteBytes(void* address, const void* bytes, std::size_t size);

bool WriteBytesToRva(std::uintptr_t rva, const void* bytes, std::size_t size);

}}}
