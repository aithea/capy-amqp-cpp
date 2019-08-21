//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include "handler.h"
#include "capy/amqp_broker.h"
#include "pool.h"

#include "capy/amqp_deferred.h"
#include "capy/dispatchq.h"

#include <assert.h>
#include <atomic>
#include <thread>
#include <future>

namespace capy::amqp {

    class BrokerImpl;

    struct ReplayImpl: public Replay{

        using Handler  = std::function<void(Replay* replay)>;

        friend class BrokerImpl;

        using Replay::Replay;

        ReplayImpl();
        virtual  ~ReplayImpl();

        virtual void commit() override;

        void set_commit(const Handler& commit_handler);
        void on_complete(const Handler& complete_handler) override ;

    private:
        std::optional<Handler> commit_handler_;
        std::optional<Handler> complete_handler_;
    };

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

    inline static AMQP::Login to_login(const capy::Login& login) {
      return AMQP::Login(login.get_username(), login.get_password());
    }

    inline static  AMQP::Address to_address(const capy::amqp::Address& address) {
      return AMQP::Address(address.get_hostname(), address.get_port(), to_login(address.get_login()), address.get_vhost());
    }

    class Channel: public AMQP::TcpChannel{
        typedef AMQP::TcpChannel __TcpChannel;
    public:
        using __TcpChannel::__TcpChannel;
        virtual ~Channel() override {}
    };


    struct Connection {
    private:
        std::shared_ptr<uv_loop_t> loop_;
        std::shared_ptr<ConnectionHandler> handler_;
        std::unique_ptr<AMQP::TcpConnection> connection_;

    public:

        Connection(const capy::amqp::Address& address, const std::shared_ptr<uv_loop_t>& loop, uint16_t heartbeat_timeout):
                loop_(loop),
                handler_(std::make_shared<ConnectionHandler>(loop_.get(), heartbeat_timeout)),
                connection_(std::make_unique<AMQP::TcpConnection>(handler_.get(),to_address(address)))//,
        {

        }

        AMQP::TcpConnection* get_conection() { return connection_.get(); };

        void set_deferred(const std::shared_ptr<capy::amqp::DeferredListen>& aDeferred) {
          handler_->deferred = aDeferred;
        }

        void reset_deferred() {
          handler_->deferred = nullptr;
        }

        Connection(const Connection& ) = delete;
        Connection(Connection&& ) = delete;

    };

    class DeferredFetching;
    class DeferredListening;

    class ConnectionCache {

    public:
        ConnectionCache(
                const capy::amqp::Address &address,
                const std::shared_ptr<uv_loop_t>& loop,
                uint16_t heartbeat_timeout):
                loop_(loop),
                address_(address),
                connections_(),
                heartbeat_timeout_(heartbeat_timeout)
        {}

        void flush() {
          connections_.flush();
        }

        void set_deferred(const std::shared_ptr<capy::amqp::DeferredListen>& aDeferred) {
          get_conection()->set_deferred(aDeferred);
        }

        void reset_deferred() {
          get_conection()->reset_deferred();
        }

        Channel* new_channel() {
          auto id = std::this_thread::get_id();

          if (!connections_.has(id)) {
            auto _connection = std::make_shared<Connection>(address_, loop_, heartbeat_timeout_);
            connections_.set(id, _connection);
            return new Channel(_connection->get_conection());
          }
          else {
            return new Channel(connections_.get(id)->get_conection());
          }
        }

        ConnectionCache(const ConnectionCache& ) = delete;
        ConnectionCache(ConnectionCache&& ) = delete;

    private:
        std::shared_ptr<uv_loop_t> loop_;
        capy::amqp::Address address_;
        capy::Cache<std::thread::id, Connection> connections_;
        uint16_t heartbeat_timeout_;

        Connection* get_conection() {
          auto id = std::this_thread::get_id();
          if (!connections_.has(id)) {
            connections_.set(id, std::make_shared<Connection>(address_, loop_, heartbeat_timeout_));
          }
          return connections_.get(id).get();
        }
    };

    class BrokerImpl {
        friend class Broker;

    private:

        std::string exchange_name_;
        std::shared_ptr<uv_loop_t> loop_;
        std::unique_ptr<ConnectionCache> connections_;
        capy::Cache<std::string, DeferredFetching> fetchers_;
        capy::Cache<std::string, DeferredListening> listeners_;
        std::thread thread_loop_;

    public:

        BrokerImpl(const capy::amqp::Address &address, const std::string &exchange_name, uint16_t heartbeat_timeout);
        BrokerImpl(const BrokerImpl&) = delete;
        BrokerImpl(BrokerImpl&&) = delete;

        ~BrokerImpl();

        DeferredListen& listen_messages(const std::string &queue, const std::vector<std::string> &keys);

        DeferredFetch& fetch_message(const json& message, const std::string& routing_key);

        Error publish_message(const json &message, const std::string &routing_key);


        void run(const capy::amqp::Broker::Launch launch);
    };
}