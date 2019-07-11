//
// Created by denn nevera on 2019-06-25.
//

#include "capy/amqp.h"
#include "../../src/broker_impl/broker.h"
#include "gtest/gtest.h"


capy::amqp::DeferredListen& simulate_deferred() {

  auto handler = std::make_shared<capy::amqp::DeferredListen>();

  capy::dispatchq::main::async([handler]{
      // lock on_data

      handler->report_error(capy::Error(capy::amqp::CommonError::UNKNOWN, "Something error occurred"));

      capy::amqp::Request request(capy::amqp::Rpc("key", {"result", true}));
      capy::amqp::Replay *replay = new capy::amqp::ReplayImpl();

      handler->report_data(request, replay);
  });

  capy::dispatchq::main::async([handler]{
      // unlock
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

      handler->report_success();
  });

  capy::dispatchq::main::async([handler]{

      std::this_thread::sleep_for(std::chrono::milliseconds(2));

      capy::amqp::Request request(capy::amqp::Rpc("key", {"result", true}));
      capy::amqp::Replay *replay = new capy::amqp::ReplayImpl();

      handler->report_data(request, replay);

      std::cout << "report_data: " << replay->message->dump(4) << std::endl;
  });


  return *handler;
}


TEST(Deferred, DeferredTest) {

  std::cout << std::endl;

  simulate_deferred()

          .on_data([](const capy::amqp::Request& request, capy::amqp::Replay* replay){
              std::cout << "Deferred: on_data: " << request->message.dump(4) << std::endl;
              capy::json json = {"replay", "payload..."};
              replay->message = json;
              replay->commit();
          })

          .on_success([]{

              std::cout << "Deferred: on_success... " << std::endl;

          })

          .on_error([](const capy::Error& error){

              std::cout << "Deferred: on_error: " << error.message() << std::endl;

          })

          .on_finalize([]{

              std::cout << "Deferred: on_finalize... " << std::endl;

              std::this_thread::sleep_for(std::chrono::milliseconds(10));
              capy::dispatchq::main::loop::exit();

          });

  capy::dispatchq::main::loop::run();

}