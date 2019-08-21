//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <system_error>
#include <memory>
#include <thread>
#include <algorithm>
#include <map>

#include "capy/dispatchq.h"
#include "capy/amqp_common.h"
#include "capy/amqp_address.h"
#include "capy/amqp_deferred.h"

namespace capy::amqp {

    /***
     * AMQP Broker errors
     */
    class BrokerError : public CommonError {

    public:
        enum Enum: Error::Type {
            /***
             * Connection error
             */
                    CONNECTION = CommonError::LAST, //EXTEND_ENUM(CommonError, LAST),

            CONNECTION_CLOSED,
            CONNECTION_LOST,
            MEMORY,
            LOGIN,
            CHANNEL_READY,
            CHANNEL_MESSAGE,
            PUBLISH,
            EXCHANGE_DECLARATION,
            QUEUE_DECLARATION,
            QUEUE_BINDING,
            QUEUE_CONSUMING,
            LISTENER_CONFLICT,
            EMPTY_REPLAY,
            DATA_RESPONSE,

            LAST
        };
    };

    class BrokerTaskQueue:public capy::dispatchq::Queue{
    public:
        BrokerTaskQueue():Queue(std::max(std::thread::hardware_concurrency(),2u)){};
    };

    class Task:public Singleton<BrokerTaskQueue>
    {
        friend class Singleton<BrokerTaskQueue>;
    private:
        Task(){};
    };

    class BrokerImpl;

    /**
     * Common AMQP Broker client rpc client
     */
    class Broker {

    public:

        /**
         * Launching type
         */
        enum class Launch:int {
            async = 0,
            sync
        };

    public:

        const constexpr static uint16_t heartbeat_timeout = 60;

        /***
         *
         * Bind broker with amqp cloud and create Broker client object
         *
         * @param address AMQP address
         * @param exchange_name exchange name
         * @return expected Broker object or Error report
         */
        static Result <Broker> Bind(
                const Address& address,
                const std::string& exchange_name,
                uint16_t heartbeat_timeout,
                const ErrorHandler& on_error);

        static Result <Broker> Bind(
                const Address& address,
                const std::string& exchange_name){
          return Broker::Bind(address, exchange_name, Broker::heartbeat_timeout, [](const Error& error){(void)error;});
        }

        static Result <Broker> Bind(
                const Address& address,
                const ErrorHandler& on_error){
          return Broker::Bind(address, "amq.topic", Broker::heartbeat_timeout, on_error);
        }

        static Result <Broker> Bind(
                const Address& address,
                uint16_t heartbeat_timeout){
          return Broker::Bind(address, "amq.topic", heartbeat_timeout, [](const Error& error){(void)error;});
        }

        static Result <Broker> Bind(
                const Address& address){
          return Broker::Bind(address, "amq.topic", Broker::heartbeat_timeout, [](const Error& error){(void)error;});
        }

        /***
         * Publish message with routing key and exit
         * @param message object message
         * @param routing_key routing key is listened by consumers or workers
         * @return error object if some fails occurred
         */
        Error publish(const json& message, const std::string& routing_key);

        /***
         *
         * Request message with action and fetch result
         *
         * @param message request actions with payload
         * @param routing_key routing key
         * @param on_data messaging handling
         * @return error or ok
         */
        DeferredFetch& fetch(const json& message, const std::string& routing_key);

        /**
         * Listen queue bound list of certain topic keys
         * @param queue queue name
         * @param keys topic keys
         * @param on_data messaging handling
         */
        DeferredListen& listen(const std::string& queue, const std::vector<std::string>& keys);


        void run(const Launch launch = Launch::async);

    protected:
        Broker();
        Broker(const std::shared_ptr<BrokerImpl>& impl);

    private:
        std::shared_ptr<BrokerImpl> impl_;
    };

    /***
     * Broker errors handling
     */
    class BrokerErrorCategory : public ErrorCategory {
    public:
        virtual std::string message(int ev) const override;
    };

    /***
     * Predefined exchange error category
     * @return error category
     */
    const std::error_category &broker_error_category();

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::BrokerError::Enum >
            : public true_type {};
}
