#pragma once
#include "engine/core/System.hpp"
#include <string>
#include <unordered_map>
#include <iostream>
#include <source_location>
#include "engine/core/TimeManager.hpp"
#include "Message.hpp"

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

    void Clear();

    static void Comment(const std::string &message,
                    const std::source_location& loc = std::source_location::current());
    static void Warn(const std::string &message,
                    const std::source_location& loc = std::source_location::current());
    static void Alert(const std::string &message,
                    const std::source_location& loc = std::source_location::current());

    static std::unordered_map<std::string, Message>& GetComments(){return Get().comments;}
    static std::unordered_map<std::string, Message>& GetWarnings(){return Get().warnings;}
    static std::unordered_map<std::string, Message>& GetAlerts(){return Get().alerts;}

    

protected:
    Console():System(){};


private:
    static void CreateMessage(std::string message, std::string type, const std::source_location loc);
    std::unordered_map<std::string, Message> comments;
    std::unordered_map<std::string, Message> warnings;
    std::unordered_map<std::string, Message> alerts;
};
