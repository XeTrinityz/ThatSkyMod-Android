#include <game/memory/Address.h>
#include <Cipher/Cipher.h>

namespace tsm { namespace game { namespace memory {

static std::uintptr_t g_baseAddress = 0;

bool InitializeBase(const std::string& module_name) {
    (void)module_name;
    g_baseAddress = Cipher::get_libBase();
    return g_baseAddress != 0;
}

std::uintptr_t GetBase() {
    return g_baseAddress;
}

}}}
