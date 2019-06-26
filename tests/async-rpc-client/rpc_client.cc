//
// Created by denn nevera on 2019-06-23.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"

TEST(Exchange, AsyncFetchTest) {
  auto address = capy::amqp::Address::From("amqp://guest:guest@localhost:5672/");

  EXPECT_TRUE(address);

  if (!address) {
    std::cerr << "amqp address error: " << address.error().value() << " / " << address.error().message()
              << std::endl;
    return;
  }


  capy::Result<capy::amqp::Broker> broker = capy::amqp::Broker::Bind(*address); //capy::amqp::BrokerImpl(*address, "capy-test");

  EXPECT_TRUE(broker);

  if (!broker) {
    std::cerr << "amqp broker error: " << broker.error().value() << " / " << broker.error().message()
              << std::endl;
    return;
  }

  int max_count = 300000;

  for (int i = 0; i < max_count ; ++i) {

    std::string timestamp = std::to_string(time(0));

    capy::json action;
    action["action"] = "echo";
    action["payload"] = {{"ids", timestamp}, {"timestamp", timestamp}, {"i", i}};

    //std::cout << "fetch[" << i << "] action: " <<  action.dump(4) << std::endl;

    std::string key = "echo.ping";

    if (auto error = broker->fetch(action, key, [&](const capy::Result<capy::json> &message){

        if (!message){

          std::cerr << "amqp broker fetch receiving error: " << message.error().value() << " / " << message.error().message()
                    << std::endl;

        }
        else {
          std::cout << "fetch["<< i << "] received: " <<  message->dump(4) << std::endl;
        }


    })) {

      std::cerr << "amqp broker fetch error: " << error.value() << " / " << error.message()
                << std::endl;

    }

    if (max_count - 1 == i) {
      ::exit(0);
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(1));

  }

}