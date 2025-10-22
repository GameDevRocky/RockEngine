#include "engine/core/Observable.hpp"

int Observable::Subscribe(const Callback& cb, int priority) {
    int id = next_id++;
    auto it = subscribers.emplace(priority, std::make_pair(id, cb));
    id_map[id] = it;
    return id;
}

void Observable::Unsubscribe(int id) {
    auto it = id_map.find(id);
    if (it != id_map.end()) {
        subscribers.erase(it->second);
        id_map.erase(it);
    }
}

void Observable::Notify() {
    for (auto& [priority, pair] : subscribers) {
        auto& [id, cb] = pair;
        if (cb) cb();
    }
}

