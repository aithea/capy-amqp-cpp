//
// Created by denn nevera on 2019-06-03.
//

#include "capy_amqp.hpp"
#include "gtest/gtest.h"
#include <optional>
#include <memory>

TEST(AddressTestResult, BaddAddress) {

  auto bad_address = capy::amqp::Address::From("https:://result@somewhereelse.org/");

  EXPECT_FALSE(bad_address);

  if (!bad_address) {
    std::cout << "amqp bad address error: " << bad_address.error().value() << " / " << bad_address.error().message() << std::endl;
  }
}

TEST(AddressTest, GooddAdress) {

  auto good_address = capy::amqp::Address::From("amqp://guest:guest@somewhereelse.org/vhost");

  EXPECT_TRUE(good_address);

  if (!good_address) {
    std::cout << "amqp bad address error: " << good_address.error().value() << " / " << good_address.error().message()
              << std::endl;
  }
  else {
    std::cout << "amqp protocol : " << good_address->get_protocol() << std::endl;
    std::cout << "amqp host     : " << good_address->get_host() << std::endl;
    std::cout << "amqp port     : " << good_address->get_port() << std::endl;
  }
}