#include "Transport.h"

using namespace JSONRPC;
using json = nlohmann::json;

void Transport::setUpstreamHandler(const TransportHandler& handler)
{
    _upstream = handler;
}

void Transport::setDownstreamHandler(const TransportHandler& handler)
{
    _downstream = handler;
}

void Transport::sendUpstream(const json& j)
{
    if (_upstream)
        _upstream(j);
    else
        throw NoTransportHandlerException();
}

void Transport::sendDownstream(const json& j)
{
    if (_downstream)
        _downstream(j);
    else
        throw NoTransportHandlerException();
}
