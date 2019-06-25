//
// Created by denn nevera on 2019-06-21.
//

#include "gtest/gtest.h"
#include "capy/amqp.h"
#include <uv.h>


#include <ctime>
#include <cstdlib>

void monitor(uv_timer_t *handle){
  std::cout << "monitor ping ... " << std::endl;
}

void main_loop() {

  /* Main thread will run default loop */
  uv_loop_t *main_loop = uv_default_loop();

  uv_timer_t timer_req;
  uv_timer_init(main_loop, &timer_req);
  uv_timer_start(&timer_req, monitor, 0, 2000);

  std::cout << "Starting main loop" << std::endl;
  uv_run(main_loop, UV_RUN_DEFAULT);
}


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
                replay.value() = {"reply", true, counter, r};
                counter++;

                std::cout << " listen replay: " << replay->dump(4)  << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(r));

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

  //main_loop();

}
