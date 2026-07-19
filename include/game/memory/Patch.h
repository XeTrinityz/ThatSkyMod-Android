#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

namespace tsm { namespace game { namespace memory {


struct Patch {
    void* address{nullptr};
    std::vector<std::uint8_t> original;
    std::vector<std::uint8_t> modified;
    bool applied{false};

    bool Apply();

    bool Restore();
};


Patch CreatePatch(void* address, const void* replacement, std::size_t size);

Patch CreatePatchAtRva(std::uintptr_t rva, const void* replacement, std::size_t size);

Patch CreateNopPatch(void* address, std::size_t instruction_count);

Patch CreateNopPatchAtRva(std::uintptr_t rva, std::size_t instruction_count);


int CreateNopPatchesForPattern(const std::string& ida_pattern,
                                std::vector<Patch>& out_patches,
                                std::size_t instruction_count = 1,
                                int max_results = 0);

int CreateReplacementPatches(std::uint64_t search_value,
                              std::uint64_t replace_value,
                              std::vector<Patch>& out_patches,
                              int max_results = 0);

}}}
