#include "engine/core/Observable.hpp"
#include "engine/utils/Callback.hpp"

#include <algorithm>

std::atomic<Observable::Event> Observable::next_event_id{1};

Observable::Event Observable::CreateEvent() {
    return next_event_id.fetch_add(1);
}

Observable::~Observable(){
    for (auto& [_, callbacks] : this->subscribers) {
        for (auto* cb : callbacks) {
            delete cb;
        }
        callbacks.clear();
    }
    this->subscribers.clear();
}


Callback* Observable::Subscribe(const payload_function& lambda, Event event) {
    Callback* cb = new Callback(this, lambda);
    this->subscribers[event].push_back(cb);
    return cb;
}

Callback* Observable::Subscribe(const function& lambda, Event event) {
    Callback* cb = new Callback(this, lambda);
    this->subscribers[event].push_back(cb);
    return cb;
}

void Observable::Unsubscribe(Callback* cb){
    if (!cb) return;

    for (auto& [_, callbacks] : this->subscribers) {
        auto it = std::find(callbacks.begin(), callbacks.end(), cb);
        if (it != callbacks.end()) {
            callbacks.erase(it);
            delete cb;
            return;
        }
    }
}

void Observable::Notify(Event event, std::any data) {
    std::vector<Callback*> copy;
    auto collect = [&](Event e) {
        auto found = this->subscribers.find(e);
        if (found == this->subscribers.end()) return;
        copy.insert(copy.end(), found->second.begin(), found->second.end());
    };

    if (event == ALL_EVENT) {
        for (auto& [_, callbacks] : this->subscribers) {
            copy.insert(copy.end(), callbacks.begin(), callbacks.end());
        }
    } else {
        collect(event);
        if (event != ANY_EVENT) {
            collect(ANY_EVENT);
        }
    }

    for (auto* cb : copy){
        if (!cb->Execute(data)){
            Unsubscribe(cb);
        }
    }
}



