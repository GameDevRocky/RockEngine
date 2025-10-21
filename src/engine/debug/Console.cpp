#include "engine/debug/Console.hpp"

static void Comment(const std::string& message) {
    auto& comments = Console::Get().comments;
    auto it = comments.find(message);

    if (it != comments.end()) {
        it->second += 1; 
    } else {
        comments[message] = 1;
    }
}


static void Warn(std::string message){
    auto& warnings = Console::Get().warnings;
    auto it = warnings.find(message);

    if (it != warnings.end()) {
        it->second += 1; 
    } else {
        warnings[message] = 1;
    }

}

static void Alert(std::string message){
    auto& alerts = Console::Get().alerts;
    auto it = alerts.find(message);

    if (it != alerts.end()) {
        it->second += 1; 
    } else {
        alerts[message] = 1;
    }

}