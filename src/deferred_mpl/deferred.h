//
// Created by denn nevera on 2019-07-04.
//

#pragma once

#include "capy/amqp_deferred.h"
#include "../broker_impl/broker.h"

namespace capy::amqp {

    /***
     * TODO: implement channels pool
     */
    class DeferredConections {

    public:

        DeferredConections(ConnectionCache* connections);
        Channel& get_channel() const;

    protected:
        ConnectionCache* connections_;

    private:
        std::unique_ptr<Channel> channel_;
    };


    class DeferredFetching: public DeferredFetch, public DeferredConections {
    public:
        using DeferredFetch::DeferredFetch;

        DeferredFetching(ConnectionCache* connections, const Error &error = Error(CommonError::OK)):
                DeferredFetch(error), DeferredConections(connections)  {}

    };

    class DeferredListening: public DeferredListen, public DeferredConections  {
    public:
        using DeferredListen::DeferredListen;

        DeferredListening(ConnectionCache* connections,  const Error &error = Error(CommonError::OK)):
                DeferredListen(error), DeferredConections(connections)
        {

        }
    };
}