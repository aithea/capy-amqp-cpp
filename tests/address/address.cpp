//
// Created by denn nevera on 2019-06-03.
//

#include "capy/amqp.hpp"
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

TEST(AddressTestResult, BaddAddressLogin) {

  auto bad_address = capy::amqp::Address::From("amqp://admin@somewhereelse.org/vhost");

  EXPECT_FALSE(bad_address);

  if (!bad_address) {
    std::cout << "amqp bad address login error: " << bad_address.error().value() << " / " << bad_address.error().message() << std::endl;
  }
}

TEST(AddressTest, GooddAdress) {

  auto good_address = capy::amqp::Address::From("amqp://admin:password@somewhereelse.org:1111/vhost");

  EXPECT_TRUE(good_address);

  if (!good_address) {
    std::cout << "amqp bad address error: " << good_address.error().value() << " / " << good_address.error().message()
              << std::endl;
  }
  else {
    ASSERT_EQ(good_address->get_protocol(),capy::amqp::Address::Protocol::amqp);
    ASSERT_EQ(good_address->get_hostname(),"somewhereelse.org");
    ASSERT_EQ(good_address->get_port(),1111);
    ASSERT_EQ(good_address->get_vhost(),"/vhost");
    ASSERT_EQ(good_address->get_login().get_username(),"admin");
    ASSERT_EQ(good_address->get_login().get_password(),"password");
  }
}