//
// Created by denn nevera on 2019-06-21.
//

#include "amqp_broker_impl.h"
#include "capy/amqp_common.h"

#include <condition_variable>
#include <sstream>

namespace capy::amqp {

    class Channel: public AMQP::TcpChannel{
        typedef AMQP::TcpChannel __TcpChannel;
    public:
        using __TcpChannel::__TcpChannel;
        virtual ~Channel() override {}
    };

    inline static std::string create_unique_id() {
      static int n = 1;
      std::ostringstream os;
      os << n++;
      return os.str();
    }

    inline static AMQP::Login to_login(const capy::amqp::Login& login) {
      return AMQP::Login(login.get_username(), login.get_password());
    }

    inline static  AMQP::Address to_address(const capy::amqp::Address& address) {
      return AMQP::Address(address.get_hostname(), address.get_port(), to_login(address.get_login()), address.get_vhost());
    }

    struct uv_loop_t_deallocator {
        void operator()(uv_loop_t* loop) const {
          uv_stop(loop);
          uv_loop_close(loop);
          free(loop);
        }
    };

    inline static uv_loop_t * uv_loop_t_allocator() {
      uv_loop_t *loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
      uv_loop_init(loop);
      return loop;
    }

    BrokerImpl::~BrokerImpl() {
      isExited = true;
    }

    BrokerImpl::BrokerImpl(
            const capy::amqp::Address &address,
            const std::string &exchange):

            loop_(uv_loop_t_allocator(), uv_loop_t_deallocator()),
            handler_(new ConnectionHandler(loop_.get())),
            connection_(new AMQP::TcpConnection(handler_.get(), to_address(address))),
            exchange_name(exchange)
    {

      std::thread thread_loop([this] {
          uv_run(loop_.get(), UV_RUN_DEFAULT);
      });

      thread_loop.detach();
    }

    std::atomic_uint32_t BrokerImpl::correlation_id = 0;

    Error BrokerImpl::publis_message(const capy::json &message, const std::string &routing_key) {
      std::promise<Result<std::string>> queue_declaration;

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));
      envelope.setDeliveryMode(2);

      mutex.lock();
      auto channel = Channel(connection_.get());
      mutex.unlock();

      std::promise<std::string> publish_barrier;

      channel.startTransaction();

      channel.publish(exchange_name, routing_key, envelope, AMQP::autodelete|AMQP::mandatory);

      channel.commitTransaction()
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

      std::promise<Result<std::string>> queue_declaration;

      if (fetch_channel_== nullptr) {
        fetch_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection_.get()));
      }

      fetch_channel_

              ->declareQueue(AMQP::exclusive|AMQP::autodelete)

              .onSuccess([&queue_declaration](const std::string &name, uint32_t messagecount, uint32_t consumercount){
                  queue_declaration.set_value(name);
              })

              .onError([&queue_declaration](const char *message) {
                  queue_declaration.set_value(capy::make_unexpected(Error(BrokerError::QUEUE_DECLARATION,message)));
              });

      auto queue = queue_declaration.get_future().get();

      if (!queue){
        capy::amqp::Task::Instance().async([queue, deferred]{
            deferred->report_error(queue.error());
        });
        return *deferred;
      }

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));

      envelope.setDeliveryMode(2);
      envelope.setCorrelationID(create_unique_id());
      envelope.setReplyTo(queue.value());

      fetch_channel_

              ->consume(envelope.replyTo(), AMQP::noack)

              .onReceived([deferred](

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


      fetch_channel_
              ->startTransaction()
              .onError([deferred](const char *message) {
                  deferred->report_error(Error(BrokerError::PUBLISH, message));
              });

      fetch_channel_
              ->publish(exchange_name, routing_key, envelope, AMQP::autodelete|AMQP::mandatory)
              .onError([deferred](const char *message) {
                  deferred->report_error(Error(BrokerError::PUBLISH, message));
              });


      fetch_channel_
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

      if (listen_channel_ != nullptr) {
        throw_abort("Listener channel already used, you must create a new broker...");
      }

      listen_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection_.get()));

      listen_channel_->onError([deferred](const char *message) {
          deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, message));
      });

      std::promise<void> on_ready_barrier;

      listen_channel_->onReady([&on_ready_barrier] {
          on_ready_barrier.set_value();
      });

      on_ready_barrier.get_future().wait();

      // create a queue
      listen_channel_

              ->declareQueue(queue, AMQP::durable)

              .onError([deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_DECLARATION, message));
              });


      for (auto &routing_key: keys) {

        listen_channel_

                ->bindQueue(exchange_name, queue, routing_key)

                .onError([&deferred, this, routing_key, queue](const char *message) {

                    deferred->
                            report_error(
                            capy::Error(BrokerError::QUEUE_BINDING,
                                        error_string("%s: %s:%s <- %s", message, exchange_name.c_str())));
                });
      }

      listen_channel_

              ->consume(queue)

              .onReceived([this, deferred, &queue](
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
                          envelope.setExpiration("60000");

                          mutex.lock();
                          listen_channel_->ack(deliveryTag);
                          auto channel = Channel(connection_.get());
                          mutex.unlock();

                          channel.startTransaction()
                                  .onError([deferred](const char *message) {
                                      deferred->report_error(capy::Error(BrokerError::PUBLISH, message));
                                  });

                          channel.publish("", replay_to, envelope)
                                  .onError([deferred](const char *message) {
                                      deferred->report_error(capy::Error(BrokerError::PUBLISH, message));
                                  });

                          channel.commitTransaction()
                                  .onError([deferred](const char *message) {
                                      deferred->report_error(capy::Error(BrokerError::PUBLISH, message));
                                  });

                        }

                        catch (json::exception &exception) {
                          ///
                          /// Some programmatic exception is not processing properly
                          ///
                          mutex.lock();
                          listen_channel_->ack(deliveryTag);
                          mutex.unlock();
                          throw_abort(exception.what());
                        }
                        catch (...) {
                          mutex.lock();
                          listen_channel_->ack(deliveryTag);
                          mutex.unlock();
                          throw_abort("Unexpected exception...");
                        }
                    });

                  }

                  catch (json::exception &exception) {
                    listen_channel_->ack(deliveryTag);
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, exception.what()));
                  }
                  catch (...) {
                    listen_channel_->ack(deliveryTag);
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, "Unexpected exception..."));
                  }

              })

              .onError([deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_CONSUMING, message));
              });

      return *deferred;

    }
}