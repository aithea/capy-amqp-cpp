//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include "amqp_handler_impl.h"
#include "capy/amqp_broker.h"

#include "amqp.h"
#include "amqp_tcp_socket.h"

#include "capy/dispatchq.h"
#include <assert.h>
#include <atomic>
#include <thread>
#include <future>
#include <capy/amqp_deferred.h>

namespace capy::amqp {

    bool die_on_amqp_error(amqp_rpc_reply_t x, char const *context, std::string &return_string);


    class BrokerImpl {

    public:
        std::mutex mutex;

        std::shared_ptr<uv_loop_t> loop_;
        std::shared_ptr<ConnectionHandler> handler_;
        std::unique_ptr<AMQP::TcpConnection> connection_;
        std::unique_ptr<AMQP::TcpChannel> listen_channel_;
        std::unique_ptr<AMQP::TcpChannel> fetch_channel_;

    public:
        amqp_channel_t channel_id;
        std::string exchange_name;
        bool isExited;
        static std::atomic_uint32_t correlation_id;
        amqp_connection_state_t producer_conn;


    public:
        BrokerImpl(const capy::amqp::Address& address, const std::string& exchange);

        BrokerImpl() : isExited(false), listen_channel_(nullptr) {}

        BrokerImpl(amqp_channel_t aChannel_id) : channel_id(aChannel_id), isExited(false) {}

        ~BrokerImpl();
//        {
//
//          isExited = true;
//
//          std::string error_message;
//
//          ///
//          /// @todo
//          /// Solve debug logging problem...
//          ///
//
////          if (die_on_amqp_error(amqp_channel_close(producer_conn, channel_id, AMQP_REPLY_SUCCESS), "Closing channel",
////                                error_message)) {
////            std::cerr << error_message << std::endl;
////          }
////
////          if (die_on_amqp_error(amqp_connection_close(producer_conn, AMQP_REPLY_SUCCESS), "Closing connection",
////                                error_message)) {
////            std::cerr << error_message << std::endl;
////          }
////
////          if (amqp_destroy_connection(producer_conn)) {
////            std::cerr << "Ending connection error..." << std::endl;
////          }
//
//        }

        void consume_once(
                const std::function<bool(const capy::json &body, const std::string &reply_to,
                                         const std::string &routing_key, const std::string &correlation_id,
                                         uint64_t delivery_tag)> on_replay,
                const std::function<void()> on_complete) {

          amqp_rpc_reply_t ret;
          amqp_envelope_t envelope;

          ret = amqp_consume_message(producer_conn, &envelope, NULL, 0);

          std::string correlation_id;

          bool do_complete = false;

          if (AMQP_RESPONSE_NORMAL == ret.reply_type) {

            if ((envelope.message.properties._flags & AMQP_BASIC_CORRELATION_ID_FLAG) /*&& !correlation_id.empty()*/) {
              correlation_id = std::string(
                      static_cast<char *>(envelope.message.properties.correlation_id.bytes),
                      envelope.message.properties.correlation_id.len);
            }

            std::string reply_to_routing_key;
            std::string routing_key;

            if (envelope.routing_key.bytes != NULL) {
              routing_key = std::string(static_cast<char *>(envelope.routing_key.bytes), envelope.routing_key.len);
            }

            if (envelope.message.properties._flags & AMQP_BASIC_REPLY_TO_FLAG) {

              reply_to_routing_key = std::string(
                      static_cast<char *>(envelope.message.properties.reply_to.bytes),
                      envelope.message.properties.reply_to.len);
            }

            std::vector<std::uint8_t> buffer(
                    static_cast<std::uint8_t *>(envelope.message.body.bytes),
                    static_cast<std::uint8_t *>(envelope.message.body.bytes) + envelope.message.body.len);

            try {

              capy::json json = json::from_msgpack(buffer);
              do_complete = on_replay(json, reply_to_routing_key, routing_key, correlation_id, envelope.delivery_tag);

            }

            catch (std::exception &exception) {

              amqp_queue_delete(producer_conn, channel_id, amqp_cstring_bytes(reply_to_routing_key.c_str()), 1, 1);
              std::cerr << "capy::Broker::error: consume message: " << exception.what() << std::endl;

            }

          }

          amqp_destroy_envelope(&envelope);
          amqp_maybe_release_buffers(producer_conn);
          amqp_maybe_release_buffers_on_channel(producer_conn, channel_id);

          if (do_complete) on_complete();
        }

