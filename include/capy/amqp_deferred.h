//
// Created by denn nevera on 2019-06-24.
//

#pragma once
#include "amqp_common.h"
#include "capy/dispatchq.h"

#include <functional>
#include <system_error>
#include <mutex>
#include <thread>
#include <atomic>
#include <optional>

namespace capy::amqp {

    class Channel;

    /***
     * Deferred handlers object
     * @tparam Types
     */
    template<class ... Types>
    class Deferred {
    public:

        using VoidHandler      = std::function<void()>;

        /***
         * Data handler prototype
         */
        using DataHandler      = std::function<void(Types... parameters)>;

        /***
         * Success handler prototype
         */
        using SuccessHandler   = VoidHandler;

        /***
         * Finalize handler prototype
         */
        using FinalizeHandler  = VoidHandler;

        /***
         * Create Deferred object with error state
         * @param error - error state
         */
        Deferred(const Error &error = Error(CommonError::OK)):
                error_(error) {}

        /***
         * Copy constructor
         * @param that
         */
        Deferred(const Deferred &that) = delete;

        /***
         * Replacer
         * @param that
         */
        Deferred(Deferred &&that) = delete;

        /***
         *
         * Check a statee of the deferred object
         * @return true or false
         */
        operator bool() const {
          return !failed_;
        }

        /**
         * @todo: prepare stack-like reporting
         */

        /***
         * Report data if they received
         * @param parameters
         * @return the object
         */
        const Deferred &report_data(Types... parameters) const {
          if (*this && data_handler_) data_handler_.value()(parameters...);
          return *this;
        }

        /***
         * Reports success if data receiving has done succesfuly
         * @return the object
         */
        const Deferred &report_success() {
          failed_ = false;
          if (success_handler_) success_handler_.value()();
          return *this;
        }

        /***
         * Report error if some error occured
         * @param error
         * @return the object
         */
        const Deferred &report_error(const Error &error) {
          error_ = error;
          if (error_) {
            failed_ = true;
            if (error_handler_) error_handler_.value()(error_);
            error_ = Error(CommonError::OK);
          }
          return *this;
        }

        /***
         * Deferred call on data event
         * @param callback
         * @return the object
         */
        Deferred &on_data(const DataHandler &callback) {
          data_handler_ = callback;
          return *this;
        }

        /***
         * Deferred call on success event
         * @param callback
         * @return the object
         */
        Deferred &on_success(const SuccessHandler &callback) {
          success_handler_ = callback;
          return *this;
        }

        /***
         * Error handler on error event
         * @param callback
         * @return the object
         */
        Deferred &on_error(const ErrorHandler &callback) {
          error_handler_ = callback;
          return *this;
        }

        /***
         * it calls in any case at the end
         * @param callback
         * @return the object
         */
        Deferred &on_finalize(const FinalizeHandler &callback) {
          finalize_handler_ = callback;
          return *this;
        }

        /**
        *  Destructor
        */
        virtual ~Deferred() {
          if (error_ && error_handler_) error_handler_.value()(error_);
          if (finalize_handler_) finalize_handler_.value()();
          reset();
        }

    protected:
        std::optional<DataHandler>     data_handler_     = std::nullopt;
        std::optional<SuccessHandler>  success_handler_  = std::nullopt;
        std::optional<ErrorHandler>    error_handler_    = std::nullopt;
        std::optional<FinalizeHandler> finalize_handler_ = std::nullopt;

    private:
        Error error_;
        bool failed_;

        void reset(){
          data_handler_.reset();
          success_handler_.reset();
          error_handler_.reset();
          finalize_handler_.reset();
        }
    };

    /***
    * Fetcher handling request
    */
    using DeferredFetch  = Deferred<const Response &>;

    /***
    * Listener handling action request and replies
    */
    using DeferredListen = Deferred<const Request &, Replay*>;
}