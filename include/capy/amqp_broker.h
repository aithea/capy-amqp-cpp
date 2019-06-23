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
#include "capy/amqp_expected.h"

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
     * AMQP Broker errors
     */
    enum class BrokerError : PUBLIC_ENUM(CommonError) {

        /***
         * Connection error
         */
        CONNECTION = EXTEND_ENUM(CommonError, LAST),
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

        /***
         *
         * Bind broker with amqp cloud and create Broker client object
         *
         * @param address AMQP address
         * @param exchange_name exchange name
         * @return expected Broker object or Error report
         */
        static Result <Broker> Bind(const Address& address, const std::string& exchange_name = "amq.topic");

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
        Error fetch(const json& message, const std::string& routing_key, const FetchHandler& on_data);

        /**
         * Listen queue bound list of certain topic keys
         * @param queue queue name
         * @param keys topic keys
         * @param on_data messaging handling
         */
        void listen(const std::string& queue, const std::vector<std::string>& keys, const ListenHandler& on_data);


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

    /***
     * Broker rpc errors logic
     * @param error broker error state
     * @return error condition
     */
    std::error_condition make_error_condition(capy::amqp::BrokerError error);

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::BrokerError>
            : public true_type {};
}
