//
// Created by denn on 2019-02-03.
//

#include "capy_address.hpp"

#include <string>
#include <iostream>
#include <cctype>
#include <memory>
#include <assert.h>
#include <system_error>

#include "amqpcpp.h"

namespace capy::amqp {

    using namespace std;


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

    capy::Result<Address> Address::From(const std::string &address) {

      if (address.empty()) {
        return capy::make_unexpected(capy::Error(AddressError::EMPTY));
      }

      try{
        return Address(address);
      }
      catch (const std::exception &exception) {
        return capy::make_unexpected(capy::Error(AddressError::PARSE,exception.what()));
      }
      catch (...) {
        return capy::make_unexpected(capy::Error(CommonError::UNKNOWN_ERROR));
      }

    }

    const std::string& Address::get_host() const {
      return imp_->amqp_address_->hostname();
    }

    const uint16_t Address::get_port() {
      return imp_->amqp_address_->port();
    }

    const Address::protocol Address::get_protocol() const {
      return  Address::protocol::amqp;
    }

    std::string AddressErrorCategory::message(int ev) const {
      switch (ev) {
        case static_cast<int>(AddressError::PARSE):
          return "Parse error";
        case static_cast<int>(AddressError::EMPTY):
          return "Empty input";
        default:
          return ErrorCategory::message(ev);
      }
    }

    const std::error_category& address_error_category()
    {
      static AddressErrorCategory instance;
      return instance;
    }

    std::error_condition make_error_condition(capy::amqp::AddressError e)
    {
      return std::error_condition(
              static_cast<int>(e),
              capy::amqp::address_error_category());
    }
}