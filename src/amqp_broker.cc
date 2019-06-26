//
// Created by denn nevera on 2019-05-31.
//

#include "capy/amqp_broker.h"
#include "amqp.h"
#include "amqp_tcp_socket.h"

#include "capy/dispatchq.h"
#include <assert.h>
#include <atomic>
#include <thread>
#include <future>

#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>

#include <future>

#include "impl/amqp_handler_impl.h"
#include "impl/amqp_broker_impl.h"

namespace capy::amqp {

    inline static AMQP::Login to_login(const capy::amqp::Login& login) {
      return AMQP::Login(login.get_username(), login.get_password());
    }

    inline static  AMQP::Address to_address(const capy::amqp::Address& address) {
      return AMQP::Address(address.get_hostname(), address.get_port(), to_login(address.get_login()), address.get_vhost());
    }

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

    //
    // MARK: - bind
    //
    Result <Broker> Broker::Bind(const capy::amqp::Address &address, const std::string &exchange_name) {

      try {
        auto loop = std::shared_ptr<uv_loop_t>(uv_loop_t_allocator(), uv_loop_t_deallocator());
        if (loop == nullptr) {
          return capy::make_unexpected(
                  capy::Error(BrokerError::MEMORY,
                              error_string("Connection event loop could not be created because memory error..")));
        }

        auto handler = std::shared_ptr<ConnectionHandler>(new ConnectionHandler(loop.get()));

        if (handler == nullptr) {
          return capy::make_unexpected(
                  capy::Error(BrokerError::MEMORY,
                              error_string("Connection handler could not be created because memory error..")));
        }

        auto connection = std::unique_ptr<AMQP::TcpConnection>(new AMQP::TcpConnection(handler.get(), to_address(address)));

        if (connection == nullptr) {
          return capy::make_unexpected(
                  capy::Error(BrokerError::MEMORY,
                              error_string("Connection could not be created because memory error..")));
        }

        auto impl = std::shared_ptr<BrokerImpl>(new BrokerImpl());

        impl->exchange_name_ = exchange_name;
        impl->loop_ = loop;
        impl->handler_ = std::move(handler);
        impl->connection_ = std::move(connection);

        std::thread thread_loop([&impl] {
            uv_run(impl->loop_.get(), UV_RUN_DEFAULT);
        });

        thread_loop.detach();

        AMQP::TcpChannel channel(impl->connection_.get());

        std::promise<std::string> error_message;

        channel

                .declareExchange(exchange_name, AMQP::topic, AMQP::durable)

                .onSuccess([&error_message]{
                    error_message.set_value("");
                })

                .onError([&error_message](const char *message){
                    error_message.set_value(message);
                });

        auto error = error_message.get_future().get();

        if(!error.empty()) {
          return capy::make_unexpected(
                  capy::Error(BrokerError::EXCHANGE_DECLARATION,
                              error_string("Connection has been closed: %s", error.c_str())));
        }

        return Broker(impl);
      }
      catch (std::exception& e) {
        return capy::make_unexpected(
                capy::Error(BrokerError::CONNECTION,
                            error_string(e.what())));
      }
      catch (...) {
        return capy::make_unexpected(
                capy::Error(BrokerError::CONNECTION,
                            error_string("Unknown error")));
      }

    }

    //
    // Broker constructors
    //
    Broker::Broker(const std::shared_ptr<capy::amqp::BrokerImpl> &impl) : impl_(impl) {}
    Broker::Broker() : impl_(nullptr) {}


    //
    // fetch
    //
    DeferredFetch& Broker::fetch(const capy::json& message, const std::string& routing_key) {
      return impl_->fetch_message(message, routing_key);
    }

    //
    // listen
    //
    DeferredListen& Broker::listen(
            const std::string& queue,
            const std::vector<std::string>& routing_keys) {
      return impl_->listen_messages(queue,routing_keys);
    }

    //
    // publish
    //
    Error Broker::publish(const capy::json& message, const std::string& routing_key) {
      return impl_->publish_message(message, routing_key);
    }

    ///
    /// Errors...
    ///

    std::string BrokerErrorCategory::message(int ev) const {
      switch (ev) {
        case static_cast<int>(BrokerError::CONNECTION):
          return "Connection error";
        default:
          return ErrorCategory::message(ev);
      }
    }

    const std::error_category &broker_error_category() {
      static BrokerErrorCategory instance;
      return instance;
    }

    std::error_condition make_error_condition(capy::amqp::BrokerError e) {
      return std::error_condition(
              static_cast<int>(e),
              capy::amqp::broker_error_category());
    }
}


