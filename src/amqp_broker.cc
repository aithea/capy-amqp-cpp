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

    //
    // MARK: - bind
    //
    Result <Broker> Broker::Bind(const capy::amqp::Address &address, const std::string &exchange_name) {

      try {

        auto impl = std::make_shared<BrokerImpl>();

        impl->exchange_name_ = exchange_name;
        impl->connection_pool_ = std::make_unique<ConnectionPool>(address);

        auto channel = impl->connection_pool_->get_channel();

        std::promise<std::string> error_message;

        std::cout << "1.0 ... bind " << std::endl;

        channel

                ->declareExchange(exchange_name, AMQP::topic, AMQP::durable)

                .onSuccess([&error_message]{
                    std::cout << "1.1 ... bind onSuccess" << std::endl;
                    error_message.set_value("");
                })

                .onError([&error_message](const char *message){
                    std::cout << "1.2 ... bind onError: " << message << std::endl;
                    error_message.set_value(message);
                })
                .onFinalize([](){
                    std::cout << "1.3 ... bind onFinalize" << std::endl;
                });

        std::cout << "2.0 ... bind " << std::endl;

        auto error = error_message.get_future().get();

        std::cout << "3.0 ... bind " << std::endl;

        if(!error.empty()) {
          return capy::make_unexpected(
                  capy::Error(BrokerError::EXCHANGE_DECLARATION,
                              error_string("ConnectionPool has been closed: %s", error.c_str())));
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
          return "ConnectionPool error";
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


