//
// Created by denn nevera on 2019-06-05.
//

#pragma once

#include "capy/amqp_common.h"
#include "capy/expected.h"
#include "capy/amqp_address.h"
#include "capy/amqp_broker.h"
#include "capy/amqp_deferred.h"
#include "capy/dispatchq.h"
#include "dotenv/dotenv.h"

namespace capy {
    /***
     * Read variable value from dotenv (.env) file
     * @param variable_name
     * @return
     */
    static inline Result<std::string> get_dotenv(const std::string& variable_name) {
      using namespace dotenv;
      try{
        return env[variable_name];
      }
      catch (std::out_of_range &e) {
        return capy::make_unexpected(Error(capy::amqp::CommonError::OUT_OF_RANGE, e.what()));
      }
      catch (std::exception &e) {
        return capy::make_unexpected(Error(capy::amqp::CommonError::NOT_FOUND, e.what()));
      }
      catch (...) {
        return capy::make_unexpected(Error(capy::amqp::CommonError::UNKNOWN));
      }
    }
}
