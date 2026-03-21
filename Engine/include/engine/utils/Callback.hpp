#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>
#include <memory>

class Observable;


class Callback{
    using func = std::function<void()>;
    public:
        Callback(const func& function);
        ~Callback() = default;

        void Init(Observable* instance);
        bool Execute();
        
    
    private:
        std::weak_ptr<Observable> instance;

};