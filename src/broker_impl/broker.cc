//
// Created by denn nevera on 2019-06-21.
//

#include "broker.h"
#include "capy/amqp_common.h"

#include <condition_variable>
#include <sstream>

namespace capy::amqp {

    inline static std::string create_unique_id() {
      static int n = 1;
      std::ostringstream os;
      os << n++;
      return os.str();
    }

    BrokerImpl::~BrokerImpl() {
      connection_pool_->flush();
    }


    ///
    /// MARK: - publish
    ///

    Error BrokerImpl::publish_message(const capy::json &message, const std::string &routing_key) {
      std::promise<Result<std::string>> queue_declaration;

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));
      envelope.setDeliveryMode(2);

      auto channel = connection_pool_->new_channel();

      std::promise<std::string> publish_barrier;

      channel->startTransaction();

      channel->publish(exchange_name_, routing_key, envelope, AMQP::autodelete|AMQP::mandatory);

      channel->commitTransaction()
              .onSuccess([&publish_barrier](){
                  publish_barrier.set_value("");
              })
              .onError([&publish_barrier](const char *message) {
                  publish_barrier.set_value(message);
              });

      auto error = publish_barrier.get_future().get();

      if (!error.empty()){
        return Error(amqp::BrokerError::PUBLISH, error);
      }

      return Error(amqp::CommonError::OK);
    }


    ///
    /// MARK: - fetch
    ///

    DeferredFetch& BrokerImpl::fetch_message(
            const capy::json &message,
            const std::string &routing_key) {

      auto deferred = std::make_shared<capy::amqp::DeferredFetch>();

      auto channel = connection_pool_->new_channel();

      std::promise<std::string> declare_barrier;
      auto declare_barrier_value = declare_barrier.get_future();

      channel

              ->declareQueue(AMQP::exclusive | AMQP::autodelete)

              .onSuccess(
                      [
                              &declare_barrier
                      ]
                              (const std::string &name, uint32_t messagecount, uint32_t consumercount) {
                          (void) consumercount;
                          (void) messagecount;


                          declare_barrier.set_value(name);

                      })

              .onError([deferred, &declare_barrier](const char *message) {
                  declare_barrier.set_value("");
                  deferred->report_error(Error(BrokerError::QUEUE_DECLARATION, message));
              });

      if (declare_barrier_value.wait_for(std::chrono::seconds(1)) == std::future_status::timeout ){
        capy::dispatchq::main::async([&deferred]{
            deferred->report_error(Error(BrokerError::QUEUE_DECLARATION, "time out"));
        });
        return *deferred;
      }

      auto name = declare_barrier_value.get();

      if (name.empty()) {
        return *deferred;
      }

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));

      envelope.setDeliveryMode(2);
      envelope.setCorrelationID(create_unique_id());
      envelope.setReplyTo(name);

      channel

              ->consume(name, AMQP::noack)

              .onReceived([deferred, &channel, name, this](

                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  (void) deliveryTag;
                  (void) redelivered;

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  capy::json received;

                  try {
                    received = json::from_msgpack(buffer);
                  }
                  catch (std::exception& exception) {
                    deferred->report_error(Error(BrokerError::DATA_RESPONSE, exception.what()));
                  }
                  catch (...) {
                    deferred->report_error(Error(BrokerError::DATA_RESPONSE, "unknown error"));
                  }

                  //capy::amqp::Task::Instance().async([received, deferred]{
                  try {
                    deferred->report_data(received);

                  }
                  catch (json::exception &exception) {
                    ///
                    /// Some programmatic exception is not processing properly
                    ///

                    throw_abort(exception.what());
                  }
                  catch (...) {
                    throw_abort("Unexpected exception...");
                  }
                  //});

              })

              .onError([deferred](const char *message) {
                  deferred->report_error(Error(BrokerError::DATA_RESPONSE, message));
              })

              .onFinalize([channel](){
                  //
                  // lock channel_ until message receiving
                  //
                  (void) channel;
              });

      channel->startTransaction();

      channel
              ->publish(exchange_name_, routing_key, envelope, AMQP::autodelete|AMQP::mandatory);

      channel
              ->commitTransaction()
              .onError([deferred](const char *message) {
                  deferred->report_error(Error(BrokerError::PUBLISH, message));
              });

      return *deferred;
    }

    ///
    /// MARK: - listen
    ///
    DeferredListen& BrokerImpl::listen_messages(const std::string &queue,
                                                const std::vector<std::string> &keys) {

      auto deferred = std::make_shared<capy::amqp::DeferredListen>();

      connection_pool_->set_deferred(deferred);

      auto channel = connection_pool_->get_default_channel();

      channel->onError([deferred](const char *message) {
          deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, message));
      });

      std::promise<void> on_ready_barrier;

      channel->onReady([&on_ready_barrier] {
          on_ready_barrier.set_value();
      });

      on_ready_barrier.get_future().wait();

      // create a queue
      channel

              ->declareQueue(queue, AMQP::durable)

              .onError([&deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_DECLARATION, message));
              });

      for (auto &routing_key: keys) {

        channel

                ->bindQueue(exchange_name_, queue, routing_key)

                .onError([&deferred, this, routing_key, queue](const char *message) {

                    deferred->
                            report_error(
                            capy::Error(BrokerError::QUEUE_BINDING,
                                        error_string("%s: %s:%s <- %s", message, exchange_name_.c_str())));
                });
      }

      channel

              ->consume(queue)

              .onReceived([this, deferred, &queue, channel](
                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  (void) redelivered;

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  auto replay_to = message.replyTo();
                  auto cid = message.correlationID();

                  try {

                    capy::json received = json::from_msgpack(buffer);

                    channel->ack(deliveryTag);

                    capy::amqp::Task::Instance().async([ this,
                                                               deferred,
                                                               replay_to,
                                                               received,
                                                               cid,
                                                               deliveryTag
                                                       ] {

                        try {

                          Result<capy::json> replay;
                          capy::json error_json;

                          deferred->report_data(Rpc(replay_to, received), replay);

                          if (!replay) {
                            error_json = {"error",
                                          {{"code", replay.error().value()}, {"message", replay.error().message()}}};
                          } else if (replay->empty()) {
                            error_json = {"error",
                                          {{"code", BrokerError::EMPTY_REPLAY}, {"message", "worker replay is empty"}}};
                          }

                          auto data = json::to_msgpack(error_json.empty() ? replay.value() : error_json);

                          AMQP::Envelope envelope(static_cast<char *>((void *) data.data()),
                                                  static_cast<uint64_t>(data.size()));

                          envelope.setCorrelationID(cid);

                          auto channel = connection_pool_->get_default_channel();

                          channel->startTransaction();

                          channel->publish("", replay_to, envelope);

                          channel->commitTransaction()
                                  .onError([deferred](const char *message) {
                                      deferred->report_error(capy::Error(BrokerError::PUBLISH, message));
                                  });
                        }

                        catch (json::exception &exception) {
                          ///
                          /// Some programmatic exception is not processing properly
                          ///

                          throw_abort(exception.what());
                        }
                        catch (...) {
                          throw_abort("Unexpected exception...");
                        }
                    });

                  }

                  catch (json::exception &exception) {
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, exception.what()));
                  }
                  catch (...) {
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, "unknown error"));
                  }

              })

              .onError([deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_CONSUMING, message));
              });

      return *deferred;
    }
}