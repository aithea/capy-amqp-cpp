//
// Created by denn nevera on 2019-06-21.
//

#include "broker.h"
#include "capy/amqp_common.h"
#include "../deferred_mpl/deferred.h"

#include <condition_variable>
#include <sstream>

namespace capy::amqp {


//    static void monitor(uv_timer_t *handle){
//      auto broker = static_cast<BrokerImpl*>(handle->data);
//      std::cout << "monitor ping ... " << broker << std::endl;
//    }

    inline static std::string create_unique_id() {
      static int n = 1;
      std::ostringstream os;
      os << n++;
      return os.str();
    }

    BrokerImpl::BrokerImpl(const capy::amqp::Address &address,
                           const std::string &exchange_name,
                           uint16_t heartbeat_timeout):
            exchange_name_(exchange_name),
            loop_(std::shared_ptr<uv_loop_t>(uv_loop_t_allocator(), uv_loop_t_deallocator())),
            connections_(std::make_unique<ConnectionCache>(address,loop_, heartbeat_timeout)),
            fetchers_(),
            listeners_()
    {

    }

    BrokerImpl::~BrokerImpl() {
      connections_->flush();
    }

    void BrokerImpl::run() {

      thread_loop_ = std::thread([this] {

//          uv_timer_t timer_req;
//
//          timer_req.data = this;
//
//          uv_timer_init(loop_.get(), &timer_req);
//          uv_timer_start(&timer_req, monitor, 0, 1000);

          uv_run(loop_.get(), UV_RUN_DEFAULT);

      });

      thread_loop_.detach();
    }

    ///
    /// MARK: - publish
    ///

