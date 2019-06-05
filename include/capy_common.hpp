//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#include <iostream>
#include <exception>
#include <optional>

#include "capy_expected.hpp"

#define PUBLIC_ENUM(OriginalType) std::underlying_type_t<OriginalType>
#define EXTEND_ENUM(OriginalType,LAST) static_cast<std::underlying_type_t<OriginalType>>(OriginalType::LAST)

namespace capy {

    using namespace nonstd;

    /***
     * Common Error handler
     */
    struct Error {
        Error(const std::error_condition code, const std::optional<std::string>& message = std::nullopt);
        const int value() const;
        const std::string message() const;
    private:
        std::error_code code_;
        std::optional<std::string>  exception_message_;
    };

    /***
     * Synchronous expected result type
     */
    template <typename T>
    using Result = expected<T,Error>;

    /***
    *
    * Formated error string
    *
    * @param format
    * @param ...
    * @return
    */
    const std::string error_string(const char* format, ...);
}

namespace capy::amqp {

    /***
     * Common error codes
     */
    enum class CommonError: int {
        /***
         * not supported error
         */
        NOT_SUPPORTED = 300,
        /***
         * unknown error
         */
        UNKNOWN_ERROR = 3001,
        /**
         * the last error code
         */
        LAST
    };

    /***
     * Common Error category
     */
    class ErrorCategory: public std::error_category
    {
    public:
        virtual const char* name() const noexcept override ;
        virtual std::string message(int ev) const override ;
        virtual bool equivalent(const std::error_code& code, int condition) const noexcept override ;

    };

    const std::error_category& error_category();
    std::error_condition make_error_condition(capy::amqp::CommonError e);

}

namespace std {

    template <>
    struct is_error_condition_enum<capy::amqp::CommonError>
            : public true_type {};
}