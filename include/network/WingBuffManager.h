#pragma once

#include <string>
#include <vector>
#include <functional>

namespace tsm { namespace network {

struct WingBuffResult {
    bool success = false;
    bool needsRelog = false;
    std::string errorMessage;
};

enum class WingBuffOperation {
    Collect,
    Drop
};

using ProgressCallback = std::function<void(float, const char*)>;

class WingBuffManager {
public:
    static WingBuffResult HandleWingBuffs(WingBuffOperation operation, const std::vector<std::string>& names);

    static WingBuffResult CollectLevelWings(const std::vector<std::string>& names);

    static WingBuffResult CollectSpiritWings(const std::vector<std::string>& names);

    static WingBuffResult DropWings(const std::vector<std::string>& names);

    static void TriggerRelog();
};

}}
