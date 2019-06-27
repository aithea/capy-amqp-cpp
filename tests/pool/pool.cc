//
// Created by denn nevera on 2019-06-27.
//

#include "capy/amqp.h"
#include "gtest/gtest.h"
#include "../../src/impl/amqp_pool_impl.h"

struct TestObject {
    TestObject(int anI ): i(anI){}
    TestObject(const TestObject& object) = default;
    TestObject(TestObject&& object) = default;

    ~TestObject() {
      std::cout << " ~TestObject("<< i <<") " << std::endl;
    }

    int i;

};

TEST(Pool, PoolTest) {

  std::cerr << std::endl;
  std::cout << std::endl;

  size_t size = 8;
  size_t count = 1000;

  srand(time(0));

  auto queue = capy::dispatchq::Queue(size);
  auto next_queue = capy::dispatchq::Queue(size);

  capy::Pool<TestObject> pool(size, [](size_t index){
      return new TestObject(index);
  });

  capy::dispatchq::main::async([&pool, count, &queue] {
      for (size_t i = 0; i < count; i++) {
        auto r = (rand() % 1000) + 1;

        queue.async([&pool,r] {
            auto object = pool.acquire();

            std::this_thread::sleep_for(std::chrono::microseconds(r));

            std::cout << "background thread     id: " << std::this_thread::get_id() << std::endl;
            std::cout << "background polled object: " << object->i << " available: " << pool.get_available()
                      << std::endl;
            pool.release(object);
        });
      }
  });

  for(size_t i = 0; i < count; i++){
    auto r = (rand() % 1000) + 1;

    next_queue.async([&pool, r, count, i]{

        auto object = pool.acquire();

        std::this_thread::sleep_for(std::chrono::microseconds(r));

        std::cout << ".... next thread     id: " << std::this_thread::get_id() << std::endl;
        std::cout << ".... next polled object: " << object->i << " available: " << pool.get_available() << std::endl;
        pool.release(object);

        if (i == count - 1) {
          capy::dispatchq::main::loop::exit();
        }

    });

    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }

  capy::dispatchq::main::loop::run();

  std::cout << "... exited" << std::endl;

}