#include <network/CollectiblesApi.h>
#include <network/ApiClient.h>

namespace tsm { namespace network {

nlohmann::json GetCollectibles() {
    static ApiClient client;
    return client.PostJson("/account/get_collectibles");
}

void AcknowledgeCollectible(const std::string& collectibleId, const std::string& ackType) {
    static ApiClient client;
    nlohmann::json body;
    body["ack"] = ackType;
    body["name"] = collectibleId;
    client.PostJson("/account/ack_collectible", {}, body);
}

}}
