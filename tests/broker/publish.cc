//
// Created by denn nevera on 2019-06-05.
//

#include "broker_constructor.h"

TEST(Broker, PublishTest) {

  if (auto exchange = create_broker()) {

    capy::json action = R"({
                                "action": "getAllByIds",
                                "payload": { "ids": "" }
                              })"_json;

    if (auto error = exchange->publish(action, "documents.find")) {
      std::cerr << "amqp broker publish error: " << error.value() << " / " << error.message()
                << std::endl;
    }
  }
}