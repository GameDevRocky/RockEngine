#pragma once
#include <functional>
#include <unordered_map>

class Observable {
public:
    using Callback = std::function<void()>;

    int Subscribe(const Callback& cb);
    void Unsubscribe(int id);
    void Notify();

private:
    int nextId = 0;
    std::unordered_map<int, Callback> subscribers;
};
