#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <string>
#include "JSONRPC/Node.h"
#include "JSONRPC/WebsocketTransport.h"
#include "esp_log.h"

#if CONFIG_USE_MDNS
#include <ESPmDNS.h>
#endif

class Service
{
public:
    Service(std::string _name);

    ~Service();

    #if CONFIG_USE_MDNS
    void MDNSQuery();
    #endif

    void BeginTransport();
    void BeginTransportSSL();

    void SetAuthToken(const std::string& token);

    std::string name;
    std::string host;
    int port;
    std::string path;
    std::string fingerprint;
    std::string tokenHeader;
    JSONRPC::WebsocketTransport transport;
    JSONRPC::Node* RPC;
};

#endif
