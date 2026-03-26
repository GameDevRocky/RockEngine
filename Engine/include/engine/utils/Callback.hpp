#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <iostream>
#include <any>


class Observable;

class Callback{
    using function = std::function<void()>;
    using payload_function = std::function<void(std::any)>;
    public:
        Callback(Observable* instance, const payload_function& cb);
        Callback(Observable* instance, const function& cb);
        ~Callback() = default;
        bool Execute(std::any data);
        
    
    private:
        std::weak_ptr<Observable*> instance;
        payload_function cb = nullptr;

};