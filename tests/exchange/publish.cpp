//
// Created by denn nevera on 2019-06-05.
//

#include "exchange.hpp"

TEST(Exchange, PublishTest) {

  if (auto exchange = create_exchange()) {

    capy::json action = R"({
                                "action": "getAllByIds",
                                "payload": { "ids": [] }
                              })"_json;

    if (auto error = exchange->publish(action, "documents.find")) {
      std::cerr << "amqp exchange publish error: " << error.value() << " / " << error.message()
                << std::endl;
    }
  }
}