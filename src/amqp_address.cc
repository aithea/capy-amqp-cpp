//
// Created by denn on 2019-02-03.
//

#include "capy/amqp_address.h"

namespace capy::amqp {

    Address::Address(const url::Parts& address):CommonAddress(address){};

    capy::Result<Address> Address::From(const std::string &address) {

      auto parts = capy::url::parse(
              address,
              "(amqp?|amqps)://(\\w+:{0,1}\\w*@)?([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)",
              {5672, "guest","guest"}
      );

      if (!parts) return capy::make_unexpected(parts.error());

      auto proto = std::get<0>(*parts);

      transform(proto.begin(), proto.end(), proto.begin(), ::toupper);

      auto protocol = Address::Protocol::unknown;

      if ( proto == "AMQP") {
        protocol = Address::Protocol::amqp;
      }
      else if (proto == "AMQPS") {
        protocol = Address::Protocol::amqps;
      }
      else {
        return capy::make_unexpected(capy::Error(UrlErrorValue::PARSE,
                                                 error_string("Url Protocol %s is not supported yet", proto.c_str())));
      }

      auto a = Address(*parts);

      a.protocol_.name = a.get_schema_name();
      a.protocol_.type = protocol;
      a.vhost_ = std::get<4>(*parts);;

      return a;
    }

}