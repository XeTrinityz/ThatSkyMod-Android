#include <game/memory/Memory.h>
#include <game/memory/Address.h>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <fstream>
#include <sstream>

namespace tsm { namespace game { namespace memory {

namespace {
    struct MemoryRegion {
        std::uintptr_t start{0};
        std::uintptr_t end{0};
        bool executable{false};
    };

    bool ReadMemoryRegions(std::uintptr_t addr, MemoryRegion& out_region) {
        std::ifstream maps("/proc/self/maps");
        if (!maps) return false;

        std::string line;
        while (std::getline(maps, line)) {
            std::istringstream iss(line);
            std::string addr_range, perms;
            if (!(iss >> addr_range >> perms)) continue;

            auto dash_pos = addr_range.find('-');
            if (dash_pos == std::string::npos) continue;

            try {
                std::uintptr_t start = std::stoull(addr_range.substr(0, dash_pos), nullptr, 16);
                std::uintptr_t end = std::stoull(addr_range.substr(dash_pos + 1), nullptr, 16);

                if (addr >= start && addr < end) {
                    out_region.start = start;
                    out_region.end = end;
                    out_region.executable = (perms.size() >= 3 && perms[2] == 'x');
                    return true;
                }
            } catch (...) {
                continue;
            }
        }
        return false;
    }

    bool MakeWritable(void* address, std::size_t size) {
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(address);
        std::uintptr_t page = addr & ~(getpagesize() - 1);
        std::uintptr_t span = (addr + size) - page;

        MemoryRegion region;
        bool has_exec = false;
        if (ReadMemoryRegions(addr, region)) {
            has_exec = region.executable;
        }

        int prot = PROT_READ | PROT_WRITE | (has_exec ? PROT_EXEC : 0);
        return mprotect(reinterpret_cast<void*>(page), span, prot) == 0;
    }
}


std::uint8_t ReadByte(std::uintptr_t rva) {
    return *reinterpret_cast<std::uint8_t*>(GetBase() + rva);
}

std::uint32_t ReadU32(std::uintptr_t rva) {
    return *reinterpret_cast<std::uint32_t*>(GetBase() + rva);
}

std::uint64_t ReadU64(std::uintptr_t rva) {
    return *reinterpret_cast<std::uint64_t*>(GetBase() + rva);
}

float ReadFloat(std::uintptr_t rva) {
    return *reinterpret_cast<float*>(GetBase() + rva);
}

double ReadDouble(std::uintptr_t rva) {
    return *reinterpret_cast<double*>(GetBase() + rva);
}


bool WriteBytes(void* address, const void* bytes, std::size_t size) {
    if (!address || !bytes || size == 0) return false;

    if (!MakeWritable(address, size)) return false;

    std::memcpy(address, bytes, size);

    __builtin___clear_cache(reinterpret_cast<char*>(address),
                            reinterpret_cast<char*>(address) + size);

    return true;
}

bool WriteBytesToRva(std::uintptr_t rva, const void* bytes, std::size_t size) {
    void* address = RvaToPtr(rva);
    return WriteBytes(address, bytes, size);
}

bool WriteByte(std::uintptr_t rva, std::uint8_t value) {
    return WriteBytesToRva(rva, &value, sizeof(value));
}

bool WriteU32(std::uintptr_t rva, std::uint32_t value) {
    return WriteBytesToRva(rva, &value, sizeof(value));
}

bool WriteU64(std::uintptr_t rva, std::uint64_t value) {
    return WriteBytesToRva(rva, &value, sizeof(value));
}

bool WriteFloat(std::uintptr_t rva, float value) {
    return WriteBytesToRva(rva, &value, sizeof(value));
}

bool WriteDouble(std::uintptr_t rva, double value) {
    return WriteBytesToRva(rva, &value, sizeof(value));
}

}}}
