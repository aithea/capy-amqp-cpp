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

    struct Error;

    /***
     * Error handler spec
     */
    using ErrorHandler  = std::function<void(const Error &error)>;

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

    struct PayloadContainer {
    public:
        /**
         * Request message
         */
        capy::json  message;

        PayloadContainer() = default;
        PayloadContainer(const PayloadContainer&) = default;
        PayloadContainer(const capy::json& json):message(json){};
        virtual ~PayloadContainer() = default;
    };

    /**
     * Rpc callback container
     */
    struct Rpc: public PayloadContainer {
        /**
         * Rpc routnig key
         */
        std::string routing_key;

        Rpc() = default;
        Rpc(const Rpc&) = default;
        Rpc(const std::string& key, const capy::json& message):PayloadContainer(message), routing_key(key){};
    };

    /**
     * Expected fetching response data type
     */
    typedef Result<json> Payload;

    /**
     * Expected listening request data type. Contains json-like structure of action key and routing key of queue
     */
    typedef Result<Rpc> Request;

    /**
    * Replay data container
    */
    struct Replay {
    public:

        using Handler  = std::function<void(Replay* replay)>;

        /**
         * Replay message structure
         */
        Payload message;

        /**
         * Empty replay constructor
         */
        Replay();

        /**
         * Default copy constructor
         */
        Replay(const Replay&) = default;

        /**
         * Default move constructor
         */
        Replay(Replay&&) = default;

        /**
         * Commit replay and send message to queue
         */
        virtual void commit() = 0;

        /**
         * Destroy replay object
         */
        virtual ~Replay();

        /**
         * On complete event
         */
        virtual void on_complete(const Handler&) = 0;
    };

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
