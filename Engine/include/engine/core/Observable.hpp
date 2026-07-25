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
    static constexpr Event ALL_EVENT = UINT64_MAX;

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

    // Notify dispatches in place (no per-notify copy of the subscriber list).
    // std::unordered_map keeps references to its mapped vectors stable across
    // rehash, and vectors are indexed by position, so a handler may safely
    // Subscribe mid-dispatch. Removals (a handler returning false, or an
    // Unsubscribe call) are deferred until dispatch fully unwinds so they never
    // shift the vector out from under an active loop.
    int dispatchDepth = 0;
    std::vector<int> pendingUnsub;
    void FlushPendingUnsub();
};