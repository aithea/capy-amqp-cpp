//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <system_error>

#include "amqpcpp.h"
#include "json.hpp"
#include "capy_address.hpp"

namespace capy::amqp {

    using namespace std;

    typedef nlohmann::json json;

    typedef std::function<void(const json &message)> MessageHandler;


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
        static optional<Exchange> Bind(const Address &address, const string &exchange_name="amq.topic");

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
        Exchange(const Address &url, const string &exchange_name);

    };

}
