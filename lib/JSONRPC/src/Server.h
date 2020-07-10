#ifndef _JSONRPCSERVER_H_
#define _JSONRPCSERVER_H_

#include "Transport.h"
#include <map>
#include <string>
#include <json.hpp>

using json = nlohmann::json;

namespace JSONRPC
{
    class MethodNotFoundException : public std::exception
    {
    public:
        MethodNotFoundException(std::string& m): method(m) {}

        const char* what() const throw()
        {
            return "Method not found";
        }

        std::string method;
    };

    typedef std::function<json(const json&)> ServerMethod;

    class Server
    {
    public:
        Server(Transport* transport = nullptr);
        void setTransport(Transport* transport);

        template<typename T>
        void bind(std::string name, T func);

        // Handle incoming json data
        void handleDownstream(const json& j);
    
    private:
        std::map<std::string, ServerMethod> _methods;
        Transport* _transport;
    };

    template <typename S>
    struct signature : signature<decltype(&S::operator())> {};

    template <typename C, typename R, typename... Args>
    struct signature<R (C::*)(Args...)> : signature<R (*)(Args...)> {};

    template <typename C, typename R, typename... Args>
    struct signature<R (C::*)(Args...) const> : signature<R (*)(Args...)> {};

    template<typename R, typename... Args>
    struct signature<R (*)(Args...)>
    {
        using return_type = R;
        using argument_type = std::tuple<typename std::decay<Args>::type...>;
    };

    // Unpacks json array into tuple
    template<size_t N>
    struct json_converter
    {
        template<typename Tuple>
        static void to_tuple(Tuple& t, const json& j)
        {
            std::get<N-1>(t) = j[N-1];
            json_converter<N-1>::to_tuple(t, j);
        }
    };

    template<>
    struct json_converter<0>
    {
        template<typename Tuple>
        static void to_tuple(Tuple& t, const json& j)
        {
        }
    };

    template <size_t N>
    struct call_helper
    {
        template <typename Functor, typename... ArgsT, typename... ArgsF>
        static auto call(Functor f, std::tuple<ArgsT...>& args_t, ArgsF&&... args_f)
        -> decltype(call_helper<N-1>::call(f, args_t, std::get<N-1>(args_t), std::forward<ArgsF>(args_f)...))
        {
            return call_helper<N-1>::call(f, args_t, std::get<N-1>(args_t), std::forward<ArgsF>(args_f)...);
        }
    };

    template <>
    struct call_helper<0>
    {
        template <typename Functor, typename... ArgsT, typename... ArgsF>
        static auto call(Functor f, std::tuple<ArgsT...>&, ArgsF&&... args_f)
        -> decltype(f(std::forward<ArgsF>(args_f)...))
        {
            return f(std::forward<ArgsF>(args_f)...);
        }
    };

    // Calls a functor with tuple arguments
    template <typename Functor, typename... ArgsT>
    auto call(Functor f, std::tuple<ArgsT...>& args_t)
    -> decltype(call_helper<sizeof...(ArgsT)>::call(f, args_t))
    {
        return call_helper<sizeof...(ArgsT)>::call(f, args_t);
    }

    template<typename T>
    inline void Server::bind(std::string name, T func)
    {
        // Get tuple type of function arguments
        using args_t = typename signature<T>::argument_type;

        _methods.insert(std::make_pair(name, [func](const json& j) {
            args_t args;
            json_converter<std::tuple_size<args_t>::value>::to_tuple(args, j);

            return call(func, args);
        }));
    }
}

#endif
