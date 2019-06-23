//
// Created by denn nevera on 2019-06-21.
//

#include "amqp_broker_impl.h"
#include <condition_variable>
#include <sstream>

namespace capy::amqp {

    class Channel: public AMQP::TcpChannel{
        typedef AMQP::TcpChannel __TcpChannel;
    public:
        using __TcpChannel::__TcpChannel;
         ~Channel() override {
          std::cout << " ~Channel("<< this->id() << ") .... " << std::endl;
        }
    };

    inline static std::string cretate_unique_id() {
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

    void BrokerImpl::fetch_message(
            const capy::json &message,
            const std::string &routing_key,
            const capy::amqp::FetchHandler &on_data) {

      std::promise<Result<std::string>> queue_declaration;

      if (fetch_channel_== nullptr) {
        fetch_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection_.get()));
      }

      fetch_channel_->

              declareQueue(AMQP::exclusive|AMQP::autodelete)

              .onSuccess([&queue_declaration](const std::string &name, uint32_t messagecount, uint32_t consumercount){

                  queue_declaration.set_value(name);
              })

              .onError([&queue_declaration](const char *message) {

                  queue_declaration.set_value(capy::make_unexpected(Error(BrokerError::QUEUE_DECLARATION,message)));
              });

      auto queue = queue_declaration.get_future().get();

      if (!queue){
        return on_data(capy::make_unexpected(queue.error()));
      }

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));

      envelope.setDeliveryMode(2);
      envelope.setCorrelationID(cretate_unique_id());
      envelope.setReplyTo(queue.value());


      fetch_channel_->

              consume(envelope.replyTo(), AMQP::noack)

              .onReceived([&on_data](

                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  capy::json received = json::from_msgpack(buffer);

                  on_data(received);

              })

              .onError([&on_data](const char *message) {

                  on_data(capy::make_unexpected(Error(BrokerError::DATA_RESPONSE, message)));

              });


      fetch_channel_->startTransaction();

      fetch_channel_->
              publish(exchange_name, routing_key, envelope, AMQP::autodelete|AMQP::mandatory)

              .onError([&on_data](const char *message) {
                  on_data(capy::make_unexpected(Error(BrokerError::PUBLISH, message)));
              });


      fetch_channel_->
              commitTransaction()

              .onError([&on_data](const char *message) {
                  on_data(capy::make_unexpected(Error(BrokerError::PUBLISH, message)));
              });
    }

    void BrokerImpl::listen_messages(const std::string &queue,
                                     const std::vector<std::string> &keys,
                                     const capy::amqp::ListenHandler &on_data) {

      if (listen_channel_ != nullptr) {
        Result<json> replay;
        return on_data(
                capy::make_unexpected(capy::Error(BrokerError::LISTENER_CONFLICT,
                                                  error_string("Listener channel already used, you must create a new broker..."))),
                replay);
      }

      listen_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection_.get()));

      std::promise<std::string> error_message;

      listen_channel_->
              onError([&error_message,&on_data](const char *message){
          try {
            error_message.set_value(message);
          }
          catch (std::exception& e){}
          Result<json> replay;
          return on_data(
                  capy::make_unexpected(capy::Error(BrokerError::CHANNEL_MESSAGE, message)),
                  replay);
      });

      listen_channel_->
              onReady([&error_message]{
          error_message.set_value("");
      });

      auto error = error_message.get_future().get();

      if (!error.empty()) {
        Result<json> replay;
        return on_data(
                capy::make_unexpected(capy::Error(BrokerError::CHANNEL_READY, error)),
                replay);
      }

      // create a queue
      listen_channel_->declareQueue(queue, AMQP::durable)

              .onError([&on_data](const char *message){

                  Result<json> replay;
                  return on_data(
                          capy::make_unexpected(capy::Error(BrokerError::QUEUE_DECLARATION, message)),
                          replay);

              });

      for (auto &routing_key: keys) {

        listen_channel_->bindQueue(exchange_name, queue, routing_key)

                .onError([&on_data, this, routing_key, queue](const char *message) {
                    Result<json> replay;
                    return on_data(
                            capy::make_unexpected(capy::Error(BrokerError::QUEUE_BINDING,
                                                              error_string("%s: %s:%s <- %s",message, exchange_name.c_str(), queue.c_str(), routing_key.c_str()))),
                            replay);
                });

      }


      listen_channel_->consume(queue)

              .onReceived([this,&on_data,&queue](
                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  listen_channel_->ack(deliveryTag);

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  auto replay_to = message.replyTo();
                  auto cid = message.correlationID();

                  try {

                    capy::json received = json::from_msgpack(buffer);

                    std::promise<Result<capy::json>> replay_barrier;

                    capy::amqp::Task::Instance().async([
                                                               replay_to,
                                                               &received,
                                                               &on_data,
                                                               &replay_barrier
                                                       ] {


                        Result<capy::json> replay;

                        on_data(Rpc(replay_to, received), replay);

                        replay_barrier.set_value(replay);

                    });

                    auto replay = replay_barrier.get_future().get();

                    if (!replay) {
                      return;
                    }

                    if (replay->empty()) {
                      Result<json> replay;
                      return on_data(
                              capy::make_unexpected(capy::Error(BrokerError::EMPTY_REPLAY, "replay is empty")),
                              replay);
                    }

                    auto data = json::to_msgpack(replay.value());

                    AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));

                    envelope.setCorrelationID(cid);
                    envelope.setExpiration("60000");

                    listen_channel_->startTransaction();

                    listen_channel_->publish("", replay_to, envelope);

                    listen_channel_->commitTransaction()

                            .onError([&on_data](const char *message){

                                Result<json> replay;
                                return on_data(
                                        capy::make_unexpected(capy::Error(BrokerError::PUBLISH, message)),
                                        replay);

                            });

                  }
                  catch (json::exception &exception) {
                    throw_abort(exception.what());
                  }
                  catch (...) {
                    throw_abort("Unexpected exception...");
                  }

              })

              .onError([&on_data](const char *message) {

                  Result<json> replay;
                  return on_data(
                          capy::make_unexpected(capy::Error(BrokerError::QUEUE_CONSUMING, message)),
                          replay);

              });

    }
}