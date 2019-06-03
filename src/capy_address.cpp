//
// Created by denn on 2019-02-03.
//

#include "capy_address.hpp"
//#include "utils.hpp"

#include <string>
#include <iostream>
#include <cctype>
#include <memory>
#include "amqpcpp.h"

namespace capy::amqp {

    using namespace std;

    class AddressImpl: public Address {
    public:
        AddressImpl(const std::string& address):Address() {
            amqp_address_ = shared_ptr<AMQP::Address>(new AMQP::Address(address));
        }

        const protocol get_protocol() const {
          return amqp;
        }

        const std::string &get_host() const {
          return amqp_address_->hostname();
        }


        const std::uint16_t get_port() const {
          return amqp_address_->port();
        }

        Address& operator=(const Address& address) {
          AddressImpl *addr = (AddressImpl*)&address;
          amqp_address_ = addr->amqp_address_;
        }

    protected:
      shared_ptr<AMQP::Address> amqp_address_;
    };


    std::optional<Address> Address::Parse(const std::string &address, const capy::amqp::ErrorHandler &error_handler) {
      try {
        return make_optional(AddressImpl(address));
      }
      catch (const std::exception &exception) {
        error_handler(std::error_code(
                address_error ::PARSE,
                error_category(amqp::error_string(exception.what()))));
      }
      catch  (...) {
        error_handler(std::error_code(
                address_error ::UNKNOWN_ERROR,
                error_category(amqp::error_string("Could not connect to server"))));
      }

      return std::nullopt;
    };

    Address::Address(const capy::amqp::Address &) {}
    Address& Address::operator=(const capy::amqp::Address &) {}
}