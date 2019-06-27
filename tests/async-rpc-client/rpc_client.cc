//
// Created by denn nevera on 2019-06-23.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"

#define CAPY_RPC_TEST_EMULATE_COMPUTATION 0

TEST(Exchange, AsyncFetchTest) {
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

  int max_count = 100;

  for (int i = 0; i < max_count ; ++i) {

    std::string timestamp = std::to_string(time(0));

    capy::json action;
    action["action"] = "echo";
    action["payload"] = {{"ids", timestamp}, {"timestamp", timestamp}, {"i", i}};

    std::string key = "echo.ping";

    std::cout << " fetch["<<i<<"]: " << key << std::endl;

    broker->fetch(action, key)

            .on_data([i, max_count](const capy::amqp::Response &response){
              if (response){
                std::cout << "fetch["<< i << "] received: " <<  response->dump(4) << std::endl;
              }
              else {
                std::cerr << "amqp broker fetch data error: " << response.error().value() << " / " << response.error().message()
                          << std::endl;
              }


                if (max_count - 1 == i) {
                  std::cout << " ... exiting ... " << std::endl;
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  capy::dispatchq::main::loop::exit();
                }

            })

            .on_error([i,max_count](const capy::Error& error){
                std::cerr << "amqp broker fetch receiving error: " << error.value() << " / " << error.message()
                          << std::endl;

                if (max_count - 1 == i) {
                  std::cout << " ... exiting ... " << std::endl;
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  capy::dispatchq::main::loop::exit();
                }
            });

#if CAPY_RPC_TEST_EMULATE_COMPUTATION == 1
    auto r = (rand() % 10) + 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(r));
#endif

  }

  capy::dispatchq::main::loop::run();
}