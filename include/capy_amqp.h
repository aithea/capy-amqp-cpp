//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <system_error>

//#include "json.hpp"

namespace capy::amqp {

    using namespace std;

    typedef string json;

    typedef std::function<void(const std::error_code &code)> ErrorHandler;

    typedef std::function<void(const json &message)> MessageHandler;

    class Url {
    };

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
        static optional<Exchange> Bind(const Url &url, const string &exchange_name="amq.topic");

        /**
         *
         * @param message
         * @param keys
         */
        void fetch(const json& message, const vector<string>& keys);

        /***
         *
         * @param message
         * @param keys
         */
        void publish(const json& message, const string& queue_name);

        /**
         *
         * @param queue_name
         * @param on_data
         */
        void listen(const string& queue_name, const MessageHandler& on_data);

    protected:
        Exchange(const Url &url, const string &exchange_name);

    };

}
