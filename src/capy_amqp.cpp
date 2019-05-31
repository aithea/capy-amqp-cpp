//
// Created by denn nevera on 2019-05-31.
//

#include "capy_amqp.h"

namespace capy::amqp {

    optional<Exchange> Exchange::Bind(const capy::amqp::Url &url, const std::string &exchange_name) {
      if (true){
        return make_optional(Exchange(url,exchange_name));
      }
      return nullopt;
    }

    Exchange::Exchange(const capy::amqp::Url &url, const std::string &exchange_name) {

    }

    void Exchange::fetch(const capy::amqp::json &message, const std::vector<std::string> &keys) {

    }

    void Exchange::listen(const std::string &queue_name, const capy::amqp::MessageHandler &on_data) {
      on_data(json());
    }

    void Exchange::publish(const capy::amqp::json &message, const std::string &queue_name) {

    }

}
