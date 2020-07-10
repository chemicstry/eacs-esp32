#include "Node.h"
#include "JSONRPC.h"

using namespace JSONRPC;
using namespace std::placeholders;

Node::Node(Transport& transport): Server(), Client(), _transport(transport)
{
    // Handle downstream locally (split client and server messages)
    _transport.setDownstreamHandler(std::bind(&Node::handleDownstream, this, _1));

    // Set server and client transports
    Client::setTransport(&_clientTransport);
    _clientTransport.setUpstreamHandler(std::bind(&Transport::sendUpstream, &_transport, _1));
    Server::setTransport(&_serverTransport);
    _serverTransport.setUpstreamHandler(std::bind(&Transport::sendUpstream, &_transport, _1));
}

void Node::handleDownstream(const json& j)
{
    auto jsonit = j.find("jsonrpc");
    if (jsonit == j.end() || *jsonit != "2.0")
        throw InvalidJSONRPCException();

    // Check if message is a request
    if (j.find("method") != j.end())
        _serverTransport.sendDownstream(j);
    else
        _clientTransport.sendDownstream(j);
}