        void consuming(
                const std::function<bool(const capy::json &body, const std::string &reply_to,
                                         const std::string &routing_key, const std::string &correlation_id,
                                         uint64_t delivery_tag)> on_replay) {

          while (!isExited) {
            consume_once(on_replay, [] {});
          }

        }


        DeferredListen& listen_messages(const std::string &queue, const std::vector<std::string> &keys);

        DeferredFetch& fetch_message(const json& message, const std::string& routing_key);

        Error publis_message(const json& message, const std::string& routing_key);


        Error publish(
                const std::string &correlation_id,
                const capy::json &message,
                const std::string &exchange_name,
                const std::string &routing_key,
                const std::optional<FetchHandler> &on_data) {

          amqp_basic_properties_t props;

          std::string reply_to_queue;

          props._flags = 0;

          if (on_data.has_value()) {

            //
            // create private reply_to queue
            //

            amqp_queue_declare_ok_t *r =
                    amqp_queue_declare(
                            producer_conn, channel_id, amqp_empty_bytes, 0, 0, 1, 1, amqp_empty_table);

            std::string error_message;

            if (!r) {
              return capy::Error(BrokerError::QUEUE_DECLARATION, "Connection has been lost");
            }

            if (die_on_amqp_error(amqp_get_rpc_reply(producer_conn), "Publisher declaring queue", error_message)) {
              return capy::Error(BrokerError::QUEUE_DECLARATION, error_message);
            }

            if (r->queue.bytes == NULL) {
              return capy::Error(BrokerError::QUEUE_DECLARATION, "Out of memory while copying queue name");
            }

            reply_to_queue = std::string(static_cast<char *>(r->queue.bytes), r->queue.len);

            props.reply_to = amqp_cstring_bytes(reply_to_queue.c_str());

            props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG |
                           AMQP_BASIC_DELIVERY_MODE_FLAG |
                           AMQP_BASIC_REPLY_TO_FLAG;

            props.delivery_mode = 2; /* persistent delivery mode */
            props.content_type = amqp_cstring_bytes("text/plain");

            if (props.reply_to.bytes == NULL) {
              return capy::Error(BrokerError::QUEUE_DECLARATION, "Out of memory while copying queue name");
            }

          }

          props._flags |= AMQP_BASIC_CORRELATION_ID_FLAG;

          props.correlation_id = amqp_cstring_bytes(correlation_id.c_str());

          //
          // publish
          //

          amqp_bytes_t message_bytes;
          auto data = json::to_msgpack(message);

          message_bytes.len = data.size();
          message_bytes.bytes = data.data();

          auto ret = amqp_basic_publish(producer_conn,
                                        channel_id,
                                        exchange_name.empty() ? amqp_empty_bytes : amqp_cstring_bytes(
                                                exchange_name.c_str()),
                                        amqp_cstring_bytes(routing_key.c_str()),
                                        0,
                                        0,
                                        &props,
                                        message_bytes);


          if (ret) {
            return Error(BrokerError::PUBLISH,
                         error_string("Could not produce message to routingkey: %s", routing_key.c_str()));
          }

          if (on_data.has_value()) {

            std::string error_message;

            //
            // Consume queue
            //
            amqp_basic_consume(producer_conn,
                               channel_id,
                               amqp_cstring_bytes(reply_to_queue.c_str()),
                               amqp_empty_bytes,
                               0, 0, 0, amqp_empty_table);

            if (die_on_amqp_error(amqp_get_rpc_reply(producer_conn), "Consuming rpc", error_message)) {
              return capy::Error(BrokerError::QUEUE_CONSUMING, error_message);
            }

            consume_once(
                    [&on_data, this, correlation_id, &props]
                            (const capy::json &body, const std::string &reply_to, const std::string &routing_key,
                             const std::string &replay_correlation_id, uint64_t delivery_tag) {

                        if (correlation_id != replay_correlation_id)
                          return false;

                        capy::amqp::Task::Instance().async([=] {
                            on_data.value()(body);
                        });

                        return true;
                    },

                    [this, &props] {
                        amqp_queue_delete(producer_conn, channel_id, props.reply_to, 0, 0);
                    });

          }

          return Error(CommonError::OK);
        }

