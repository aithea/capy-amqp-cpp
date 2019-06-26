//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include "amqp_handler_impl.h"
#include "capy/amqp_broker.h"

#include "amqp.h"
#include "amqp_tcp_socket.h"

#include "capy/dispatchq.h"
#include <assert.h>
#include <atomic>
#include <thread>
#include <future>
#include <capy/amqp_deferred.h>

namespace capy::amqp {

    class BrokerImpl {

        friend class Broker;

    protected:
        std::mutex mutex_;
        std::shared_ptr<uv_loop_t> loop_;
        std::shared_ptr<ConnectionHandler> handler_;
        std::unique_ptr<AMQP::TcpConnection> connection_;
        std::unique_ptr<AMQP::TcpChannel> listen_channel_;
        std::unique_ptr<AMQP::TcpChannel> fetch_channel_;
        std::string exchange_name_;

    public:
        BrokerImpl(const capy::amqp::Address& address, const std::string& exchange);

        BrokerImpl(): listen_channel_(nullptr) {}

        ~BrokerImpl();

        DeferredListen& listen_messages(const std::string &queue, const std::vector<std::string> &keys);

        DeferredFetch& fetch_message(const json& message, const std::string& routing_key);

        Error publish_message(const json &message, const std::string &routing_key);

    };
}