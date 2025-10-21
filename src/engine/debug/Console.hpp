#pragma once
#include "engine/core/System.hpp"
#include "engine/debug/Message.hpp"
#include <string>
#include <unordered_map>

class Console : public System{
public:
    std::unordered_map<std::string, int> comments;
    std::unordered_map<std::string, int> warnings;
    std::unordered_map<std::string, int> alerts;

    static Console& Get() {
        static Console instance;
        return instance;
    }
    ~Console() = default;
    void Init() override;
    void Update() override;
    void Shutdown() override;

    static void Comment(std::string message);
    static void Warn(std::string message);
    static void Alert(std::string message);

    
protected:
    Console() = default; 
};