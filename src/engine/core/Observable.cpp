#include "engine/core/Observable.hpp"

int Observable::Subscribe(const Callback& cb) {
    int id = nextId++;
    subscribers[id] = cb;
    return id;
}

void Observable::Unsubscribe(int id) {
    subscribers.erase(id);
}

void Observable::Notify() {
    for (auto& [id, cb] : subscribers) {
        cb();
    }
}
