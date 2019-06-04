//
// Created by denn on 2019-02-03.
//

#pragma once

#include <optional>
#include <string>
#include <functional>


#include "capy_common.hpp"

namespace capy::amqp {

    /***
     * AMQP Address errors
     */
    enum class AddressError : PUBLIC_ENUM(CommonError) {
        /***
         * Parsing input address sgtring error
         */
        PARSE = EXTEND_ENUM(CommonError,LAST),
        /***
         * Input address string is empty
         */
        EMPTY,
        LAST
    };

    class AddressErrorCategory: public ErrorCategory
    {
    public:
        virtual std::string message(int ev) const override ;
    };

    const std::error_category& address_error_category();
    std::error_condition make_error_condition(capy::amqp::AddressError e);

    class AddressImpl;

    /**
     * Address class
     */
    class Address {

    public:

        /**
         * Address protocol is supported by the current version
         */
        enum protocol:int {
            amqp = 0,
            unknown
        };

    public:

        /***
         * Create new Address object from address string
         * @param address string
         * @return expected result or capy:Error
         */
        static capy::Result<Address> From(const std::string &address);

        /**
         * Copy Address object
         */
        Address(const Address &);

        /***
         * Copy operation
         * @return new Address object
         */
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
        const uint16_t get_port();

        /***
         * Destroy the object
         */
        ~Address();

    protected:
        std::shared_ptr<AddressImpl> imp_;
        Address(const std::shared_ptr<AddressImpl>& impl);
        Address(const std::string& address);
        Address();
    };

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::AddressError>
            : public true_type {};
}