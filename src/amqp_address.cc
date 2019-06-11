//
// Created by denn on 2019-02-03.
//

#include "capy/amqp_address.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cctype>
#include <assert.h>
#include <system_error>
#include <regex>
#include <locale>         // std::locale, std::toupper
#include <algorithm>
#include <memory>
#include <vector>

namespace capy::amqp {

    using namespace std;

    ///
    /// Login private implementation
    ///
    class LoginImp: public Login {
    public:
        string user;
        string password;

        virtual const std::string& get_username() const override {
          return user;
        };
        virtual const std::string& get_password() const override {
          return password;
        };
    };

    ///
    /// Address private implementation
    ///
    struct AddressImpl {

        Address::Protocol protocol;
        LoginImp  login;
        string hostname;
        string vhost;
        uint16_t port;


        AddressImpl() = default;
    };

    /// Utils
    std::vector<std::string> split(const std::string& s, char delimiter)
    {
      std::vector<std::string> tokens;
      std::string token;
      std::istringstream tokenStream(s);
      while (std::getline(tokenStream, token, delimiter))
      {
        tokens.push_back(token);
      }
      return tokens;
    }

    ///
    /// Interface
    ///

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

    capy::Result<Address> Address::From(const std::string &address) {

      if (address.empty()) {
        return capy::make_unexpected(capy::Error(AddressError::EMPTY));
      }

      try{

        regex ex("(amqp?|amqps)://(\\w+:{0,1}\\w*@)?([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
        cmatch what;

        if (regex_match(address.c_str(), what, ex)) {

          string proto = std::string(what[1].first, what[1].second);

          transform(proto.begin(), proto.end(), proto.begin(), ::toupper);

          Address::Protocol aProtocol = Address::Protocol::unknown;

          if ( proto == "AMQP") {
            aProtocol = Address::Protocol::amqp;
          }
          else if (proto == "AMQPS") {
            aProtocol = Address::Protocol::amqps;
          }
          else {
            return capy::make_unexpected(capy::Error(AddressError::PARSE,
                    error_string("Url Protocol %s is not supported yet", proto.c_str())));
          }

          auto imp = shared_ptr<AddressImpl>(new AddressImpl);

          imp->protocol = aProtocol;

          imp->hostname = string(what[3].first, what[3].second);
          string port = string(what[4].first, what[4].second);

          if (port.empty()){

            imp->port = 5672;

          }

          else {

            int port_num = std::stoi(port, nullptr, 0);

            if (port_num>=0 && port_num <= 65535) {

              imp->port = (uint16_t) port_num;

            } else {

              return capy::make_unexpected(capy::Error(AddressError::PARSE,
                                                       error_string("Address port number %s is out of range", port.c_str())));
            }
          }

          imp->vhost = string(what[5].first, what[5].second);

          string login = string(what[2].first, what[2].second);

          if (login.empty()){
            imp->login.user = "guest";
            imp->login.password = "guest";
          }
          else {

            login.pop_back();

            vector<string> login_vector = split(login, ':');

            if (login_vector.size()<2){
              return capy::make_unexpected(capy::Error(AddressError::PARSE,
                                                       error_string("LoginImp %s is not correct", port.c_str())));
            }

            imp->login.user = login_vector[0];
            imp->login.password = login_vector[1];

          }

          return Address(std::move(imp));

        }
        else {
          return capy::make_unexpected(capy::Error(AddressError::PARSE));
        }
      }
      catch (const std::exception &exception) {
        return capy::make_unexpected(capy::Error(AddressError::PARSE,exception.what()));
      }
      catch (...) {
        return capy::make_unexpected(capy::Error(CommonError::UNKNOWN));
      }

    }

    const std::string& Address::get_hostname() const {
      return imp_->hostname;
    }

    const uint16_t Address::get_port() const {
      return imp_->port;
    }

    const std::string& Address::get_vhost() const {
      return  imp_->vhost;
    }

    const Address::Protocol Address::get_protocol() const {
      return  Address::Protocol::amqp;
    }

    const Login& Address::get_login() const {
      return imp_->login;
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