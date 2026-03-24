#include "engine/core/Observable.hpp"
#include "engine/utils/Callback.hpp"

void Observable::Subscribe(const function& lambda) {
    Callback* cb = new Callback(this, lambda);
    this->subscribers.push_back(cb);
}


void Observable::Notify() {
    std::vector<Callback*> copy = this->subscribers;

    for (auto* cb : copy){
        if (!cb->Execute()){
            std::erase(this->subscribers, cb);
        }
    }
    copy.clear();
}
