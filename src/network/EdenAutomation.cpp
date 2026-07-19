#include <network/EdenAutomation.h>
#include <network/ApiClient.h>
#include <network/SocialManager.h>
#include <ui/helpers/Toast.h>
#include <ui/core/Localization.h>
#include <game/interop/LuaHelpers.h>
#include <game/data/EdenData.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdio>
#include <algorithm>

namespace tsm { namespace network {

std::atomic<bool> EdenAutomation::s_busy{false};
std::atomic<bool> EdenAutomation::s_stop{false};
std::mutex EdenAutomation::s_mu;
float EdenAutomation::s_progress = 0.0f;
std::string EdenAutomation::s_message;

void EdenAutomation::Start(const Config& config) {
    if (s_busy.load()) return;

    s_busy.store(true);
    s_stop.store(false);
    UpdateProgress(0.0f, tsm::ui::i18n::Tr("Starting..."));

    std::thread([config]() {
        RunThread(config);
    }).detach();
}

void EdenAutomation::Stop() {
    s_stop.store(true);
}

bool EdenAutomation::IsRunning() {
    return s_busy.load();
}

float EdenAutomation::GetProgress() {
    std::lock_guard<std::mutex> lk(s_mu);
    return s_progress;
}

std::string EdenAutomation::GetStatus() {
    std::lock_guard<std::mutex> lk(s_mu);
    return s_message;
}

void EdenAutomation::UpdateProgress(float pct, const char* msg) {
    std::lock_guard<std::mutex> lk(s_mu);
    s_progress = pct;
    s_message = msg ? msg : "";
}

void EdenAutomation::RunThread(const Config& config) {
    auto finish = []() {
        s_busy.store(false);
        s_stop.store(false);
        UpdateProgress(100.0f, tsm::ui::i18n::Tr("Idle"));
    };

    if (!SocialManager::Get().IsLoggedIn()) {
        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Not logged in"));
        finish();
        return;
    }

    try {
        static ApiClient client;
        UpdateProgress(0.0f, tsm::ui::i18n::Tr("Starting..."));
        UpdateProgress(10.0f, tsm::ui::i18n::Tr("Fetching wing light information"));

        auto resp = client.PostJson("/account/wing_buffs/get");
        std::vector<std::pair<std::string, uint64_t>> pairs;
        std::vector<std::string> names;

        if (resp.is_object() && resp.contains("wing_buffs") && resp["wing_buffs"].is_array()) {
            for (const auto& b : resp["wing_buffs"]) {
                if (!b.is_object()) continue;

                bool collected = b.value("collected", false);
                bool deposited = b.value("deposited", false);

                if (collected && !deposited) {
                    std::string nm = b.value("name", std::string(""));
                    if (!nm.empty()) {
                        auto it = tsm::game::data::kWingLightDepositIds.find(nm);
                        if (it != tsm::game::data::kWingLightDepositIds.end()) {
                            pairs.emplace_back(nm, it->second);
                            names.push_back(nm);
                        }
                    }
                }
            }
        }

        if (s_stop.load()) { finish(); return; }

        UpdateProgress(20.0f, tsm::ui::i18n::Tr("Depositing wing light"));
        const size_t total = pairs.size();
        for (size_t i = 0; i < pairs.size(); ++i) {
            const auto& pr = pairs[i];
            if (s_stop.load()) { finish(); return; }

            nlohmann::json body;
            body["name_deposit_id_pairs"] = nlohmann::json::array();
            nlohmann::json single = nlohmann::json::array();
            single.push_back(pr.first);
            single.push_back(pr.second);
            body["name_deposit_id_pairs"].push_back(single);

            auto dresp = client.PostJson("/account/wing_buffs/deposit", {}, body);
            if (dresp.is_object() && dresp.contains("result") && dresp["result"].is_string()) {
                std::string r = dresp["result"].get<std::string>();
                if (r == "exceed_max_wing_buff_deposits") {
                    UpdateProgress(30.0f, tsm::ui::i18n::Tr("Deposit limit reached. Continuing..."));
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    break;
                }
            }

            float pct = 20.0f + 10.0f * static_cast<float>(i + 1) / std::max<size_t>(1, total);
            pct = std::min(pct, 30.0f);
            char op[128];
            const char* fmt = tsm::ui::i18n::Tr("Depositing wing light (%zu/%zu)");
            std::snprintf(op, sizeof(op), fmt, i + 1, total);
            UpdateProgress(pct, op);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (s_stop.load()) { finish(); return; }

        UpdateProgress(30.0f, tsm::ui::i18n::Tr("Converting to ascended candles"));
        (void)client.PostJson("/account/wing_buffs/convert");
        if (s_stop.load()) { finish(); return; }

        if (config.recollectAfterConvert && !names.empty()) {
            UpdateProgress(40.0f, tsm::ui::i18n::Tr("Recollecting wing light"));
            nlohmann::json body;
            body["names"] = names;
            (void)client.PostJson("/account/wing_buffs/collect", {}, body);
        }
        if (s_stop.load()) { finish(); return; }

        UpdateProgress(50.0f, tsm::ui::i18n::Tr("Entering Eden"));
        tsm::lua::helpers::ChangeLevel("CandleSpaceEnd", true);
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (s_stop.load()) { finish(); return; }

        UpdateProgress(80.0f, tsm::ui::i18n::Tr("Entering Storm"));
        tsm::lua::helpers::ChangeLevel("Storm", true);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (s_stop.load()) { finish(); return; }

        UpdateProgress(90.0f, tsm::ui::i18n::Tr("Returning to Home"));
        tsm::lua::helpers::ChangeLevel("CandleSpace", true);

        UpdateProgress(100.0f, tsm::ui::i18n::Tr("Eden sequence completed!"));
    } catch (...) {
        UpdateProgress(100.0f, tsm::ui::i18n::Tr("Eden sequence failed!"));
        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Auto Eden failed"));
    }

    finish();
}

}}
