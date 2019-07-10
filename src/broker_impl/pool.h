//
// Created by denn nevera on 2019-06-27.
//

#pragma once

#include <functional>
#include <thread>
#include <vector>
#include <optional>
#include <condition_variable>

/***
 * TODO: pool channeling... 
 */

namespace capy {

    template<typename T> class Pool;

    template<typename T>
    class Node
    {
        friend class Pool<T>;

    public:
        explicit Node() : m_val() { }

        template<class... Args>
        void construct(Args&&... args)
        {
          new (&m_val) T(std::forward<Args>(args)...);
        }

        T& operator=(const T& val)
        {
          m_val = val;
          return m_val;
        }

        T& operator*() { return m_val; }
        const T& operator*() const { return m_val; }

        T& val() { return m_val; }
        const T& val() const { return m_val; }

        T* operator->() { return &m_val; }
        const T* operator->() const { return &m_val; }

    private:

        T m_val;
    };

    template<typename O>
    class Pool final {

        using ObjectFactoryHandler  = std::function<O*(size_t index)>;

    public:
        Pool(size_t size, ObjectFactoryHandler factory):size_(size),factory_(factory),mutex_(),available_objects_(){
          fill();
        }

        O* acquire() {
          std::unique_lock lock(mutex_);
          while (available_objects_.empty()){
            wait_for_release_.wait(lock);
          }
          auto last = std::move(available_objects_.back());
          available_objects_.pop_back();
          return last;
        }

        void release(O* object) {
          std::unique_lock lock(mutex_);
          available_objects_.insert(available_objects_.begin(),object);
          wait_for_release_.notify_all();
        }

        size_t get_size() const { return size_; };

        size_t get_available() const {
          std::lock_guard lock(mutex_);
          return available_objects_.size();
        };

        bool empty() const {
          std::lock_guard lock(mutex_);
          return  available_objects_.empty();
        }

        ~Pool(){
         for (auto o: available_objects_){
           delete o;
         }
        }

    private:
        size_t size_;
        ObjectFactoryHandler factory_;
        mutable std::mutex mutex_;
        std::vector<O*> available_objects_;
        std::condition_variable wait_for_release_;

    private:
        void fill(){
          std::lock_guard lock(mutex_);
          for (size_t i = 0; i < size_; ++i) {
            available_objects_.push_back(factory_(i));
          }
        }
    };
}