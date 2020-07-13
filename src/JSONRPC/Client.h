#ifndef _JSONRPCCLIENT_H_
#define _JSONRPCCLIENT_H_

#include "JSONRPC.h"
#include "Transport.h"
#include <json.hpp>
#include <string>
#include <future>
#include <map>
#include <mutex>
#include <atomic>
#include "esp_log.h"

using json = nlohmann::json;
using namespace std::chrono;

#define JSON_RPC_CLIENT_TIMEOUT 2000
#define JSON_RPC_CLIENT_TIMEOUT_CHECK_INTERVAL 500

namespace JSONRPC
{
    typedef std::shared_ptr<std::promise<json>> PromisePtr;
    typedef std::future<json> Future;

    struct Request
    {
        Request(PromisePtr _promise, system_clock::time_point _start):
            promise(_promise), start(_start) {}
        PromisePtr promise;
        system_clock::time_point start;
    };

    class Client
    {
    public:
        Client(Transport* transport = nullptr);
        ~Client();

        void setTransport(Transport* transport);

        template<typename ...Args>
        void notify(std::string method, Args&&... args);
        template<typename ...Args>
        json call(std::string method, Args&&... args);
        template<typename ...Args>
        Future callAsync(std::string method, Args&&... args);

        // Handle incoming json data
        void handleDownstream(const json& j);

    private:
        std::atomic<uint32_t> _id;
        std::map<uint32_t, Request> _requests;
        std::mutex _requestsMutex;

        Transport* _transport;

        // Timeout thread checks for timed out requests
        uint32_t _timeout;
        std::thread _timeoutThreadHandle;
        std::promise<void> _timeoutThreadExitSignal;
        void _timeoutThread(std::future<void> f);
    };

    template<typename Arg>
    inline void unpack(json& params, Arg&& arg)
    {
        params.push_back(arg);
    }

    template<typename Arg, typename ...Args>
    inline void unpack(json& params, Arg&& arg, Args&&... args)
    {
        params.push_back(arg);
        unpack(params, std::forward<Args>(args)...);
    }

    template<typename ...Args>
    inline void Client::notify(std::string method, Args&&... args)
    {
        json j;
        j["jsonrpc"] = "2.0";
        j["method"] = method;

        // Unpack arguments
        unpack(j["params"], std::forward<Args>(args)...);

        // Send formatted JSON RPC message
        if (_transport)
            _transport->sendUpstream(j);
        else
            throw NoTransportException();
    }

    template<typename ...Args>
    inline json Client::call(std::string method, Args&&... args)
    {
        return callAsync(method, std::forward<Args>(args)...).get();
    }

    template<typename ...Args>
    inline Future Client::callAsync(std::string method, Args&&... args)
    {
        uint32_t id = _id++;
        json j;

        j["jsonrpc"] = "2.0";
        j["method"] = method;
        j["id"] = id;

        // Unpack arguments
        unpack(j["params"], std::forward<Args>(args)...);

        // Save promise for matching response message to request
        auto promise = std::make_shared<std::promise<json>>();

        {
            std::lock_guard<std::mutex> l(_requestsMutex);
            _requests.insert(std::make_pair(id, Request(promise, system_clock::now())));
        }

        // Send formatted JSON RPC message
        if (_transport)
            _transport->sendUpstream(j);
        else
            throw NoTransportException();

        return promise->get_future();
    }
}

#endif
