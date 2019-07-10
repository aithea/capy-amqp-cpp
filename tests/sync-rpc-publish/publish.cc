//
// Created by denn nevera on 2019-06-26.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"

TEST(Exchange, SyncPublishTest) {

  auto address = capy::amqp::Address::From("amqp://guest:guest@localhost:5672/");

  EXPECT_TRUE(address);

  if (!address) {
    std::cerr << "amqp address error: " << address.error().value() << " / " << address.error().message()
              << std::endl;
    return;
  }


  capy::Result<capy::amqp::Broker> broker = capy::amqp::Broker::Bind(*address);

  EXPECT_TRUE(broker);

  if (!broker) {
    std::cerr << "amqp broker error: " << broker.error().value() << " / " << broker.error().message()
              << std::endl;
    return;
  }

  for (int i = 0; i < 2 ; ++i) {

    std::string timestamp = std::to_string(time(0));

    capy::json action;
    action["action"] = "echo";
    action["payload"] = {{"ids",       timestamp},
                         {"timestamp", timestamp},
                         {"i",         i}, {"publish test...", 0}};

    std::string key = "echo.ping";
    if (auto error = broker->publish(action,key)) {
      std::cout << " message has not been  send with error: " << error.message() << std::endl;
    }
    else {
      std::cout << " message has been sent ... " << std::endl;
    }
  }
}
