#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>
#include "engine/utils/Callback.hpp"
#include "engine/utils./EngineUtils.hpp"

using EngineUtils::EventUtils::Event;

class Observable {
public:
    int Subscribe(const Callback* cb, Event event);
    void Unsubscribe(int id);
    void Notify();

protected:
    std::multimap<int, std::pair<int, Callback>, std::greater<int>> subscribers;
    std::unordered_map<int, decltype(subscribers)::iterator> id_map;
    std::atomic<int> next_id{0};
};