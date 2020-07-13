#include "Service.h"

static const char* TAG = "service";

Service::Service(std::string _name): name(_name), path("/")
{
    RPC = new JSONRPC::Node(transport);
}

Service::~Service()
{
    delete RPC;
}

#if CONFIG_USE_MDNS
void Service::MDNSQuery()
{
    ESP_LOGI(TAG, "Querying MDNS for service %s", name.c_str());

    int n = MDNS.queryService(name.c_str(), "tcp");
    if (n == 0)
    {
        ESP_LOGE(TAG, "MDNS service not found");
        ESP.restart();
    }

    host = MDNS.IP(0).toString().c_str();
    port = MDNS.port(0);

    ESP_LOGI(TAG, "Acquired IP: %s:%d", host.c_str(), port);
}
#endif

void Service::BeginTransport()
{
    transport.begin(host.c_str(), port, path.c_str());
}

void Service::BeginTransportSSL()
{
    transport.beginSSL(host.c_str(), port, path.c_str(), fingerprint.c_str());
}

void Service::SetAuthToken(const std::string& token)
{
    tokenHeader = "token: " + token;
    transport.setExtraHeaders(tokenHeader.c_str());
}