        void listen(
                const std::string &queue,
                const std::vector<std::string> &routing_keys,
                const capy::amqp::ListenHandler &on_data) {
          try {

            std::string error_message;

            //
            //
            // declare named queue
            //

            amqp_queue_declare_ok_t *declare =
                    amqp_queue_declare(producer_conn, channel_id, amqp_cstring_bytes(queue.c_str()), 0, 1, 0, 0,
                                       amqp_empty_table);


            if (die_on_amqp_error(amqp_get_rpc_reply(producer_conn), "Listener declaring queue", error_message)) {
              Result<json> replay;
              return on_data(
                      capy::make_unexpected(capy::Error(BrokerError::QUEUE_DECLARATION, error_message)),
                      replay);
            }

            if (declare->queue.bytes == NULL) {
              Result<json> replay;
              return on_data(
                      capy::make_unexpected(
                              capy::Error(BrokerError::QUEUE_DECLARATION, "Queue name declaration mismatched")),
                      replay);
            }

            //
            // bind routing key
            //

            for (auto &routing_key: routing_keys) {

              amqp_queue_bind(producer_conn,
                              channel_id,
                              amqp_cstring_bytes(queue.c_str()),
                              amqp_cstring_bytes(exchange_name.c_str()),
                              amqp_cstring_bytes(routing_key.c_str()), amqp_empty_table);

              if (die_on_amqp_error(amqp_get_rpc_reply(producer_conn), "Binding queue", error_message)) {
                Result<json> replay;
                return on_data(
                        capy::make_unexpected(capy::Error(BrokerError::QUEUE_BINDING, error_message)),
                        replay);
              }
            }

            //
            // Consume queue
            //
            amqp_basic_consume(producer_conn,
                               channel_id,
                               amqp_cstring_bytes(queue.c_str()),
                               amqp_empty_bytes,
                               0, 0, 0, amqp_empty_table);

            if (die_on_amqp_error(amqp_get_rpc_reply(producer_conn), "Consuming", error_message)) {
              Result<json> replay;
              return on_data(
                      capy::make_unexpected(capy::Error(BrokerError::QUEUE_CONSUMING, error_message)),
                      replay);
            }

            consuming(
                    [&on_data, this](const capy::json &body, const std::string &reply_to,
                                     const std::string &routing_key, const std::string &correlation_id,
                                     uint64_t delivery_tag) {

                        amqp_basic_ack(producer_conn, channel_id, delivery_tag, 1);

                        capy::amqp::Task::Instance().async([=] {

                            try {

                              Result<json> replay;

                              on_data(Rpc(routing_key, body), replay);

                              if (!replay) {
                                return;
                              }

                              if (replay->empty()) {
                                std::cerr << "capy::Broker::error: replay is empty" << std::endl;
                                return;
                              }

                              publish(correlation_id, replay.value(), "", reply_to, std::nullopt);

                            }
                            catch (json::exception &exception) {
                              throw_abort(exception.what());
                            }
                            catch (...) {
                              throw_abort("Unexpected excaption...");
                            }
                        });

                        return true;
                    });

          }
          catch (std::exception &exception) {
            std::cerr << "capy::Broker::error: " << exception.what() << std::endl;
          }
          catch (...) {
            std::cerr << "capy::Broker::error: Unknown error" << std::endl;
          }
        }

    };
}