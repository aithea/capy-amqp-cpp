//
// Created by denn nevera on 2019-06-06.
//

#pragma once

#include "gtest/gtest.h"
#include "capy/amqp.h"

#include <optional>
#include <cstdlib>

static inline std::optional<capy::amqp::Broker> create_broker() {

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

  auto broker = capy::amqp::Broker::Bind(address.value());

  EXPECT_TRUE(broker);

  if (!broker) {
    std::cerr << "amqp broker error: " << broker.error().value() << " / " << broker.error().message()
              << std::endl;
    return std::nullopt;
  }

  return  broker.value();
}