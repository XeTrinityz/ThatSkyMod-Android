#include <network/WingBuffManager.h>
#include <network/ApiClient.h>
#include <ui/helpers/Toast.h>
#include <game/memory/Address.h>
#include <game/memory/mem.h>
#include <game/memory/offsets.h>
#include <nlohmann/json.hpp>

namespace tsm { namespace network {

WingBuffResult WingBuffManager::HandleWingBuffs(WingBuffOperation operation, const std::vector<std::string>& names) {
    WingBuffResult result;

    if (names.empty()) {
        result.errorMessage = "No wing lights selected";
        return result;
    }

    std::string endpoint = (operation == WingBuffOperation::Collect)
        ? "/account/wing_buffs/collect"
        : "/account/wing_buffs/drop";

    static ApiClient client;
    HttpResponse raw{};
    nlohmann::json body;
    body["names"] = names;

    (void)client.PostJson(endpoint, {}, body, &raw);

    if (raw.status == 200 || raw.status == 418) {
        result.success = true;
        result.needsRelog = true;
    } else {
        result.success = false;
        result.errorMessage = (operation == WingBuffOperation::Collect)
            ? "Wing Light collect failed"
            : "Wing Light drop failed";
        tsm::ui::helpers::ShowToastError(result.errorMessage.c_str());
    }

    return result;
}

WingBuffResult WingBuffManager::CollectLevelWings(const std::vector<std::string>& names) {
    return HandleWingBuffs(WingBuffOperation::Collect, names);
}

WingBuffResult WingBuffManager::CollectSpiritWings(const std::vector<std::string>& names) {
    return HandleWingBuffs(WingBuffOperation::Collect, names);
}

WingBuffResult WingBuffManager::DropWings(const std::vector<std::string>& names) {
    return HandleWingBuffs(WingBuffOperation::Drop, names);
}

void WingBuffManager::TriggerRelog() {
    tsm::game::memory::EnsureInitialized();
    void* audience = tsm::game::mem::read_ptr_rva(tsm::game::Offsets::AudienceBarn);
    if (!audience) return;

    void* accountBarn = nullptr;
    {
        std::uintptr_t ptr_addr = tsm::game::mem::add(audience, tsm::game::Offsets::kAccountBarn);
        accountBarn = *reinterpret_cast<void**>(ptr_addr);
    }

    if (!accountBarn) {
        std::uintptr_t ptr_addr = tsm::game::mem::add(audience, 0x8);
        accountBarn = *reinterpret_cast<void**>(ptr_addr);
    }

    if (!accountBarn) return;

    std::uintptr_t loading_abs = tsm::game::mem::add(accountBarn, tsm::game::Offsets::kLoadingType);
    tsm::game::mem::write_abs<std::uint8_t>(loading_abs, static_cast<std::uint8_t>(0));
}

}}
