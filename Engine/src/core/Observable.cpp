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

    for (auto& [_, callbacks] : this->subscribers) {
        auto it = std::find(callbacks.begin(), callbacks.end(), cb);
        if (it != callbacks.end()) {
            callbacks.erase(it);
            return;
        }
    }
}

void Observable::Unsubscribe(int id) {
    for (auto& [_, callbacks] : this->subscribers) {
        auto it = std::find_if(callbacks.begin(), callbacks.end(),
            [id](const Callback& cb) { return cb.GetID() == id; });
        if (it != callbacks.end()) {
            callbacks.erase(it);
            return;
        }
    }
}

void Observable::Notify(Event event, const std::any& data) {
    std::vector<Callback> copy;
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

    for (auto& cb : copy){
        if (!cb.Execute(data)){
            Unsubscribe(cb);
        }
    }
}



