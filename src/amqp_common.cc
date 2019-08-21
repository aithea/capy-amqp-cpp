//
// Created by denn nevera on 2019-06-03.
//

#include "capy/amqp_common.h"
#include <string>
#include <cstdarg>
#include <iostream>

namespace capy::amqp {

    using namespace std;

    Replay::~Replay() {
    }

    Replay::Replay():
    message()
    {}

    const char *ErrorCategory::name() const noexcept {
      return "capy.amqp";
    }

    bool ErrorCategory::equivalent(const std::error_code &code, int condition) const noexcept {
      return code.value() == condition;
    }

    std::string ErrorCategory::message(int ev) const {
      switch (ev) {

        case static_cast<int>(CommonError::OK):
          return "OK";

        case static_cast<int>(CommonError::NOT_SUPPORTED):
          return "Not supported format";

          default:
          return "Unknown error";
      }
    }

    const std::error_category& error_category()
    {
      static ErrorCategory instance;
      return instance;
    }

    std::error_condition make_error_condition(capy::amqp::CommonError::Type e)
    {
      return std::error_condition(
              static_cast<capy::amqp::CommonError::Type>(e),
              capy::amqp::error_category());
    }
}
