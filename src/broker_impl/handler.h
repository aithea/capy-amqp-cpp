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
        /**
         *  Method that is called when a connection error occurs
         *  @param  connection
         *  @param  message
         */
        virtual void onError(AMQP::TcpConnection* connection, const char* message) override {
          (void) connection;
          std::cout << "error: " << message << std::endl;
        }

        /**
         *  Method that is called when the TCP connection ends up in a connected state
         *  @param  connection  The TCP connection
         */
        virtual void onConnected(AMQP::TcpConnection* connection) override {
          (void) connection;
        }

        virtual void onClosed(AMQP::TcpConnection *connection) override
        {
          // make sure compilers dont complain about unused parameters
          (void) connection;
          std::cerr << "closed" << std::endl;
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

    public:
        /**
         *  Constructor
         *  @param  uv_loop
         */
        ConnectionHandler(uv_loop_t* loop) :
        AMQP::LibUvHandler(loop) {}

        /**
         *  Destructor
         */
        virtual ~ConnectionHandler() = default;

        std::shared_ptr<capy::amqp::DeferredListen> deferred = nullptr;
    };
}
