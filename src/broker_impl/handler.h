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

        virtual void onClosed(AMQP::TcpConnection *connection) override
        {
          // make sure compilers dont complain about unused parameters
          (void) connection;
          std::cout << "closed" << std::endl;

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
          std::cout << "is lost... " << std::endl;
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
