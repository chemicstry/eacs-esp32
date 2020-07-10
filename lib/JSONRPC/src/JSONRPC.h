#ifndef _JSONRPCJSONRPC_H_
#define _JSONRPCJSONRPC_H_

#include <exception>
#include <json.hpp>

using json = nlohmann::json;

namespace JSONRPC
{
    class InvalidJSONRPCException : public std::exception
    {
        const char* what() const throw()
        {
            return "Invalid JSON RPC format";
        }
    };

    class NoTransportException : public std::exception
    {
        const char* what() const throw()
        {
            return "Attempt to send without a valid transport set";
        }
    };

    class RPCMethodException : public std::exception
    {
    public:
        RPCMethodException(int _code, const std::string& _message, const json& _data):
            code(_code), message(_message), data(_data)
        {
        }

        RPCMethodException(int _code, const std::string& _message):
            code(_code), message(_message)
        {
        }

        const char* what() const throw()
        {
            return message.c_str();
        }
    
        int code;
        std::string message;
        json data;
    };

    class TimeoutException : public std::exception
    {
        const char* what() const throw()
        {
            return "Request timed out";
        }
    };
}

#endif
