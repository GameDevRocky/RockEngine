#pragma once
#include "engine/core/System.hpp"
#include <string>
#include <unordered_map>
#include <iostream>

class Console : public System {
public:
    static Console& Get() {
        static Console instance;
        return instance;
    }
    ~Console() = default;
    void Init() override {}
    void Update() override;
    void Shutdown() override;

    static void Comment(const std::string &message);
    static void Warn(const std::string &message);
    static void Alert(const std::string &message);

    static std::unordered_map<std::string, int>& GetComments();
    static std::unordered_map<std::string, int>& GetWarnings();
    static std::unordered_map<std::string, int>& GetAlerts();

protected:
    Console():System(){};


private:
    std::unordered_map<std::string, int> comments;
    std::unordered_map<std::string, int> warnings;
    std::unordered_map<std::string, int> alerts;
};
