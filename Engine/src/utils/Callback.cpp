#include "engine/utils/Callback.hpp"



Callback::Callback(Observable* instance, const payload_function& cb){
    this->instance = std::make_shared<Observable*>(instance);
    this->cb = cb;
}

Callback::Callback(Observable* instance, const function& cb){
    this->instance = std::make_shared<Observable*>(instance);
    this->cb = [cb](std::any) { cb(); };
}


bool Callback::Execute(std::any data){
    try{
        this->cb(data);
        return true;
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return false;
    }
}


