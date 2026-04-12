#include "engine/utils/Callback.hpp"

static int next_id = 0;

Callback::Callback(const payload_function& cb){
    this->id = next_id++;
    this->cb = cb;
}

Callback::Callback(const function& cb){
    this->id = next_id++;
    this->cb = [cb](std::any){ return cb(); };
}


bool Callback::Execute(const std::any& data){
    try{
        return this->cb(data);
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return false;
    }
}


