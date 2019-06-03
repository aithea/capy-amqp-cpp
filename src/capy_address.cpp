//
// Created by denn on 2019-02-03.
//

#include "capy_address.hpp"

#include <string>
#include <iostream>
#include <cctype>
#include <memory>
#include <assert.h>
#include "amqpcpp.h"

namespace capy::amqp {

    using namespace std;

    const std::string Address::default_host = "localhost";
    const uint16_t Address::default_port = 5672;

    class AddressImpl {

    public:

        AddressImpl(const std::string& address) {
            amqp_address_ = shared_ptr<AMQP::Address>(new AMQP::Address(address));
        }

      shared_ptr<AMQP::Address> amqp_address_;
    };

    Address::Address():imp_(nullptr){
      assert("Address::Address is protected. Use Address(\"amqp://login@host:port/vhost\") constructor");
    }

    Address::~Address() {}

    Address& Address::operator=(const capy::amqp::Address &address) {
      this->imp_ = address.imp_;
      return *this;
    }

    Address::Address(const capy::amqp::Address &address):imp_(address.imp_) {}

    Address::Address(const std::shared_ptr<capy::amqp::AddressImpl> &impl):imp_(impl) {}

    Address::Address(const std::string &address):imp_(new AddressImpl(address)) {}

    std::optional<Address> Address::Parse(const std::string &address, const capy::amqp::ErrorHandler &error_handler) {
      try {
        return  make_optional(Address(address));
      }
      catch (const std::exception &exception) {
        error_handler(std::error_code(
                AddressError ::PARSE,
                error_category(amqp::error_string(exception.what()))));
      }
      catch  (...) {
        error_handler(std::error_code(
                AddressError ::UNKNOWN_ERROR,
                error_category(amqp::error_string("Could not connect to server"))));
      }

      return nullopt;
    };

    const std::string& Address::get_host() const {
      return imp_->amqp_address_->hostname();
    }

    const uint16_t Address::get_port() {
      return imp_->amqp_address_->port();
    }

    const Address::protocol Address::get_protocol() const {
      return amqp;
    }

}