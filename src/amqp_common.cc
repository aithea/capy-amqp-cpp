//
// Created by denn nevera on 2019-06-03.
//

#include "capy/amqp_common.h"
#include <string>
#include <cstdarg>
#include <iostream>

namespace capy {

    Error::Error(const std::error_condition code, const std::optional<std::string> &message):
    code_(code.value(),code.category()), exception_message_(message)
    {
    }

   const int Error::value() const {
     return code_.value();
    }

    const std::string Error::message() const {
      if (exception_message_){
        return *exception_message_;
      }
      return code_.message();
    }

    Error::operator bool() const {
      return (code_.value() != static_cast<int>(capy::amqp::CommonError ::OK));
    }

    const std::string error_string(const char* format, ...)
    {
      char buffer[1024] = {};
      va_list ap = {};

      va_start(ap, format);
      vsnprintf(buffer, sizeof(buffer), format, ap);
      va_end(ap);

      return "Cappy error: " + std::string(buffer);
    }

}

namespace capy::amqp {

    using namespace std;

    Replay::~Replay() {
      std::cout << " *** ~Replay " << (this->message.has_value() ? this->message.value().dump() : this->message.error().message()) << std::endl;
      //if (!this->has_value())
      //  commit();
    }

    //Replay::Replay(const std::optional<capy::amqp::Replay::CommitHandler> &commit_handler):
    //ReplayType(),
    Replay::Replay():
    commit_handler_(nullptr)
    {}

    void Replay::set_commit(const std::optional<capy::amqp::Replay::CommitHandler> &commit_handler) {
      commit_handler_ = commit_handler;
    }

    void Replay::commit() {
      std::cout << " *** commit " << (this->message.has_value() ? this->message.value().dump() : this->message.error().message()) << std::endl;

      if (commit_handler_) {
        commit_handler_.value()(this);
      }
    }

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

    std::error_condition make_error_condition(capy::amqp::CommonError e)
    {
      return std::error_condition(
              static_cast<int>(e),
              capy::amqp::error_category());
    }
}
