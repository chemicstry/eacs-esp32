#ifndef _JSONRPC_NODE_H_
#define _JSONRPC_NODE_H_

#include "Server.h"
#include "Client.h"

namespace JSONRPC
{
    // Implements a bidirectional JSONRPC interface (acts as both server and client)
    class Node : public Server, public Client
    {
    public:
        Node(Transport& transport);

        void handleDownstream(const json& j);

    private:
        Transport _clientTransport, _serverTransport;
        Transport& _transport;
    };
}

#endif
