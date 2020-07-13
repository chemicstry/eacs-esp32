#ifndef _JSONRPC_WEBSOCKETTRANSPORT_H_
#define _JSONRPC_WEBSOCKETTRANSPORT_H_

#include "Transport.h"
#include "WebSocketsClient.h"
#include <mutex>

namespace JSONRPC
{
    class WebsocketTransport : public Transport, public WebSocketsClient
    {
    public:
        WebsocketTransport();
        void eventHandler(WStype_t type, uint8_t* payload, size_t length);

        // Thread safe version of loop()
        void update()
        {
            std::lock_guard<std::recursive_mutex> l(_mutex);
            loop();
        }
    
    private:
        std::recursive_mutex _mutex;
    };
}

#endif
