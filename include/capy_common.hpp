//
// Created by denn nevera on 2019-05-31.
//

#pragma once

#import <iostream>

namespace capy::amqp {

    class error_category: public std::error_category
    {
    public:
        error_category(const std::string &message);
        error_category():mess_(""){};
        const char* name() const noexcept override;
        std::string message(int ev) const override;

    private:
        std::string mess_;
    };

    typedef std::function<void(const std::error_code &code)> ErrorHandler;

    /**
     * Default error handler
     */

    static auto default_error_handler = [](const std::error_code &code) {
        std::cerr << "Capy::Error: code[" << code.value() << "] " << code.message() << std::endl;
    };

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