//
// Created by denn nevera on 2019-06-03.
//

#include "capy_amqp.hpp"
#include "gtest/gtest.h"
#include <optional>
#include <memory>

TEST(AddressTest, BadAddress) {

  /***
   * Example of user error handler redefenition
   */
  std::optional<capy::amqp::Address> bad_address
          = capy::amqp::Address::Parse("https:://somewhereelse.org:9777/",
                                       [](const std::error_code &code) {
                                           EXPECT_TRUE(true)
                                                         << "Bad Address::Test::Error: code[" << code.value() << "] "
                                                         << code.message() << std::endl;
                                       });

}

/***
 * Default handler for this file
 */
static auto error_handler = [](const std::error_code &code) {
    EXPECT_TRUE(false) << "Address::Test::Error:: code[" << code.value() << "] " << code.message() << std::endl;
};

TEST(AddressTest, GoodAddress) {

  /***
   * Example of usage local error handler defenition
   */
  std::optional<capy::amqp::Address> good_address = capy::amqp::Address::Parse("amqp://guest:guest@somewhereelse.org/vhost",
                                                                               error_handler);

  std::cout << "amqp protocol : " << good_address->get_protocol() << std::endl;
  std::cout << "amqp host     : " << good_address->get_host() << std::endl;
  std::cout << "amqp port     : " << good_address->get_port() << std::endl;

}