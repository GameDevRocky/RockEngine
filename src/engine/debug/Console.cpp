#include "engine/debug/Console.hpp"

void Console::Comment(const std::string& message) {
    
    auto& comments = Console::Get().comments;
    auto it = comments.find(message);

    if (it != comments.end()) {
        it->second += 1; 
    } else {
        comments[message] = 1;
    }
    Console::Get().Notify();
    std::cout << "Comment: " << message << std::endl; 
}

void Console::Warn(const std::string &message){
    auto& warnings = Console::Get().warnings;
    auto it = warnings.find(message);

    if (it != warnings.end()) {
        it->second += 1; 
    } else {
        warnings[message] = 1;
    }
    Console::Get().Notify();
    std::cout << "Warning: " << message << std::endl; 
}

void Console::Alert(const std::string &message){
    auto& alerts = Console::Get().alerts;
    auto it = alerts.find(message);

    if (it != alerts.end()) {
        it->second += 1; 
    } else {
        alerts[message] = 1;
    }
    Console::Get().Notify();
    std::cout << "Alert: " << message << std::endl;  
}

std::unordered_map<std::string, int>& Console::GetComments(){
    return Console::Get().comments;
}

std::unordered_map<std::string, int>& Console::GetWarnings(){
    return Console::Get().warnings;
}

std::unordered_map<std::string, int>& Console::GetAlerts(){
    return Console::Get().alerts;
}

void Console::Update(){

}
void Console::Shutdown(){

}