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

    BrokerImpl::~BrokerImpl() {}

//    BrokerImpl::BrokerImpl(
//            const capy::amqp::Address &address,
//            const std::string &exchange):
//
//            loop_(uv_loop_t_allocator(), uv_loop_t_deallocator()),
//
//            handler_(new ConnectionHandler(loop_.get())),
//
//            pool_(std::make_shared<ConnectionPool>(
//                    std::thread::hardware_concurrency(),
//                    [&address,this](size_t index){
//                        return new Connection(handler_, address);
//                    })),
//
//            //connection_(new AMQP::TcpConnection(handler_.get(), to_address(address))),
//            exchange_name_(exchange)
//    {
//
////      pool_ = std::make_shared<ConnectionPool>(std::thread::hardware_concurrency(), [&address,this](size_t index){
////                return new Connection(handler_, address);
////            });
//
//      std::thread thread_loop([this] {
//          uv_run(loop_.get(), UV_RUN_DEFAULT);
//      });
//
//      thread_loop.detach();
//    }

    Error BrokerImpl::publish_message(const capy::json &message, const std::string &routing_key) {
      std::promise<Result<std::string>> queue_declaration;

      auto data = json::to_msgpack(message);

      AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));
      envelope.setDeliveryMode(2);

      //mutex_.lock();
      //auto channel = Channel(connection_.get());
      //mutex_.unlock();

      auto connection = connection_pool_->acquire();

      std::promise<std::string> publish_barrier;

      connection->get_channel()->startTransaction();

      connection->get_channel()->publish(exchange_name_, routing_key, envelope, AMQP::autodelete|AMQP::mandatory);

      connection->get_channel()->commitTransaction()
              .onSuccess([&publish_barrier](){
                  publish_barrier.set_value("");
              })
              .onError([&publish_barrier](const char *message) {
                  publish_barrier.set_value(message);
              });

      auto error = publish_barrier.get_future().get();

      if (!error.empty()){
        connection_pool_->release(connection);
        return Error(amqp::BrokerError::PUBLISH, error);
      }

      connection_pool_->release(connection);

      return Error(amqp::CommonError::OK);
    }


    DeferredFetch& BrokerImpl::fetch_message(
            const capy::json &message,
            const std::string &routing_key) {

      auto deferred = std::make_shared<capy::amqp::DeferredFetch>();

      std::cout << "1 ... acquire... " << std::endl;

      //std::lock_guard lock(mutex_);

      if (fetch_channel_ == nullptr) {
        auto connection = connection_pool_->acquire();
        fetch_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection->get_connection())); //connection->get_channel();

      }
      //  fetch_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection_.get()));

      std::promise<void> ready_barrier;

      //auto &channel = *fetch_channel_; //Channel(connection->get_connection());

      fetch_channel_->onReady([&ready_barrier](){
          std::cout << "1.1 ... onReady... " << std::endl;
          ready_barrier.set_value();
      });

        ready_barrier.get_future().wait();
