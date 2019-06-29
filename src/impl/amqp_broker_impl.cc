//
// Created by denn nevera on 2019-06-21.
//

#include "amqp_broker_impl.h"
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

    BrokerImpl::~BrokerImpl() {}

    Error BrokerImpl::publish_message(const capy::json &message, const std::string &routing_key) {
      std::promise<Result<std::string>> queue_declaration;

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));
      envelope.setDeliveryMode(2);

      auto channel = connection_pool_->get_channel();

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


    DeferredFetch& BrokerImpl::fetch_message(
            const capy::json &message,
            const std::string &routing_key) {

      auto deferred = std::make_shared<capy::amqp::DeferredFetch>();

      std::promise<void> ready_barrier;

      auto channel = connection_pool_->get_channel();

      channel->onReady([&ready_barrier](){
          ready_barrier.set_value();
      });

      ready_barrier.get_future().wait();

      std::promise<std::string> declare_barrier;

      channel

              ->declareQueue(AMQP::exclusive | AMQP::autodelete)

              .onSuccess(
                      [&declare_barrier
                      ](const std::string &name, uint32_t messagecount, uint32_t consumercount) {

                          declare_barrier.set_value(name);

                      })

              .onError([deferred, &declare_barrier](const char *message) {
                  declare_barrier.set_value("");
                  deferred->report_error(Error(BrokerError::QUEUE_DECLARATION, message));
              });

      auto name = declare_barrier.get_future().get();

      if (name.empty()) {
        return *deferred;
      }

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));

      envelope.setDeliveryMode(2);
      envelope.setCorrelationID(create_unique_id());
      envelope.setReplyTo(name);

      channel

              ->consume(envelope.replyTo(), AMQP::noack)

              .onReceived([deferred, this](

                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  capy::json received = json::from_msgpack(buffer);

                  capy::amqp::Task::Instance().async([received, deferred]{
                      deferred->report_data(received);
                  });

              })

              .onError([deferred](const char *message) {

                  deferred->report_error(Error(BrokerError::DATA_RESPONSE, message));

              });

      channel->startTransaction();

      channel->publish(exchange_name_, routing_key, envelope, AMQP::autodelete|AMQP::mandatory);

      channel->commitTransaction()
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

      auto channel = connection_pool_->get_channel();

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

                          auto channel = connection_pool_->get_channel();

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

                          std::lock_guard lock(mutex_);
                          throw_abort(exception.what());
                        }
                        catch (...) {
                          std::lock_guard lock(mutex_);
                          throw_abort("Unexpected exception...");
                        }
                    });

                  }

                  catch (json::exception &exception) {
                    std::lock_guard lock(mutex_);
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, exception.what()));
                  }
                  catch (...) {
                    std::lock_guard lock(mutex_);
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, "Unexpected exception..."));
                  }

              })

              .onError([deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_CONSUMING, message));
              });

      return *deferred;

    }
}