#pragma once
#include <functional>
#include <map>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <iostream>
#include <any>

class Callback{
    using function = std::function<void()>;
    using payload_function = std::function<void(std::any)>;

    public:
    
        Callback(const payload_function& cb);
        Callback(const function& cb);
        ~Callback() = default;
        int GetID() const {return id;}
        bool Execute(const std::any& data);

        bool operator==(const Callback& other) const {
        return this->id == other.id;
        }
        
    private:
        payload_function cb = nullptr;
        int id;

};