#include <game/memory/Patch.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <Cipher/Cipher.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace tsm { namespace game { namespace memory {

namespace {
    struct MemoryRegion {
        std::uintptr_t start{0};
        std::uintptr_t end{0};
        bool executable{false};
        std::string path;
    };

    bool ReadMemoryMaps(std::vector<MemoryRegion>& out_regions) {
        out_regions.clear();
        std::ifstream maps("/proc/self/maps");
        if (!maps) return false;

        std::string line;
        while (std::getline(maps, line)) {
            std::istringstream iss(line);
            std::string addr_range, perms, offset, dev, inode, path;

            if (!(iss >> addr_range >> perms >> offset >> dev >> inode)) continue;
            std::getline(iss, path);
            if (!path.empty() && path[0] == ' ') path.erase(0, 1);

            auto dash_pos = addr_range.find('-');
            if (dash_pos == std::string::npos) continue;

            try {
                MemoryRegion region;
                region.start = std::stoull(addr_range.substr(0, dash_pos), nullptr, 16);
                region.end = std::stoull(addr_range.substr(dash_pos + 1), nullptr, 16);
                region.executable = (perms.size() >= 3 && perms[2] == 'x');
                region.path = path;
                out_regions.push_back(region);
            } catch (...) {
                continue;
            }
        }
        return !out_regions.empty();
    }

    bool ScanRegionForQword(const MemoryRegion& region, std::uint64_t value, std::vector<std::uintptr_t>& out_addresses, int max_results) {
        if (region.end <= region.start) return false;

        std::size_t size = region.end - region.start;
        if (size == 0 || size > (64ULL << 20)) return false;

        const std::uint8_t* base = reinterpret_cast<const std::uint8_t*>(region.start);
        std::uint8_t needle[8];
        std::memcpy(needle, &value, 8);

        for (std::size_t i = 0; i + 8 <= size; ++i) {
            if (std::memcmp(base + i, needle, 8) == 0) {
                out_addresses.push_back(region.start + i);
                if (max_results > 0 && static_cast<int>(out_addresses.size()) >= max_results) {
                    return true;
                }
            }
        }
        return !out_addresses.empty();
    }

    bool ParseIdaPattern(const std::string& pattern, std::vector<std::uint8_t>& out_bytes) {
        out_bytes.clear();
        std::istringstream iss(pattern);
        std::string byte_str;

        while (iss >> byte_str) {
            if (byte_str.size() != 2) return false;
            try {
                unsigned int byte_val = std::stoul(byte_str, nullptr, 16);
                out_bytes.push_back(static_cast<std::uint8_t>(byte_val));
            } catch (...) {
                return false;
            }
        }
        return !out_bytes.empty();
    }

    bool ScanRegionForPattern(const MemoryRegion& region, const std::vector<std::uint8_t>& pattern, std::vector<std::uintptr_t>& out_addresses, int max_results) {
        if (region.end <= region.start || pattern.empty()) return false;

        std::size_t size = region.end - region.start;
        if (size == 0 || size > (64ULL << 20)) return false;

        const std::uint8_t* base = reinterpret_cast<const std::uint8_t*>(region.start);
        std::size_t pattern_size = pattern.size();

        for (std::size_t i = 0; i + pattern_size <= size; ++i) {
            if (std::memcmp(base + i, pattern.data(), pattern_size) == 0) {
                out_addresses.push_back(region.start + i);
                if (max_results > 0 && static_cast<int>(out_addresses.size()) >= max_results) {
                    return true;
                }
            }
        }
        return !out_addresses.empty();
    }
}


bool Patch::Apply() {
    if (applied || !address || modified.empty()) return false;

    if (!WriteBytes(address, modified.data(), modified.size())) return false;

    applied = true;
    return true;
}

bool Patch::Restore() {
    if (!applied || !address || original.empty()) return false;

    if (!WriteBytes(address, original.data(), original.size())) return false;

    applied = false;
    return true;
}


Patch CreatePatch(void* address, const void* replacement, std::size_t size) {
    Patch patch;
    patch.address = address;
    patch.original.resize(size);
    patch.modified.resize(size);

    std::memcpy(patch.original.data(), address, size);
    std::memcpy(patch.modified.data(), replacement, size);
    patch.applied = false;

    return patch;
}

Patch CreatePatchAtRva(std::uintptr_t rva, const void* replacement, std::size_t size) {
    return CreatePatch(RvaToPtr(rva), replacement, size);
}

Patch CreateNopPatch(void* address, std::size_t instruction_count) {
    const std::size_t size = instruction_count * 4;
    const std::uint32_t nop = 0xD503201F;

    Patch patch;
    patch.address = address;
    patch.original.resize(size);
    patch.modified.resize(size);

    std::memcpy(patch.original.data(), address, size);

    for (std::size_t i = 0; i < instruction_count; ++i) {
        std::memcpy(patch.modified.data() + i * 4, &nop, 4);
    }

    patch.applied = false;
    return patch;
}

