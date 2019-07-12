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

#include "broker_impl/handler.h"
#include "broker_impl/broker.h"

namespace capy::amqp {

    //
    // MARK: - bind
    //
    Result <Broker> Broker::Bind(
            const capy::amqp::Address &address,
            const std::string &exchange_name,
            uint16_t heartbeat_timeout,
            const ErrorHandler& on_error) {

      try {

        auto impl = std::make_shared<BrokerImpl>(address, exchange_name, heartbeat_timeout);

        auto channel = impl->connections_->new_channel();

        channel
                ->declareExchange(exchange_name, AMQP::topic, AMQP::durable)

                .onError([on_error](const char *message){
                    on_error(Error(BrokerError::QUEUE_DECLARATION, message));
                });

        delete channel;

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

    void Broker::run(const Launch launch) {
      impl_->run(launch);
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
          return "ConnectionCache error";
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


