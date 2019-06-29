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

    inline static uv_loop_t * uv_loop_t_allocator() {
      uv_loop_t *loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
      uv_loop_init(loop);
      return loop;
    }

    struct uv_loop_t_deallocator {
        void operator()(uv_loop_t* loop) const {
          std::cout << " ~ uv_loop_t_deallocator ... " << std::endl;
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
          std::cout << " ~ Channel(" <<id()<< ") ... " << std::endl;

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
        const std::shared_ptr<ConnectionHandler> handler_;
        std::unique_ptr<AMQP::TcpConnection> connection_;

    public:
        std::unique_ptr<Channel>    channel;

        Connection(const capy::amqp::Address& address):
                loop_(std::shared_ptr<uv_loop_t>(uv_loop_t_allocator(), uv_loop_t_deallocator())),
                handler_(std::make_shared<ConnectionHandler>(loop_.get())),
                connection_(std::make_unique<AMQP::TcpConnection>(handler_.get(),to_address(address))),
                channel(std::make_unique<Channel>(connection_.get()))
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

        Connection(const Connection& ) = default;
        Connection(Connection&& ) = default;

    };

    class ConnectionPool {

        capy::Cache<std::thread::id, Connection> connections_;
        capy::amqp::Address address_;

    public:
        ConnectionPool(
                const capy::amqp::Address &address):
                address_(address)
                {}

        Channel* get_channel() {

          auto id = std::this_thread::get_id();

          Channel* _channel;

          if (!connections_.has(id)) {
            auto _connection = std::make_shared<Connection>(address_);
            connections_.set(id, _connection);
            _channel = _connection->channel.get();
          }
          else {
            _channel = connections_.get(id)->channel.get();
          }

          return _channel;
        }
    };

    class BrokerImpl {
        friend class Broker;

    protected:

        std::mutex mutex_;
        std::string exchange_name_;
        std::unique_ptr<ConnectionPool> connection_pool_;

    public:
        BrokerImpl(){};

        ~BrokerImpl();

        DeferredListen& listen_messages(const std::string &queue, const std::vector<std::string> &keys);

        DeferredFetch& fetch_message(const json& message, const std::string& routing_key);

        Error publish_message(const json &message, const std::string &routing_key);

    };
}