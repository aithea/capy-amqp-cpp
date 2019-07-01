//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include "handler.h"
#include "capy/amqp_broker.h"

#include "amqp.h"
#include "amqp_tcp_socket.h"
#include "pool.h"

#include "capy/amqp_deferred.h"
#include "capy/dispatchq.h"

#include <assert.h>
#include <atomic>
#include <thread>
#include <future>

namespace capy::amqp {

    inline static uv_loop_t * uv_loop_t_allocator() {
      uv_loop_t *loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
      uv_loop_init(loop);
      return loop;
    }

    struct uv_loop_t_deallocator {
        void operator()(uv_loop_t* loop) const {
          uv_stop(loop);
          uv_loop_close(loop);
          free(loop);
        }
    };

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
        }

    };


    /*
     * inline static void monitor(uv_timer_t *handle){
     * std::cout << "monitor ping ... " << std::endl;
     * }
     */

    struct Connection {
    private:
        std::thread thread_loop_;
        std::shared_ptr<uv_loop_t> loop_;
        std::shared_ptr<ConnectionHandler> handler_;
        std::unique_ptr<AMQP::TcpConnection> connection_;
        std::unique_ptr<Channel>    channel_;

    public:

        Connection(const capy::amqp::Address& address):
                loop_(std::shared_ptr<uv_loop_t>(uv_loop_t_allocator(), uv_loop_t_deallocator())),
                handler_(std::make_shared<ConnectionHandler>(loop_.get())),
                connection_(std::make_unique<AMQP::TcpConnection>(handler_.get(),to_address(address))),
                channel_(std::make_unique<Channel>(connection_.get()))
        {

          thread_loop_ = std::thread([this] {
              uv_run(loop_.get(), UV_RUN_DEFAULT);
          });

          /*
           * uv_timer_t timer_req;
           * uv_timer_init(loop_.get(), &timer_req);
           * uv_timer_start(&timer_req, monitor, 0, 2000);
           */

          thread_loop_.detach();
        }

        AMQP::TcpConnection* get_conection() { return connection_.get(); };
        Channel*             get_default_channel() { return channel_.get(); };

        void set_deferred(const std::shared_ptr<capy::amqp::DeferredListen>& aDeferred) {
          handler_->deferred = aDeferred;
        }

        Connection(const Connection& ) = delete;
        Connection(Connection&& ) = delete;

    };

    class ConnectionPool {

        capy::Cache<std::thread::id, Connection> connections_;
        capy::amqp::Address address_;

    public:
        ConnectionPool(
                const capy::amqp::Address &address):
                address_(address)
        {}

        void flush() {
          connections_.flush();
        }

        void set_deferred(const std::shared_ptr<capy::amqp::DeferredListen>& aDeferred) {
          auto id = std::this_thread::get_id();
          if (!connections_.has(id)) {
            connections_.set(id, std::make_shared<Connection>(address_));
          }
          connections_.get(id)->set_deferred(aDeferred);
        }

        Channel* new_channel() {
          auto id = std::this_thread::get_id();

          if (!connections_.has(id)) {
            auto _connection = std::make_shared<Connection>(address_);
            connections_.set(id, _connection);
            return new Channel(_connection->get_conection());
          }
          else {
            return new Channel(connections_.get(id)->get_conection());
          }
        }

        Channel* get_default_channel() {

          auto id = std::this_thread::get_id();

          Channel* _channel;

          if (!connections_.has(id)) {
            auto _connection = std::make_shared<Connection>(address_);
            connections_.set(id, _connection);
            _channel = _connection->get_default_channel();
          }
          else {
            _channel = connections_.get(id)->get_default_channel();
          }

          return _channel;
        }

        ConnectionPool(const ConnectionPool& ) = delete;
        ConnectionPool(ConnectionPool&& ) = delete;
    };

    class BrokerImpl {
        friend class Broker;

    protected:

        std::string exchange_name_;
        std::unique_ptr<ConnectionPool> connection_pool_;

    public:
        BrokerImpl(){};
        BrokerImpl(const BrokerImpl&) = delete;
        BrokerImpl(BrokerImpl&&) = delete;

        ~BrokerImpl();

        DeferredListen& listen_messages(const std::string &queue, const std::vector<std::string> &keys);

        DeferredFetch& fetch_message(const json& message, const std::string& routing_key);

        Error publish_message(const json &message, const std::string &routing_key);

    };
}