Patch CreateNopPatchAtRva(std::uintptr_t rva, std::size_t instruction_count) {
    return CreateNopPatch(RvaToPtr(rva), instruction_count);
}


int CreateNopPatchesForPattern(const std::string& ida_pattern,
                                std::vector<Patch>& out_patches,
                                std::size_t instruction_count,
                                int max_results) {
    out_patches.clear();

    if (ida_pattern.empty() || instruction_count == 0) return 0;

    try {
        std::uintptr_t addr = Cipher::CipherScanIdaPattern(ida_pattern);
        if (addr != 0) {
            out_patches.push_back(CreateNopPatch(reinterpret_cast<void*>(addr), instruction_count));
            return 1;
        }

        auto hits = Cipher::CipherScanIdaPatternAll(ida_pattern);
        if (!hits.empty() && hits[0] != 0) {
            out_patches.push_back(CreateNopPatch(reinterpret_cast<void*>(hits[0]), instruction_count));
            return 1;
        }

        return 0;
    } catch (...) {
        return 0;
    }
}

int CreateReplacementPatches(std::uint64_t search_value,
                              std::uint64_t replace_value,
                              std::vector<Patch>& out_patches,
                              int max_results) {
    out_patches.clear();

    try {
        std::vector<std::uintptr_t> hits;

        std::vector<MemoryRegion> regions;
        if (ReadMemoryMaps(regions)) {
            for (const auto& region : regions) {
                if (!region.executable) continue;
                ScanRegionForQword(region, search_value, hits, max_results);
                if (max_results > 0 && static_cast<int>(hits.size()) >= max_results) break;
            }
        }

        if (hits.empty() && !regions.empty()) {
            for (const auto& region : regions) {
                if (region.executable) continue;
                ScanRegionForQword(region, search_value, hits, max_results);
                if (max_results > 0 && static_cast<int>(hits.size()) >= max_results) break;
            }
        }

        if (hits.empty()) {
            char pattern[24];
            std::snprintf(pattern, sizeof(pattern),
                          "%02X %02X %02X %02X %02X %02X %02X %02X",
                          static_cast<unsigned int>((search_value >> 0) & 0xFF),
                          static_cast<unsigned int>((search_value >> 8) & 0xFF),
                          static_cast<unsigned int>((search_value >> 16) & 0xFF),
                          static_cast<unsigned int>((search_value >> 24) & 0xFF),
                          static_cast<unsigned int>((search_value >> 32) & 0xFF),
                          static_cast<unsigned int>((search_value >> 40) & 0xFF),
                          static_cast<unsigned int>((search_value >> 48) & 0xFF),
                          static_cast<unsigned int>((search_value >> 56) & 0xFF));

            auto cipher_hits = Cipher::CipherScanIdaPatternAll(std::string(pattern));
            for (auto addr : cipher_hits) {
                if (addr != 0) {
                    hits.push_back(addr);
                    if (max_results > 0 && static_cast<int>(hits.size()) >= max_results) break;
                }
            }
        }

        if (hits.empty()) {
            std::vector<std::uintptr_t> to_hits;

            if (!regions.empty()) {
                for (const auto& region : regions) {
                    if (!region.executable) continue;
                    ScanRegionForQword(region, replace_value, to_hits, 1);
                    if (!to_hits.empty()) return 1;
                }
            }

            if (!regions.empty()) {
                for (const auto& region : regions) {
                    if (region.executable) continue;
                    ScanRegionForQword(region, replace_value, to_hits, 1);
                    if (!to_hits.empty()) return 1;
                }
            }

            char to_pattern[24];
            std::snprintf(to_pattern, sizeof(to_pattern),
                          "%02X %02X %02X %02X %02X %02X %02X %02X",
                          static_cast<unsigned int>((replace_value >> 0) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 8) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 16) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 24) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 32) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 40) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 48) & 0xFF),
                          static_cast<unsigned int>((replace_value >> 56) & 0xFF));

            auto cipher_to = Cipher::CipherScanIdaPatternAll(std::string(to_pattern));
            if (!cipher_to.empty()) return 1;

            return 0;
        }

        int count = 0;
        for (std::uintptr_t addr : hits) {
            if (addr == 0) continue;
            out_patches.push_back(CreatePatch(reinterpret_cast<void*>(addr),
                                              &replace_value,
                                              sizeof(replace_value)));
            count++;
            if (max_results > 0 && count >= max_results) break;
        }

        return count;
    } catch (...) {
        return 0;
    }
}

}}}
