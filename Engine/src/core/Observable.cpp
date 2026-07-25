#include "engine/core/Observable.hpp"

#include <algorithm>

std::atomic<Observable::Event> Observable::next_event_id{1};

Observable::Event Observable::CreateEvent() {
    return next_event_id.fetch_add(1);
}

Observable::~Observable(){
    for (auto& [_, callbacks] : this->subscribers) {
        callbacks.clear();
    }
    this->subscribers.clear();
}


int Observable::Subscribe(const payload_function& lambda, Event event) {
    this->subscribers[event].emplace_back(lambda);
    return this->subscribers[event].back().GetID();
}

int Observable::Subscribe(const function& lambda, Event event) {
    this->subscribers[event].emplace_back(lambda);
    return this->subscribers[event].back().GetID();
}

void Observable::Unsubscribe(Callback& cb){
    Unsubscribe(cb.GetID());
}

void Observable::Unsubscribe(int id) {
    // Defer removal while a dispatch is in flight so it can't shift a vector an
    // active Notify loop is walking; FlushPendingUnsub applies it on unwind.
    if (dispatchDepth > 0) {
        pendingUnsub.push_back(id);
        return;
    }
    for (auto& [_, callbacks] : this->subscribers) {
        auto it = std::find_if(callbacks.begin(), callbacks.end(),
            [id](const Callback& cb) { return cb.GetID() == id; });
        if (it != callbacks.end()) {
            callbacks.erase(it);
            return;
        }
    }
}

void Observable::FlushPendingUnsub() {
    if (pendingUnsub.empty()) return;
    // Move out first: an Unsubscribe here runs at depth 0 and erases directly.
    std::vector<int> ids;
    ids.swap(pendingUnsub);
    for (int id : ids) Unsubscribe(id);
}

void Observable::Notify(Event event, const std::any& data) {
    ++dispatchDepth;

    // Dispatch one bucket in place. Snapshot the size so callbacks added during
    // this dispatch don't fire until the next Notify (matching the old
    // copy-then-iterate semantics); re-check size() each step in case the vector
    // reallocated on a mid-dispatch Subscribe. A handler returning false is
    // queued for removal, not erased mid-loop.
    auto dispatchBucket = [&](Event e) {
        auto found = this->subscribers.find(e);
        if (found == this->subscribers.end()) return;
        std::vector<Callback>& callbacks = found->second; // ref stable across rehash
        const std::size_t count = callbacks.size();
        for (std::size_t i = 0; i < count && i < callbacks.size(); ++i) {
            Callback& cb = callbacks[i];
            const int cbId = cb.GetID();
            // Skip anything already queued for removal (e.g. a prior handler
            // unsubscribed it this dispatch).
            bool skip = false;
            for (int pid : pendingUnsub) if (pid == cbId) { skip = true; break; }
            if (skip) continue;
            if (!cb.Execute(data))
                pendingUnsub.push_back(cbId);
        }
    };

    if (event == ALL_EVENT) {
        // Snapshot keys: dispatching may insert new event buckets, and iterating
        // the map itself across that is unsafe (mapped-value refs stay valid, but
        // map iterators do not).
        std::vector<Event> keys;
        keys.reserve(this->subscribers.size());
        for (auto& [ev, _] : this->subscribers) keys.push_back(ev);
        for (Event ev : keys) dispatchBucket(ev);
    } else {
        dispatchBucket(event);
        if (event != ANY_EVENT) {
            dispatchBucket(ANY_EVENT);
        }
    }

    if (--dispatchDepth == 0) {
        FlushPendingUnsub();
    }
}



