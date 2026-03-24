#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>
class Callback;
class Observable {
public:
    using function = std::function<void()>;
    void Subscribe(const function& lambda);
    void Notify();

protected:
    std::vector<Callback*> subscribers;
};