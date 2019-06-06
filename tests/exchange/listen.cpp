//
// Created by denn nevera on 2019-06-06.
//


#include "exchange.hpp"

TEST(Exchange, ListenTest) {

  if (auto exchange = create_exchange()) {

    capy::json action = R"({
                                "action": "getAllByIds",
                                "payload": { "ids": [] }
                              })"_json;

  }
}