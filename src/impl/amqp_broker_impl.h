//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include "amqp_handler_impl.h"
#include "capy/amqp_broker.h"

#include "amqp.h"
#include "amqp_tcp_socket.h"
#include "amqp_pool_impl.h"

#include "capy/amqp_deferred.h"
#include "capy/dispatchq.h"

#include <assert.h>
#include <atomic>
#include <thread>
#include <future>

namespace capy::amqp {


    inline static AMQP::Login to_login(const capy::amqp::Login& login) {
      return AMQP::Login(login.get_username(), login.get_password());
    }

    inline static  AMQP::Address to_address(const capy::amqp::Address& address) {
      return AMQP::Address(address.get_hostname(), address.get_port(), to_login(address.get_login()), address.get_vhost());
    }

    class Channel: public AMQP::TcpChannel{
        typedef AMQP::TcpChannel __TcpChannel;
    public:
        using __TcpChannel::__TcpChannel;
        virtual ~Channel() override {
          std::cout << "~Channel("<< id() <<")" << std::endl;
        }
    };

    class Connection {

        std::shared_ptr<AMQP::TcpConnection> connection_;
        std::shared_ptr<AMQP::TcpChannel>    channel_;

    public:
        Connection(const std::shared_ptr<ConnectionHandler>& handler, const capy::amqp::Address &address):
        connection_(new AMQP::TcpConnection(handler.get(), to_address(address))),
        channel_(std::make_shared<Channel>(connection_.get())){}

        AMQP::TcpConnection* get_connection() { return connection_.get(); }
        AMQP::TcpChannel* get_channel() const { return channel_.get(); }
    };

    class BrokerImpl {
        using ConnectionPool = capy::Pool<Connection>;

        friend class Broker;

    protected:

        std::mutex mutex_;
        std::shared_ptr<uv_loop_t> loop_;
        std::shared_ptr<ConnectionHandler> handler_;
        std::shared_ptr<ConnectionPool> connection_pool_;
        //AMQP::TcpChannel* fetch_channel_ = nullptr;

        //std::unique_ptr<AMQP::TcpConnection> connection_;
        //std::unique_ptr<AMQP::TcpChannel> listen_channel_;
        std::unique_ptr<AMQP::TcpChannel> fetch_channel_ = nullptr;
        std::string exchange_name_;

    public:
        //BrokerImpl(const capy::amqp::Address& address, const std::string& exchange);
        BrokerImpl(){};
        ~BrokerImpl();

        DeferredListen& listen_messages(const std::string &queue, const std::vector<std::string> &keys);

        DeferredFetch& fetch_message(const json& message, const std::string& routing_key);

        Error publish_message(const json &message, const std::string &routing_key);

    };
}