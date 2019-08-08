//
// Created by denn nevera on 2019-06-23.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"

#define CAPY_RPC_TEST_COUNT 1000
#define CAPY_RPC_TEST_EMULATE_COMPUTATION 0
#define CAPY_RPC_TEST_ASYNC 1

TEST(Exchange, AsyncFetchTest) {

  std::cout << std::endl;

  auto login = capy::get_dotenv("CAPY_AMQP_ADDRESS");

  EXPECT_TRUE(login);

  if (!login) {
    std::cerr << "CAPY_AMQP_ADDRESS: " << login.error().message() << std::endl;
    return;
  }

  auto address = capy::amqp::Address::From(*login);

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

  broker->run();

  int max_count = CAPY_RPC_TEST_COUNT;

  for (int i = 0; i < max_count ; ++i) {

    std::string timestamp = std::to_string(time(0));

#if CAPY_RPC_TEST_ASYNC == 1
    capy::dispatchq::main::async([&broker, timestamp, max_count, i](){
#endif

    //
    // async non-block fetching messages
    //

    capy::json action;
    action["action"] = "echo";
    action["payload"] = {{"ids", timestamp}, {"timestamp", timestamp}, {"i", i}};

    std::string key = "echo.ping";

    std::cout << " fetch["<<i<<"]: " << key << std::endl;

    broker->fetch(action, key)

            .on_data([i, max_count](const capy::amqp::Payload &response){

                if (response){
                  std::cout << "fetch["<< i << "] received: " <<  response->dump(4) << std::endl;
                }
                else {
                  std::cerr << "amqp broker fetch data error: " << response.error().value() << " / " << response.error().message()
                            << std::endl;
                }


                if (max_count - 1 == i) {
                  std::cout << " ... exiting ... " << std::endl;
                  ::exit(0);
                }

            })

            .on_error([i,max_count](const capy::Error& error){
                std::cerr << "amqp broker fetch receiving error: " << error.value() << " / " << error.message()
                          << std::endl;

                if (max_count - 1 == i) {
                  std::cout << " ... exiting ... " << std::endl;
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  ::exit(-1);
                }
            });

#if CAPY_RPC_TEST_ASYNC == 1
    });
#endif

#if CAPY_RPC_TEST_EMULATE_COMPUTATION == 1
    auto r = (rand() % 100) + 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(r));
#endif

  }

  capy::dispatchq::main::loop::run();
}