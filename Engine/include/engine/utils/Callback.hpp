#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <iostream>

class Observable;

class Callback{
    using function = std::function<void()>;
    public:
        Callback(Observable* instance, const function& cb);
        ~Callback() = default;
        bool Execute();
    
    private:
        std::weak_ptr<Observable*> instance;
        function cb = nullptr;

};