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
          std::cerr << "Call delete from function uv_loop_t_deallocator from bind..." << std::endl;
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

        impl->exchange_name = exchange_name;
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

//    Result<Broker> Broker::Bind(const capy::amqp::Address& address, const std::string& exchange_name) {
//
//      amqp_channel_t channel_id = 1;
//
//      amqp_connection_state_t producer_conn = amqp_new_connection();
//      amqp_socket_t *producer_socket = amqp_tcp_socket_new(producer_conn);
//
//      if (!producer_socket) {
//        return capy::make_unexpected(capy::Error(BrokerError::CONNECTION,
//                                                 error_string("Tcp socket could not be created...")));
//      }
//
//      if (amqp_socket_open(producer_socket, address.get_hostname().c_str(), static_cast<int>(address.get_port()))) {
//        amqp_destroy_connection(producer_conn);
//        return capy::make_unexpected(capy::Error(BrokerError::CONNECTION,
//                                                 error_string("Tcp socket could not be opened for: %s:%i",
//                                                              address.get_hostname().c_str(),
//                                                              address.get_port())));
//      }
//
//      amqp_rpc_reply_t ret = amqp_login(producer_conn, address.get_vhost().c_str(),
//                                        AMQP_DEFAULT_MAX_CHANNELS,
//                                        AMQP_DEFAULT_FRAME_SIZE,
//                                        0,
//                                        AMQP_SASL_METHOD_PLAIN,
//                                        address.get_login().get_username().c_str(),
//                                        address.get_login().get_password().c_str());
//
//
//      std::string error_message;
//
//      if (die_on_amqp_error(ret, "Producer login", error_message)) {
//        amqp_connection_close(producer_conn, AMQP_REPLY_SUCCESS);
//        amqp_destroy_connection(producer_conn);
//        return capy::make_unexpected(capy::Error(BrokerError::LOGIN, error_message));
//      }
//
//      amqp_channel_open(producer_conn, channel_id);
//
//      if (die_on_amqp_error(amqp_get_rpc_reply(producer_conn), "Opening producer channel", error_message)) {
//        amqp_connection_close(producer_conn, AMQP_REPLY_SUCCESS);
//        amqp_destroy_connection(producer_conn);
//        return capy::make_unexpected(capy::Error(BrokerError::CHANNEL, error_message));
//      }
//
//      auto impl = std::shared_ptr<BrokerImpl>(new BrokerImpl(channel_id));
//
//      amqp_basic_qos(producer_conn,channel_id,0,1,0);
//
//      impl->producer_conn = producer_conn;
//      impl->exchange_name = exchange_name;
//
//      return Broker(impl);
//
//    }

    //
    // Broker constructors
    //
    Broker::Broker(const std::shared_ptr<capy::amqp::BrokerImpl> &impl) : impl_(impl) {}
    Broker::Broker() : impl_(nullptr) {}


    //
    // fetch
    //
    DeferredFetch& Broker::fetch(const capy::json& message, const std::string& routing_key) {
      //impl_->fetch_message(message, routing_key, on_data);
      return impl_->fetch_message(message, routing_key);
//      return capy::Error(CommonError::OK);
//      std::string correlation_id(std::to_string(BrokerImpl::correlation_id++).c_str());
//      return impl_->publish(correlation_id, message, impl_->exchange_name, routing_key, [&](const Result<json> &message){
//          try {
//            on_data(message);
//          }
//          catch (json::exception &exception) {
//            throw_abort(exception.what());
//          }
//          catch (...) {
//            throw_abort("Unexpected excaption...");
//          }
//      });
    }

    //
    // listen
    //
    DeferredListen& Broker::listen(
            const std::string& queue,
            const std::vector<std::string>& routing_keys/*,
            const capy::amqp::ListenHandler& on_data*/) {
      //return impl_->listen(queue, routing_keys, on_data);
      //return impl_->listen_messages(queue,routing_keys,on_data);
      return impl_->listen_messages(queue,routing_keys);
    }

    //
    // publish
    //
    Error Broker::publish(const capy::json& message, const std::string& routing_key) {
      //std::string correlation_id(std::to_string(BrokerImpl::correlation_id++).c_str());
      //return impl_->publish(correlation_id, message, impl_->exchange_name, routing_key, std::nullopt);
      return impl_->publis_message(message,routing_key);
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

    bool die_on_amqp_error(amqp_rpc_reply_t x, char const *context, std::string &return_string) {

      switch (x.reply_type) {

        case AMQP_RESPONSE_NORMAL:
          return false;

        case AMQP_RESPONSE_NONE:
          return_string = error_string("%s: missing RPC reply type!\n", context);
          break;

        case AMQP_RESPONSE_LIBRARY_EXCEPTION:
          return_string = error_string("%s: %s\n", context, amqp_error_string2(x.library_error));
          break;

        case AMQP_RESPONSE_SERVER_EXCEPTION:
          switch (x.reply.id) {
            case AMQP_CONNECTION_CLOSE_METHOD: {
              amqp_connection_close_t *m = (amqp_connection_close_t *) x.reply.decoded;

              return_string = error_string("%s: server connection error %uh, message: %.*s\n",
                                           context, m->reply_code, (int) m->reply_text.len,
                                           (char *) m->reply_text.bytes);
            }
              break;
            case AMQP_CHANNEL_CLOSE_METHOD: {
              amqp_channel_close_t *m = (amqp_channel_close_t *) x.reply.decoded;
              return_string = error_string("%s: server channel error %uh, message: %.*s\n",
                                           context, m->reply_code, (int) m->reply_text.len,
                                           (char *) m->reply_text.bytes);
            }
              break;
            default:
              return_string = error_string("%s: unknown server error, method id 0x%08X\n",
                                           context, x.reply.id);
          }
      }

      return true;
    }
}


