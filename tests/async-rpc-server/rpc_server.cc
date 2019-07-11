//
// Created by denn nevera on 2019-06-21.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"

#include <ctime>
#include <cstdlib>

#define CAPY_RPC_TEST_EMULATE_COMPUTATION 0
#define CAPY_RPC_TEST_EMULATE_ERROR 0

TEST(Exchange, AsyncListenTest) {

  srand(time(0));

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

  int counter = 0;

  int error_state = static_cast<int>(capy::amqp::CommonError::OK);

  do {

    std::cout << " ... error_state: " << error_state << std::endl;

    capy::Result<capy::amqp::Broker> broker = capy::amqp::Broker::Bind(*address, [](const capy::Error& error){
        std::cerr << "amqp broker error: " << error.value() << " / " << error.message()
                  << std::endl;
    });

    EXPECT_TRUE(broker);

    if (!broker) {
      std::cerr << "amqp broker error: " << broker.error().value() << " / " << broker.error().message()
                << std::endl;
      return;
    }

    std::promise<int> error_state_connection;

    broker->listen("capy-test", {"echo.ping"})

            .on_data([&counter](const capy::amqp::Request &request, capy::amqp::Replay* replay) {

                replay->on_complete([](capy::amqp::Replay* replay){
                    std::cout << " ... complete replay ... " << std::endl;
                });

                if (!request) {
                  std::cerr << " listen error: " << request.error().value() << "/" << request.error().message()
                            << std::endl;
                } else {


                  auto r = (rand() % 100) + 1;

                  std::cout << " listen[" << counter << "] received [" << request->routing_key << "]: "
                            << request->message.dump(4) << std::endl;

#if CAPY_RPC_TEST_EMULATE_ERROR == 1
                  ///
                  /// developer must process any exception inside the worker code
                  ///
                  try {
                    std::cout << "try get a field: " << request->message.at("some").get<std::string>() << std::endl;
                  }

                  catch (std::exception& e) {
                    std::cerr << "amqp worker error: " << e.what() << std::endl;
                  }
#endif

                  if (counter % 2 == 0) {

                    replay->message = capy::make_unexpected(capy::Error(
                            capy::amqp::BrokerError::DATA_RESPONSE,
                            capy::error_string("some error %i", counter)));

                  } else {

                    replay->message.value() = {"reply", true, counter, r};

                  }

                  if (!replay) {
                    std::cout << " listen replay: " << replay->message.error().message() << std::endl;
                  } else {
                    std::cout << " listen replay: " << replay->message.value_or(capy::json({"is empty"})).dump(4) << std::endl;
                  }

#if CAPY_RPC_TEST_EMULATE_COMPUTATION == 1
                  std::this_thread::sleep_for(std::chrono::milliseconds(r));
#endif
                  counter++;

                }

                replay->commit();
            })

            .on_success([] {

                std::cout << "Deferred: on_success... " << std::endl;

            })

            .on_error([&error_state_connection](const capy::Error &error) {

                std::cout << "Deferred: on_error: " << error.value()
                          << "(" << static_cast<int>(capy::amqp::BrokerError::CONNECTION_LOST) << ")/"
                          << error.message() << std::endl;

                try {
                  if (error.value() == static_cast<int>(capy::amqp::BrokerError::CONNECTION_LOST))
                    error_state_connection.set_value(static_cast<int>(error.value()));
                }catch (...){}

            });

    broker->run();

    error_state = error_state_connection.get_future().get();

    std::cout << " ... error_state connection: " << error_state << std::endl;

  } while (error_state != static_cast<int>(capy::amqp::CommonError::OK));


  std::cout << "Start main thread loop " << std::endl;

  capy::dispatchq::main::loop::run();

}
