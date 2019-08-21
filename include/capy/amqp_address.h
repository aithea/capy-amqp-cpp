//
// Created by denn on 2019-02-03.
//

#pragma once

#include <optional>
#include <string>
#include <functional>

#include "capy/amqp_common.h"

namespace capy::amqp {

    class Address: public CommonAddress {
    public:

        using capy::CommonAddress::CommonAddress;

        /**
         * Address protocol is supported by the current version
         */
        struct Protocol: url::Protocol {
            enum Type:url::Protocol::Type {
                amqp  = 10,
                amqps = 11,
                unknown
            };
            Type type;
        };

    public:

        /***
         * Create new Address object from address string
         * @param address string
         * @return expected result or capy:Error
         */

        static capy::Result<Address> From(const std::string &address);

        /**
        * Get url protocol
        * @return - url protocol
        */
        const Protocol& get_protocol() const { return protocol_; };

        /**
         * Get virtual host
         * @return port virtual host string
         */
        const std::string& get_vhost() const { return vhost_; };

    private:
        Address(const url::Parts& address);
        Protocol protocol_;
        std::string vhost_;
    };

    inline bool operator==(const capy::amqp::Address::Protocol& lhs, const capy::amqp::Address::Protocol::Type rhs) { return lhs.type == rhs;}
    inline bool operator==(const capy::amqp::Address::Protocol::Type lhs, const capy::amqp::Address::Protocol & rhs) { return lhs == rhs.type;}
}