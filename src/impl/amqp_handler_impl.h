//
// Created by denn nevera on 2019-06-21.
//

#pragma once

#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <memory>

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
          std::cout << "error: " << message << std::endl;
        }

        /**
         *  Method that is called when the TCP connection ends up in a connected state
         *  @param  connection  The TCP connection
         */
        virtual void onConnected(AMQP::TcpConnection* connection) override {
          std::cout << "connected" << std::endl;
        }

        virtual uint16_t onNegotiate(AMQP::TcpConnection *connection, uint16_t interval)
        {
          // we accept the suggestion from the server, but if the interval is smaller
          // that one minute, we will use a one minute interval instead
          if (interval < 60) interval = 60;

          // @todo
          //  set a timer in your event loop, and make sure that you call
          //  connection->heartbeat() every _interval_ seconds if no other
          //  instruction was sent in that period.

          // return the interval that we want to use
          return interval;
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
    };
}
