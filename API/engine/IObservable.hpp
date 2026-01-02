#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>


class IObservable{

    public:
    using Callback = std::function<void()>;
    virtual int Subscribe(const Callback& cb, int priority= 0) = 0;
    virtual void Unsubscribe(int id) = 0;
    virtual void Notify() = 0;
};