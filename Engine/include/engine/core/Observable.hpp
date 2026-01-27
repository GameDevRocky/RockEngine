#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>

class Observable {
public:
    using Callback = std::function<void()>;
    int Subscribe(const Callback& cb, int priority = 0);
    void Unsubscribe(int id);
    void Notify();
    virtual Observable* Copy(){ return nullptr; };

private:
    std::multimap<int, std::pair<int, Callback>, std::greater<int>> subscribers;
    std::unordered_map<int, decltype(subscribers)::iterator> id_map;
    std::atomic<int> next_id{0};
};