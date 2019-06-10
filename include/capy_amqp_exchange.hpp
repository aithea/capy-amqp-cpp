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

#include "capy_common.hpp"
#include "capy_address.hpp"
#include "capy_expected.hpp"

namespace capy::amqp {

    using namespace std;

    typedef std::function<void(const Result<json>& message)> FetchHandler;
    typedef std::function<void(const Result<json>& message, Result<json>& replay)> ListenHandler;

    /***
     * AMQP Exchange errors
     */
    enum class ExchangeError : PUBLIC_ENUM(CommonError) {
        /***
         * Connection error
         */
        CONNECTION = EXTEND_ENUM(CommonError, LAST),
        LOGIN,
        CHANNEL,
        PUBLISH,
        QUEUE_DECLARATION,
        QUEUE_BINDING,
        QUEUE_CONSUMING,
        DATA_RESPONSE,

        LAST
    };

    class ExchangeErrorCategory : public ErrorCategory {
    public:
        virtual std::string message(int ev) const override;
    };

    const std::error_category &exchange_error_category();

    std::error_condition make_error_condition(capy::amqp::ExchangeError e);

    class ExchangeImpl;

    /**
     *
     */
    class Exchange {

    public:

        /**
         *
         * @param url
         * @param exchange_name
         * @return expected Exchange object or Error report
         */
        static Result <Exchange> Bind(const Address& address, const string& exchange_name = "amq.topic");

        /***
         * Publish message with routing key and exit
         * @param message object message
         * @param routing_key routing key is listened by consumers or workers
         * @return error object if some fails occurred
         */
        Error publish(const json& message, const std::string& routing_key);

        Error fetch(const json& message, const string& routing_key, const FetchHandler& on_data);

        /**
         *
         * @param queue_name
         * @param on_data
         */
        void listen(const string& queue, const std::string& routing_key, const ListenHandler& on_data);


    protected:
        Exchange();
        std::shared_ptr<ExchangeImpl> impl_;
        Exchange(const std::shared_ptr<ExchangeImpl>& impl);
    };

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::ExchangeError>
            : public true_type {};
}