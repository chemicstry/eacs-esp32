#include "Client.h"
#include "JSONRPC.h"
#include <vector>

using namespace JSONRPC;
using namespace std::placeholders;

Client::Client(Transport* transport): _id(0), _timeout(JSON_RPC_CLIENT_TIMEOUT)
{
    if (transport)
        setTransport(transport);
    
    // Start timeout thread
    _timeoutThreadHandle = std::thread(std::bind(&Client::_timeoutThread, this, _1), _timeoutThreadExitSignal.get_future());
}

Client::~Client()
{
    // Send exit signal
    _timeoutThreadExitSignal.set_value();

    // Wait for exit
    _timeoutThreadHandle.join();
}

void Client::setTransport(Transport* transport)
{
    _transport = transport;
    _transport->setDownstreamHandler(std::bind(&Client::handleDownstream, this, _1));
}

void Client::handleDownstream(const json& j)
{
    auto jsonit = j.find("jsonrpc");
    if (jsonit == j.end() || *jsonit != "2.0")
        throw InvalidJSONRPCException();

    uint32_t id = j["id"];

    PromisePtr promise;
    {
        std::lock_guard<std::mutex> l(_requestsMutex);

        // Find promise
        auto it = _requests.find(id);
        if (it == _requests.end())
            return;

        promise = it->second.promise;
        _requests.erase(it);
    }

    auto errorit = j.find("error");
    if (errorit != j.end()) {
        auto error = *errorit;

        try {
            if (error.find("data") != error.end()) {
                throw RPCMethodException(error["code"], error["message"], error["data"]);
            } else {
                throw RPCMethodException(error["code"], error["message"]);
            }
        } catch (const RPCMethodException& e) {
            promise->set_exception(make_exception_ptr(e));
        }
    } else {
        auto resultit = j.find("result");
        if (resultit != j.end())
            promise->set_value(*resultit);
        else
            throw InvalidJSONRPCException();
    }
}

void Client::_timeoutThread(std::future<void> f)
{
    while (f.wait_for(std::chrono::milliseconds(JSON_RPC_CLIENT_TIMEOUT_CHECK_INTERVAL)) == std::future_status::timeout)
	{
        std::lock_guard<std::mutex> l(_requestsMutex);

        for (auto it = _requests.cbegin(); it != _requests.cend();)
        {
            system_clock::duration diff = system_clock::now() - it->second.start;
    
            if (duration_cast<milliseconds>(diff).count() > _timeout)
            {
                try {
                    throw TimeoutException();
                } catch (...) {
                    it->second.promise->set_exception(std::current_exception());
                }

                it = _requests.erase(it);
            } else
                ++it;
        }
    }
}

