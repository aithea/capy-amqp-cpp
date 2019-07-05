//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <memory>
#include "capy/amqp_broker.h"

namespace capy::amqp {

    /**
    *  Custom handler
    */
    class ConnectionHandler : public AMQP::LibUvHandler {
    private:
        uint16_t heartbeat_timeout_;

        /**
         *  Method that is called when a connection error occurs
         *  @param  connection
         *  @param  message
         */
        virtual void onError(AMQP::TcpConnection* connection, const char* message) override {
          (void) connection;
          if (deferred) {
            deferred->report_error(capy::Error(capy::amqp::BrokerError::CONNECTION, message));
          }
        }

        /**
         *  Method that is called when the TCP connection ends up in a connected state
         *  @param  connection  The TCP connection
         */
        virtual void onConnected(AMQP::TcpConnection* connection) override {
          (void) connection;
        }

        /**
         *  Method that is called when the TCP connection ends up in a connected state
         *  This method is called after the TCP connection has been set up, but before
         *  the (optional) secure TLS connection is ready, and before the AMQP login
         *  handshake has been completed. If this step has been set, the onLost()
         *  method will also always be called when the connection is closed.
         *  @param  connection  The TCP connection
         */
        virtual void onClosed(AMQP::TcpConnection *connection) override
        {
          // make sure compilers dont complain about unused parameters
          (void) connection;
        }

        /**
         *  Method that is called when the TCP connection is lost or closed. This
         *  is always called if you have also received a call to onConnected().
         *  @param  connection  The TCP connection
         */
        virtual void onLost(AMQP::TcpConnection *connection) override
        {
          // make sure compilers dont complain about unused parameters
          (void) connection;
          if (deferred) {
            deferred->report_error(capy::Error(capy::amqp::BrokerError::CONNECTION_LOST, "connection lost"));
          }
        }

        virtual void onHeartbeat(AMQP::TcpConnection *connection) override
        {
          int ret = connection->heartbeat();
        }

        virtual uint16_t onNegotiate(AMQP::TcpConnection *connection, uint16_t interval) override
        {
          (void) interval;
          (void) connection;
          return heartbeat_timeout_;
        }

    public:
        /**
         *  Constructor
         *  @param  uv_loop
         */
        ConnectionHandler(uv_loop_t* loop, uint16_t heartbeat_timeout):
        AMQP::LibUvHandler(loop),
        heartbeat_timeout_(heartbeat_timeout)
        {
        }

        /**
         *  Destructor
         */
        virtual ~ConnectionHandler() = default;

        std::shared_ptr<capy::amqp::DeferredListen> deferred = nullptr;
    };
}
