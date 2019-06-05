//
// Created by denn nevera on 2019-05-31.
//

#include "capy_amqp_exchange.hpp"
#include "amqp.h"
#include "amqp_tcp_socket.h"

namespace capy::amqp {

    bool die_on_amqp_error(amqp_rpc_reply_t x, char const *context, string &return_string);

    class ExchangeImpl {
    public:
        amqp_socket_t *socket;
        amqp_connection_state_t conn;
    };

    Result<Exchange> Exchange::Bind(const capy::amqp::Address &address, const std::string &exchange_name) {


      amqp_connection_state_t conn = amqp_new_connection();
      amqp_socket_t *socket = amqp_tcp_socket_new(conn);

      if (!socket) {
        return capy::make_unexpected(capy::Error(ExchangeError::CONNECTION_ERROR,
                                                 error_string("Tcp socket could not be created...")));
      }

      if (amqp_socket_open(socket, address.get_hostname().c_str(), static_cast<int>(address.get_port()))) {
        return capy::make_unexpected(capy::Error(ExchangeError::CONNECTION_ERROR,
                                                 error_string("Tcp socket could not be opened for: %s:%i",
                                                              address.get_hostname().c_str(),
                                                              address.get_port())));
      }

      amqp_rpc_reply_t ret = amqp_login(conn, address.get_vhost().c_str(),
                                        AMQP_DEFAULT_MAX_CHANNELS,
                                        AMQP_DEFAULT_FRAME_SIZE,
                                        0,
                                        AMQP_SASL_METHOD_PLAIN,
                                        address.get_login().get_username().c_str(),
                                        address.get_login().get_password().c_str());


      string error_message;
      if (die_on_amqp_error(ret, "Login", error_message)) {
        return capy::make_unexpected(capy::Error(ExchangeError::LOGIN_ERROR, error_message));
      }

      auto impl = shared_ptr<ExchangeImpl>(new ExchangeImpl);

      impl->conn = conn;

      return Exchange(impl);

    }

    Exchange::Exchange(const std::shared_ptr<capy::amqp::ExchangeImpl> &impl) : impl_(impl) {}

    Exchange::Exchange() : impl_(nullptr) {}

    void Exchange::fetch(const capy::amqp::json &message, const std::vector<std::string> &keys) {

    }

    void Exchange::listen(const std::string &queue_name, const capy::amqp::MessageHandler &on_data) {
      on_data(json());
    }

    void Exchange::publish(const capy::amqp::json &message, const std::string &queue_name) {

    }

    ///
    /// Errors...
    ///

    std::string ExchangeErrorCategory::message(int ev) const {
      switch (ev) {
        case static_cast<int>(ExchangeError::CONNECTION_ERROR):
          return "Connection error";
        default:
          return ErrorCategory::message(ev);
      }
    }

    const std::error_category &excahnge_error_category() {
      static ExchangeErrorCategory instance;
      return instance;
    }

    std::error_condition make_error_condition(capy::amqp::ExchangeError e) {
      return std::error_condition(
              static_cast<int>(e),
              capy::amqp::excahnge_error_category());
    }

    bool die_on_amqp_error(amqp_rpc_reply_t x, char const *context, string &return_string) {

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


