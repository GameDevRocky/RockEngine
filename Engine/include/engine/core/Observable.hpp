#pragma once
#include <functional>
#include <unordered_map>
#include <atomic>
#include <vector>
#include <cstdint>
#include <limits>
#include <any>
#include "engine/utils/Callback.hpp"

class Observable {
public:
    using Event = std::uint64_t;
    using function = std::function<bool()>;
    using payload_function = std::function<bool(std::any)>;

    static constexpr Event ANY_EVENT = 0;
    static constexpr Event ALL_EVENT = INT64_MAX;

    static Event CreateEvent();

    int Subscribe(const payload_function& lambda, Event event = ANY_EVENT);
    int Subscribe(const function& lambda, Event event = ANY_EVENT);
    void Unsubscribe(Callback& cb);
    void Unsubscribe(int id);

    void Notify(Event event = ANY_EVENT, const std::any& data = {});
    ~Observable();

protected:
    std::unordered_map<Event, std::vector<Callback>> subscribers;

private:
    static std::atomic<Event> next_event_id;
};