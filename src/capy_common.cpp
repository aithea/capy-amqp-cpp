//
// Created by denn nevera on 2019-06-03.
//

#include "capy_common.hpp"
#include <string>

namespace capy {

    using namespace std;

    amqp::error_category::error_category(const std::string &message):mess_(message) {}

    const char *amqp::error_category::name() const noexcept {
      return "capy amqp error";
    }

    std::string amqp::error_category::message(int ev) const {
          return mess_.empty() ? std::generic_category().message(ev) : mess_;
    }

    const std::string amqp::error_string(const char* format, ...)
    {
      char buffer[1024] = {};
      va_list ap = {};

      va_start(ap, format);
      vsnprintf(buffer, sizeof(buffer), format, ap);
      va_end(ap);

      return "Cappy error: " + std::string(buffer);
    }

}