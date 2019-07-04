//
// Created by denn nevera on 2019-07-04.
//

#include "deferred.h"

namespace capy::amqp {

    DeferredConections::DeferredConections(ConnectionCache* connections):
            connections_(connections),
            channel_(std::unique_ptr<Channel>(connections_->new_channel()))
    {

    }

    Channel& DeferredConections::get_channel() const {
      return *channel_;
    }

}
