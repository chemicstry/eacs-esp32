#include "Server.h"
#include "JSONRPC.h"
#include "esp_log.h"

using namespace JSONRPC;
using namespace std::placeholders;

Server::Server(Transport* transport)
{
    if (transport)
        setTransport(transport);
}

void Server::setTransport(Transport* transport)
{
    _transport = transport;
    _transport->setDownstreamHandler(std::bind(&Server::handleDownstream, this, _1));
}

void Server::handleDownstream(const json& j)
{
    auto jsonit = j.find("jsonrpc");
    if (jsonit == j.end() || *jsonit != "2.0")
        throw InvalidJSONRPCException();

    uint32_t id = j["id"];
    std::string method = j["method"];

    // Find promise
    auto it = _methods.find(method);
    if (it == _methods.end())
        throw MethodNotFoundException(method);

    json resp;

    // Handler may throw a RPCMethodException
    try {
        json result = it->second(j["params"]);

        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        resp["result"] = result;
    } catch (const RPCMethodException& e) {
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        resp["error"]["code"] = e.code;
        resp["error"]["message"] = e.message;
        //resp["error"]["data"] = e.data;
    }

    if (_transport)
        _transport->sendUpstream(resp);
    else
        throw NoTransportException();
}