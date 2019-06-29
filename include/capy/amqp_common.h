//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#include <iostream>
#include <exception>
#include <optional>

#include "capy/amqp_expected.h"
#include "capy/amqp_cache.h"
#include "nlohmann/json.h"

#define PUBLIC_ENUM(OriginalType) std::underlying_type_t<OriginalType>
#define EXTEND_ENUM(OriginalType,LAST) static_cast<std::underlying_type_t<OriginalType>>(OriginalType::LAST)

namespace capy {

    using namespace nonstd;

    /***
     * Main universal intercommunication structure
     */
    typedef nlohmann::json json;

    /***
     * Common Error handler
     */
    struct Error {

        /***
         * Create Error object from error condition or exception message string
         * @param code error condition code
         * @param message exception string
         */
        Error(const std::error_condition code, const std::optional<std::string>& message = std::nullopt);

        //Error(const Error&) = default;
        //Error(Error &&) = default;
        //Error &operator=(const Error&)  = default;

        /***
         * Get the error value
         * @return code
         */
        const int value() const;

        /***
         * Get the error message string
         * @return error message
         */
        const std::string message() const;

        /***
         * Error is negative or error can be skipped
         * @return true if error occurred
         */
        operator bool() const;

        /***
         * Error standard streaming
         * @param os
         * @param error
         * @return
         */
        friend std::ostream& operator<<(std::ostream& os, const Error& error) {
          os << error.value() << "/" << error.message();
          return os;
        }

    private:
        std::error_code code_;
        std::optional<std::string>  exception_message_;
    };

    /***
     * Synchronous expected result type
     */
    template <typename T>
    using Result = expected<T,Error>;

    /***
     * Common singleton interface
     * @tparam T
     */
    template <typename T>
    class Singleton
    {
    public:
        static T& Instance()
        {
          static T instance;
          return instance;
        }

    protected:
        Singleton() {}
        ~Singleton() {}
    public:
        Singleton(Singleton const &) = delete;
        Singleton& operator=(Singleton const &) = delete;
    };

    /***
    *
    * Formated error string
    *
    * @param format
    * @param ...
    * @return
    */
    const std::string error_string(const char* format, ...);

    static inline void _throw_abort(const char* file, int line, const std::string& msg)
    {
      std::cerr << "Capy logic error: assert failed:\t" << msg << "\n"
                << "Capy logic error: source:\t\t" << file << ", line " << line << "\n";
      abort();
    }

#ifndef NDEBUG
#   define throw_abort(Msg) _throw_abort( __FILE__, __LINE__, Msg)
#else
#   define M_Assert(Expr, Msg) ;
#endif


}

namespace capy::amqp {

    struct Rpc {
        std::string routing_key;
        capy::json  message;
        Rpc() = default;
        Rpc(const Rpc&) = default;
        Rpc(const std::string& akey, const capy::json& aMessage): routing_key(akey), message(aMessage){};
    };

    /**
     * Expected fetching response data type
     */
    typedef Result<json> Response;

    /**
     * Expected listening request data type. Contains json-like structure of action key and routing key of queue
     */
    typedef Result<Rpc> Request;

    /**
     * Replay data type
     */
    typedef Result<json> Replay;

    /***
     * Fetcher handling request
     */
    typedef std::function<void(const Response& request)> FetchHandler;

    /***
     * Listener handling action request and replies
     */
    typedef std::function<void(const Request& request, Replay& replay)> ListenHandler;

    /***
     * Common error codes
     */
    enum class CommonError: int {

        /***
         * Skip the error
         */
                OK = 0,

        /***
         * not supported error
         */
                NOT_SUPPORTED = 300,

        /***
         * unknown error
         */
                UNKNOWN,

        /***
         * Resource not found
         */
                NOT_FOUND,

        /***
         * Collection range excaption
         */
                OUT_OF_RANGE,

        /**
         * the last error code
         */
                LAST
    };

    /***
     * Common Error category
     */
    class ErrorCategory: public std::error_category
    {
    public:
        virtual const char* name() const noexcept override ;
        virtual std::string message(int ev) const override ;
        virtual bool equivalent(const std::error_code& code, int condition) const noexcept override ;

    };

    const std::error_category& error_category();
    std::error_condition make_error_condition(capy::amqp::CommonError e);

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::CommonError>
            : public true_type {};
}