    Error BrokerImpl::publish_message(const capy::json &message, const std::string &routing_key) {
      std::promise<Result<std::string>> queue_declaration;

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));
      envelope.setDeliveryMode(2);

      auto channel = connections_->new_channel();

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

      delete channel;

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

      auto correlation_id = create_unique_id();

      fetchers_.set(correlation_id,
                    std::make_shared<capy::amqp::DeferredFetching>(connections_.get()));

      auto  deferred = fetchers_.get(correlation_id).get();
      auto& channel = deferred->get_channel();

      channel

              .declareQueue(AMQP::exclusive | AMQP::autodelete)

              .onSuccess(
                      [
                              this,
                              message,
                              routing_key,
                              correlation_id
                      ]
                              (const std::string &name, uint32_t messagecount, uint32_t consumercount) {
                          (void) consumercount;
                          (void) messagecount;

                          auto data = json::to_msgpack(message);

                          auto envelope = std::make_shared<AMQP::Envelope>(
                                  static_cast<char*>((void *)data.data()),
                                  static_cast<uint64_t>(data.size()));

                          envelope->setDeliveryMode(2);
                          envelope->setCorrelationID(correlation_id);
                          envelope->setReplyTo(name);

                          auto& channel = fetchers_.get(correlation_id)->get_channel();

                          channel.startTransaction();

                          channel
                                  .publish(exchange_name_, routing_key, *envelope, AMQP::autodelete|AMQP::mandatory);

                          channel
                                  .commitTransaction()
                                  .onError([this,correlation_id](const char *message) {
                                      fetchers_.get(correlation_id)->report_error(Error(BrokerError::PUBLISH, message));
                                  });


                          channel

                                  .consume(name, AMQP::noack)

                                  .onReceived([name, this, correlation_id](

                                          const AMQP::Message &message,
                                          uint64_t deliveryTag,
                                          bool redelivered) {

                                      (void) deliveryTag;
                                      (void) redelivered;

                                      std::vector<std::uint8_t> buffer(
                                              static_cast<std::uint8_t *>((void *) message.body()),
                                              static_cast<std::uint8_t *>((void *) message.body()) + message.bodySize());

                                      capy::json received;

                                      try {
                                        received = json::from_msgpack(buffer);
                                      }
                                      catch (std::exception &exception) {
                                        fetchers_.get(correlation_id)->report_error(Error(BrokerError::DATA_RESPONSE, exception.what()));
                                      }
                                      catch (...) {
                                        fetchers_.get(correlation_id)->report_error(Error(BrokerError::DATA_RESPONSE, "unknown error"));
                                      }

                                      {
                                        ///
                                        /// Report data callback
                                        ///

                                        try {
                                          fetchers_.get(correlation_id)->report_data(received);
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

                                        fetchers_.del(correlation_id);

                                      }

                                  })

                                  .onSuccess([this,correlation_id]{
                                      fetchers_.get(correlation_id)->report_success();
                                  })

                                  .onError([correlation_id, this](const char *message) {
                                      fetchers_.get(correlation_id)->report_error(Error(BrokerError::DATA_RESPONSE, message));
                                      fetchers_.del(correlation_id);
                                  });

                      })

              .onError([this, correlation_id](const char *message) {
                  fetchers_.get(correlation_id)->report_error(Error(BrokerError::QUEUE_DECLARATION, message));
                  fetchers_.del(correlation_id);
              });

      return *deferred;
    }

    ///
    /// MARK: - listen
    ///
    DeferredListen& BrokerImpl::listen_messages(const std::string &queue,
                                                const std::vector<std::string> &keys) {

      auto correlation_id = create_unique_id();

      listeners_.set(correlation_id,
                     std::make_shared<capy::amqp::DeferredListening>(connections_.get()));

      auto deferred = listeners_.get(correlation_id);

      auto& channel = deferred->get_channel();

      connections_->set_deferred(deferred);

      channel.onError([this, correlation_id](const char *message) {
          listeners_.get(correlation_id)->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, message));
      });

      // create a queue
      channel

              .declareQueue(queue, AMQP::durable)

              .onError([this, correlation_id](const char *message) {
                  listeners_.get(correlation_id)->report_error(capy::Error(BrokerError::QUEUE_DECLARATION, message));
              });

      for (auto &routing_key: keys) {

        channel

                .bindQueue(exchange_name_, queue, routing_key)

                .onError([this, correlation_id, routing_key, queue](const char *message) {
                    listeners_.get(correlation_id)->
                            report_error(
                            capy::Error(BrokerError::QUEUE_BINDING,
                                        error_string("%s: %s:%s <- %s", message, exchange_name_.c_str())));
                });
      }

      channel

              .consume(queue)

              .onReceived([this, correlation_id, queue](
                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  (void) redelivered;

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  auto replay_to = message.replyTo();
                  auto routing_key = message.routingkey();
                  auto cid = message.correlationID();

                  capy::json received;

                  listeners_.get(correlation_id)->get_channel().ack(deliveryTag);

                  try {
                    received = json::from_msgpack(buffer);
                  }
                  catch (json::exception &exception) {
                    listeners_.get(correlation_id)->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, exception.what()));
                    return;
                  }
                  catch (...) {
                    listeners_.get(correlation_id)->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, "unknown error"));
                    return;
                  }

                  connections_->reset_deferred();

                  capy::amqp::Task::Instance().async([ this,
                                                             correlation_id,
                                                             replay_to,
                                                             routing_key,
                                                             received,
                                                             cid,
                                                             deliveryTag
                                                     ] {

                      try {

                        Result<capy::json> replay;
                        capy::json error_json;

                        listeners_.get(correlation_id)->report_data(Rpc(routing_key, received), replay);

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

                        auto channel = connections_->new_channel();

                        channel->startTransaction();

                        channel->publish("", replay_to, envelope);

                        channel->commitTransaction()
                                .onError([this, correlation_id](const char *message) {
                                    listeners_.get(correlation_id)->report_error(capy::Error(BrokerError::PUBLISH, message));
                                });

                        delete channel;

                      }

                      catch (json::exception &exception) {
                        ///
                        /// Some programmatic exception is not processing properly
                        ///

                        connections_->reset_deferred();
                        listeners_.del(correlation_id);
                        throw_abort(exception.what());
                      }
                      catch (...) {
                        connections_->reset_deferred();
                        listeners_.del(correlation_id);
                        throw_abort("Unexpected exception...");
                      }
                  });

              })

              .onSuccess([this, correlation_id]{
                  listeners_.get(correlation_id)->report_success();
              })

              .onError([this, correlation_id](const char *message) {
                  connections_->reset_deferred();
                  listeners_.get(correlation_id)->report_error(capy::Error(BrokerError::QUEUE_CONSUMING, message));
                  listeners_.del(correlation_id);
              });

      return *deferred;
    }
}