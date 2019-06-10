//
// Created by denn nevera on 2019-06-06.
//


#include "broker_constructor.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

TEST(Exchange, ListenTest) {

  auto listener_q  = capy::dispatchq::Queue(1);
  auto publisher_q = capy::dispatchq::Queue(1);

  listener_q.async([] {

      std::cout << "listener thread" << std::endl;

      if (auto broker = create_broker()) {

        std::cout << "listener broker created ... " << std::endl;

        int counter = 0;
        broker->listen("capy-test", "something.find", [&](

                const capy::Result<capy::json>& message,
                capy::Result<capy::json>& replay){

            if (!message) {
              std::cerr << " listen error: " << message.error().value() << "/" << message.error().message() << std::endl;
            }
            else {
              std::cout << " listen["<< counter << "] received: " << message.value().dump(4) << std::endl;
              replay.value() = {"reply",true};
              counter++;
            }
        });

      };

  });


  publisher_q.async([]{

      std::cout << "producer thread" << std::endl;

      if (auto broker = create_broker()) {
        std::cout << "producer broker created ... " << std::endl;

        for (int i = 0; i < 3 ; ++i) {

          std::this_thread::sleep_for(std::chrono::seconds(1));

          std::string timestamp = std::to_string(time(0));

          capy::json action = {
                  {"action", "someMethodSouldBeExecuted"},
                  {"payload", {"ids", timestamp}, {"timestamp", timestamp}, {"i", i}}
          };

          std::cout << "fetch[" << i << "] action: " <<  action.dump(4) << std::endl;

          if (auto error = broker->fetch(action, "something.find", [&](const capy::Result<capy::json> &message){


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

        }

      }

  });

  std::cout << "main thread" << std::endl;

  capy::dispatchq::main::loop::run();

}