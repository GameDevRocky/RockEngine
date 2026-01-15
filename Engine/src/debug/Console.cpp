#include "engine/debug/Console.hpp"
#include <filesystem>
#include "Engine.hpp"

void Console::Update() {}
void Console::Shutdown() {}

void Console::CreateMessage(std::string text, std::string type, const std::source_location loc){
    Console& instance = Get();
    Engine* engine = Engine::Get();
    TimeManager* timeManager = engine->GetActiveContainer()->FindSystem<TimeManager>();
    
    std::string full_path = loc.file_name();
    std::filesystem::path path(full_path);
    std::string path_str = path.generic_string();    
    size_t pos = path_str.find("src/");
    std::string file_name = (pos == std::string::npos) ? path_str : path_str.substr(pos);

    int line = loc.line();
    std::string function = loc.function_name();
    float time_stamp = timeManager->ElapsedTime();

    std::string key = text + type;
    auto it = instance.messages.find(key);
    
    
    
    if (it == instance.messages.end()) {
        // Create new message if it doesn't exist
        Message* msg = new Message(text, type, function, file_name, std::to_string(line), time_stamp);
        instance.messages[key] = msg;
        
        msg->Notify();
    } else {
        // Update existing message
        Message* msg = it->second;
        msg->count++;
        msg->count = msg->count > 999 ? 999 : msg->count;
        msg->time_stamp = time_stamp;
        msg->Notify();
    }
    
    Get().Notify();

}

void Console::Comment(const std::string &message,
                      const std::source_location& loc)
{
    CreateMessage(message, "comment", loc);
    
}

void Console::Warn(const std::string &message,
                   const std::source_location& loc)
{
   CreateMessage(message, "warning", loc);
}

void Console::Alert(const std::string &message,
                    const std::source_location& loc)
{
  CreateMessage(message, "alert", loc);
}

void Console::Clear(){
    for (auto& [_, msg] : Get().messages){
        msg->Destroy();
    }
    Get().messages.clear();
    Get().Notify();
    Console::Warn("Cleared");

}
