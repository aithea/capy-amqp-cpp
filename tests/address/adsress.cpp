//
// Created by denn nevera on 2019-06-03.
//

#include "capy_amqp.hpp"
#include "gtest/gtest.h"
#include <optional>

static auto error_handler = [](const std::error_code &code) {
    EXPECT_TRUE(false) << "Address::Test::Error:: code[" << code.value() << "] " << code.message() << std::endl;
};

TEST(AddressTest, BadAddress) {

  std::optional<capy::amqp::Address> bad_address
          = capy::amqp::Address::Parse("https:://somewhereelse.org:9777/",
                                       [](const std::error_code &code) {
                                           EXPECT_TRUE(false)
                                                         << "Bad Address::Test::Error: code[" << code.value() << "] "
                                                         << code.message() << std::endl;
                                       });

}

TEST(AddressTest, GoodAddress) {

  std::optional<capy::amqp::Address> good_address = capy::amqp::Address::Parse("amqps:://somewhereelse.org:9777/",
                                                                               error_handler);

}