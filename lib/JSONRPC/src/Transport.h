#ifndef _JSONRPCTRANSPORT_H_
#define _JSONRPCTRANSPORT_H_

#include <json.hpp>
#include <functional>
#include <string>

using json = nlohmann::json;

namespace JSONRPC
{
    class NoTransportHandlerException : public std::exception
    {
        const char* what() const throw()
        {
            return "Attempt to send without a valid transport handler";
        }
    };

    typedef std::function<void(const json&)> TransportHandler;

    class Transport
    {
    public:
        // Registers a function which handles outgoing json data
        void setUpstreamHandler(const TransportHandler& handler);
        void setDownstreamHandler(const TransportHandler& handler);

        // Handle incoming json data
        void sendUpstream(const json& j);
        void sendDownstream(const json& j);

    private:
        TransportHandler _upstream, _downstream;
    };
}

#endif
