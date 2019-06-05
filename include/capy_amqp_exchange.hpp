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

#include "json.hpp"
#include "capy_common.hpp"
#include "capy_address.hpp"
#include "capy_expected.hpp"

namespace capy::amqp {

    using namespace std;

    typedef nlohmann::json json;

    typedef std::function<void(const json &message)> MessageHandler;


    /***
     * AMQP Exchange errors
     */
    enum class ExchangeError : PUBLIC_ENUM(CommonError) {
        /***
         * Connection error
         */
        CONNECTION_ERROR = EXTEND_ENUM(CommonError, LAST),
        LOGIN_ERROR,

        LAST
    };

    class ExchangeErrorCategory : public ErrorCategory {
    public:
        virtual std::string message(int ev) const override;
    };

    const std::error_category &excahnge_error_category();

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
         * @return
         */
        static Result <Exchange> Bind(const Address &address, const string &exchange_name = "amq.topic");

        /**
         *
         * @param message
         * @param keys
         */
        void fetch(const json &message, const vector<string> &keys);

        /***
         *
         * @param message
         * @param keys
         */
        void publish(const json &message, const string &queue_name);

        /**
         *
         * @param queue_name
         * @param on_data
         */
        void listen(const string &queue_name, const MessageHandler &on_data);


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