//
// Created by denn nevera on 2019-06-21.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"

#include <ctime>
#include <cstdlib>

#define CAPY_RPC_TEST_EMULATE_COMPUTATION 0

TEST(Exchange, AsyncListenTest) {

  srand(time(0));

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

  int counter = 0;

  broker->listen("capy-test", {"echo.ping"})

          .on_data([&counter](const capy::amqp::Request &request, capy::amqp::Replay &replay) {

              if (!request) {
                std::cerr << " listen error: " << request.error().value() << "/" << request.error().message() << std::endl;
              }
              else {


                auto r = (rand() % 1000) + 1;

                std::cout << " listen["<< counter << "] received ["<< request->routing_key << "]: " << request->message.dump(4) << std::endl;

                ///
                /// developer must process any exception inside the worker code
                ///
                try {
                  std::cout << "try get a field: " << request->message.at("some").get<std::string>() << std::endl;
                }

                catch (std::exception& e) {
                  std::cerr << "amqp worker error: " << e.what() << std::endl;
                  return;
                }


                if (counter%4) {

                  replay = capy::make_unexpected(capy::Error(capy::amqp::BrokerError::DATA_RESPONSE,"some error"));

                } else{

                  replay.value() = {"reply", true, counter, r};

                }

                if (!replay) {
                  std::cout << " listen replay: " << replay.error().message() << std::endl;
                }
                else {
                  std::cout << " listen replay: " << replay.value_or(capy::json({"is empty"})).dump(4) << std::endl;
                }

#if CAPY_RPC_TEST_EMULATE_COMPUTATION == 1
                std::this_thread::sleep_for(std::chrono::milliseconds(r));
#endif
                counter++;

              }

          })

          .on_success([] {

              std::cout << "Deferred: on_success... " << std::endl;

          })

          .on_error([](const capy::Error &error) {

              std::cout << "Deferred: on_error: " << error.message() << std::endl;

          });


  std::cout << "Start main thread loop " << std::endl;

  capy::dispatchq::main::loop::run();


}
