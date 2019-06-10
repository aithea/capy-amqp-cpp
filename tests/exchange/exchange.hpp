//
// Created by denn nevera on 2019-06-06.
//

#pragma once

#include "gtest/gtest.h"
#include "capy_amqp.hpp"

#include <optional>
#include <cstdlib>

static inline std::optional<capy::amqp::Exchange> create_exchange() {

  auto login = capy::get_dotenv("CAPY_AMQP_ADDRESS");

  EXPECT_TRUE(login);

  if (!login) {
    std::cerr << "CAPY_AMQP_ADDRESS: " << login.error().message() << std::endl;
    return std::nullopt;
  }

  auto address = capy::amqp::Address::From(*login);

  EXPECT_TRUE(address);

  if (!address) {
    std::cerr << "amqp address error: " << address.error().value() << " / " << address.error().message()
              << std::endl;
    return std::nullopt;
  }

  auto exchange = capy::amqp::Exchange::Bind(address.value());

  EXPECT_TRUE(exchange);

  if (!exchange) {
    std::cerr << "amqp exchange error: " << exchange.error().value() << " / " << exchange.error().message()
              << std::endl;
    return std::nullopt;
  }

  return  exchange.value();
}