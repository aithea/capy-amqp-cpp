//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#include <iostream>
#include <exception>
#include <optional>

#include "capy/common.h"
#include "capy/address.h"

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
    class CommonError: public ErrorValue {

    public:
        enum Enum: Error::Type {
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
    std::error_condition make_error_condition(capy::amqp::CommonError::Type e);

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::CommonError::Enum >
            : public true_type {};
}
