//
// Created by denn on 2019-02-03.
//

#pragma once

#include <optional>
#include <string>
#include <functional>

#include "capy_common.hpp"

namespace capy::amqp {

    enum address_error {
        PARSE = 3001,
        EMPTY = 3002,
        NOT_SUPPORTED = 10000,
        UNKNOWN_ERROR = 10001
    };

    /**
     * Address class
     */
    class Address {

    public:

        /**
         * Address protocol is supported by the current version
         */
        enum protocol{
            amqp = 0,
            unknown
        };

    public:

        /**
         * Parse address string
         * @param address - url string
         * @param error - error handler
         * @return optional Address object
         */
        static std::optional<Address> Parse(
                const std::string &address,
                const ErrorHandler &error = default_error_handler);

        /**
         * Copy Address object
         */
        Address(const Address &);
        Address& operator=(const Address&);

        /**
         * Get url protocol
         * @return - url protocol
         */
        const protocol get_protocol() const;

        /**
         * Get host
         * @return - host string
         */
        const std::string &get_host() const;

        /**
         * Get port
         * @return port number
         */
        const uint16_t get_port() const;

    protected:
        Address(){};
    };
}
