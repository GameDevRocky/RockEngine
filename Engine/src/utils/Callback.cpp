#include "engine/utils/Callback.hpp"



Callback::Callback(Observable* instance, const function& cb){
    this->instance = std::make_shared<Observable*>(instance);
    this->cb = cb;
}


bool Callback::Execute(){
    try{
        this->cb();
        return true;
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return false;
    }
}


