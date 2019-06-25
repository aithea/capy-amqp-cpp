//
// Created by denn nevera on 2019-06-24.
//

#include "capy/amqp.h"
#include "gtest/gtest.h"

TEST(Promise, PromiseTest) {

  capy::event::Promise<int> promise;

  std::cout << std::endl;

  promise.future().then([](int i){
      std::cout << "i = " << i << std::endl;
      return "foobar";
  }).then([](const std::string& str){
      std::cout << "str = " << str << std::endl;
  });

  promise.resolve(10);
}