#include "engine/debug/Console.hpp"
#include "engine/jobs/MainThread.hpp"
#include <filesystem>
#include "Engine.hpp"

void Console::Update() {}
void Console::Shutdown() {}

void Console::CreateMessage(std::string text, std::string type, const std::source_location loc){
    // Unguarded message map, an unchecked FindSystem<TimeManager>() deref below,
    // and a Notify that fans out into Qt. A job that wants to log does it from
    // its main-thread step or its completion callback, never from the worker.
    ROCK_ASSERT_MAIN_THREAD();

    Console& instance = Get();
    Engine* engine = Engine::Get();
    TimeManager* timeManager = engine->GetActiveContainer()->FindSystem<TimeManager>();
    
    std::string full_path = loc.file_name();
    std::filesystem::path path(full_path);
    std::string path_str = path.generic_string();
    std::string root = std::string(PROJECT_ROOT) + "/";
    std::string file_name = (path_str.find(root) == 0)
        ? path_str.substr(root.length())
        : path_str;

    int line = loc.line();
    std::string function = loc.function_name();
    float time_stamp = timeManager->ElapsedTime();

    std::string key = text + type;
    auto it = instance.messages.find(key);
    
    
    
    if (it == instance.messages.end()) {
        Message* msg = new Message(text, type, function, file_name, std::to_string(line), time_stamp);
        instance.messages[key] = msg;
        
        msg->Notify();
    } else {
        Message* msg = it->second;
        msg->count++;
        msg->count = msg->count > 999 ? 999 : msg->count;
        msg->time_stamp = time_stamp;
        msg->Notify();
    }
    
    Get().Notify(Console::NEW_MESSAGE_EVENT);

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
    Get().Notify(Console::CLEAR_EVENT);
    Console::Warn("Cleared");

}
