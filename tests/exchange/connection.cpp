//
// Created by denn nevera on 2019-06-05.
//

#include "capy_amqp.h"
#include "gtest/gtest.h"
#include "amqp.h"

TEST(Exchange, ConnectionTest) {

  auto address = capy::amqp::Address::From("amqp://test:qaz789qaz789@rabbitmq-arxiv.aithea.com/");

  EXPECT_TRUE(address);

  if (!address) {
    std::cerr << "amqp address error: " << address.error().value() << " / " << address.error().message()
              << std::endl;
    return;
  }

  auto exchange = capy::amqp::Exchange::Bind(address.value());

  EXPECT_TRUE(exchange);

  if (!exchange) {
    std::cerr << "amqp exchange error: " << exchange.error().value() << " / " << exchange.error().message()
              << std::endl;
    return;
  }

  capy::amqp::json action = R"({
                                "action": "getAllByIds",
                                "payload": { "ids": [] }
                              })"_json;

  exchange.value().publish(action,"documents.find");
}