//      }
//

      std::cout << "2 ... acquire... " << std::endl;

      fetch_channel_

              ->declareQueue(AMQP::exclusive | AMQP::autodelete)

              .onSuccess(
                      [this,
                              &message,
                              deferred,
                              routing_key
                              ]

                              (const std::string &name, uint32_t messagecount, uint32_t consumercount) {

                          std::cout << "3 ... message... " << /*message.dump(4) << */std::endl;

                          auto data = json::to_msgpack(message);

                          AMQP::Envelope envelope(static_cast<char*>((void *)data.data()), static_cast<uint64_t>(data.size()));

                          envelope.setDeliveryMode(2);
                          envelope.setCorrelationID(create_unique_id());
                          envelope.setReplyTo(name);

                          //auto connection = connection_pool_->acquire();

                          //std::lock_guard lock(mutex_);

                          fetch_channel_

                                  ->consume(envelope.replyTo(), AMQP::noack)

                                  .onReceived([deferred, this](

                                      const AMQP::Message &message,
                                          uint64_t deliveryTag,
                                          bool redelivered) {

                                      //connection_pool_->release(connection);

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

                                  })

                                  .onFinalize([]{
                                      std::cout << "7 ... onFinalize... "  << std::endl;
                                      //connection_pool_->release(connection);
                                  });

                          fetch_channel_->startTransaction();

                          fetch_channel_->publish(exchange_name_, routing_key, envelope, AMQP::autodelete|AMQP::mandatory);

                          fetch_channel_->commitTransaction()
                                  .onError([deferred](const char *message) {
                                      deferred->report_error(Error(BrokerError::PUBLISH, message));
                                  });
                      })

              .onError([deferred](const char *message) {
                  std::cout << "5 ... onError... "  << message << std::endl;
                  deferred->report_error(Error(BrokerError::QUEUE_DECLARATION, message));
              })

              .onFinalize([]{
                  std::cout << "6 ... onFinalize... "  << std::endl;
              });

      //connection_pool_->release(connection);

      return *deferred;
    }

    ///
    /// MARK: - listen
    ///
    DeferredListen& BrokerImpl::listen_messages(const std::string &queue,
                                                const std::vector<std::string> &keys) {

      auto deferred = std::make_shared<capy::amqp::DeferredListen>();

//      if (listen_channel_ != nullptr) {
//        throw_abort("Listener channel already used, you must create a new broker...");
//      }

      auto connection = connection_pool_->acquire();

      //listen_channel_ = std::unique_ptr<AMQP::TcpChannel>(new AMQP::TcpChannel(connection_.get()));

      connection->get_channel()->onError([deferred](const char *message) {
          deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, message));
      });

      std::promise<void> on_ready_barrier;

      connection->get_channel()->onReady([&on_ready_barrier] {
          on_ready_barrier.set_value();
      });

      on_ready_barrier.get_future().wait();

      // create a queue
      connection->get_channel()

              ->declareQueue(queue, AMQP::durable)

              .onError([&deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_DECLARATION, message));
              });


      for (auto &routing_key: keys) {

        connection->get_channel()

                ->bindQueue(exchange_name_, queue, routing_key)

                .onError([&deferred, this, routing_key, queue](const char *message) {

                    deferred->
                            report_error(
                            capy::Error(BrokerError::QUEUE_BINDING,
                                        error_string("%s: %s:%s <- %s", message, exchange_name_.c_str())));
                });
      }

      connection->get_channel()

              ->consume(queue)

              .onReceived([this, deferred, &queue, connection](
                      const AMQP::Message &message,
                      uint64_t deliveryTag,
                      bool redelivered) {

                  std::vector<std::uint8_t> buffer(
                          static_cast<std::uint8_t *>((void*)message.body()),
                          static_cast<std::uint8_t *>((void*)message.body()) + message.bodySize());

                  auto replay_to = message.replyTo();
                  auto cid = message.correlationID();

                  try {

                    //auto channel = std::shared_ptr<Channel>(new Channel(connection_.get()));

                    capy::json received = json::from_msgpack(buffer);

                    capy::amqp::Task::Instance().async([ this,
                                                               deferred,
                                                               replay_to,
                                                               received,
                                                               cid,
                                                               deliveryTag,
                                                               &connection
                                                               //channel
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
                          //envelope.setExpiration("60000");

                          {
                            std::lock_guard lock(mutex_);
                            connection->get_channel()->ack(deliveryTag);
                          }

                          auto publush_connection = connection_pool_->acquire();

                          auto channel = publush_connection->get_channel(); //std::shared_ptr<Channel>(new Channel(connection_.get()));

                          channel->startTransaction();

                          channel->publish("", replay_to, envelope);

                          channel->commitTransaction()
                                  .onError([deferred](const char *message) {
                                      deferred->report_error(capy::Error(BrokerError::PUBLISH, message));
                                  });

                          connection_pool_->release(publush_connection);

                        }

                        catch (json::exception &exception) {
                          ///
                          /// Some programmatic exception is not processing properly
                          ///

                          std::lock_guard lock(mutex_);
                          connection->get_channel()->ack(deliveryTag);
                          throw_abort(exception.what());
                        }
                        catch (...) {
                          std::lock_guard lock(mutex_);
                          connection->get_channel()->ack(deliveryTag);
                          throw_abort("Unexpected exception...");
                        }
                    });

                  }

                  catch (json::exception &exception) {
                    std::lock_guard lock(mutex_);
                    connection->get_channel()->ack(deliveryTag);
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, exception.what()));
                  }
                  catch (...) {
                    std::lock_guard lock(mutex_);
                    connection->get_channel()->ack(deliveryTag);
                    deferred->report_error(capy::Error(BrokerError::CHANNEL_MESSAGE, "Unexpected exception..."));
                  }

              })

              .onError([deferred](const char *message) {
                  deferred->report_error(capy::Error(BrokerError::QUEUE_CONSUMING, message));
              });

      return *deferred;

    }
}