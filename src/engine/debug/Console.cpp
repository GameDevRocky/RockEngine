#include "engine/debug/Console.hpp"
#include <filesystem>

void Console::Update() {}
void Console::Shutdown() {}

void Console::CreateMessage(std::string message, std::string type, const std::source_location loc){
    Console& instance = Get();
    
    std::string full_path = loc.file_name();
    std::filesystem::path path(full_path);
    std::string path_str = path.generic_string();    
    size_t pos = path_str.find("src/");
    std::string file_name = (pos == std::string::npos) ? path_str : path_str.substr(pos);

    int line = loc.line();
    std::string function = loc.function_name();
    std::string text = message;
    float time_stamp = TimeManager::Get().ElapsedTime();

    auto& msg = instance.comments[message];
    msg.count++;
    msg.count = msg.count > 999 ? 999 : msg.count;
    msg.file_name = file_name;
    msg.text = text;
    msg.time_stamp = time_stamp;
    msg.type = type;

    instance.Notify();

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
    comments.clear();
    warnings.clear();
    alerts.clear();
}
