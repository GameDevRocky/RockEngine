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

    // In the editor, ConsoleGui subscribes to NEW_MESSAGE_EVENT and renders the message map.
    // In a player nothing subscribes, so without this every Alert/Warn/Comment would vanish
    // into a map no one ever reads -- and a shipped game that misbehaves would give you
    // nothing at all to go on. stderr for problems, stdout for the rest.
    if (engine->IsPlayer()) {
        std::ostream& out = (type == "comment") ? std::cout : std::cerr;
        out << "[" << type << "] " << text << "  (" << file_name << ":" << line << ")" << std::endl;
    }

    std::string key = text + type;
    auto it = instance.messages.find(key);
    
    
    
    if (it == instance.messages.end()) {
        Message* msg = new Message(text, type, function, file_name, std::to_string(line), time_stamp);
        instance.messages[key] = msg;
        instance.insertion_order.push_back(key);

        msg->Notify();
        EvictOverflow();

        // The key rides along so ConsoleGui can add exactly this one widget. Without it the
        // editor had to rescan the whole map on every message to find what was new, which is
        // O(n) per log line inside the frame loop.
        Get().Notify(Console::NEW_MESSAGE_EVENT, key);
    } else {
        Message* msg = it->second;
        msg->count++;
        msg->count = msg->count > 999 ? 999 : msg->count;
        msg->time_stamp = time_stamp;
        // A repeat needs no new widget: MessageGui subscribes to its own Message and
        // refreshes its count off this notify. Deliberately does NOT re-stamp the key's
        // position in insertion_order -- eviction ages by first sighting, so a message
        // repeating forever cannot pin the queue and starve out newer ones.
        msg->Notify();

        Get().Notify(Console::NEW_MESSAGE_EVENT, key);
    }
}

void Console::EvictOverflow() {
    Console& instance = Get();
    while (instance.messages.size() > kMaxMessages && !instance.insertion_order.empty()) {
        const std::string oldest = instance.insertion_order.front();
        instance.insertion_order.pop_front();

        auto victim = instance.messages.find(oldest);
        if (victim == instance.messages.end())
            continue;   // already gone via Clear(); the queue entry was just stale

        Message* dying = victim->second;
        instance.messages.erase(victim);

        // Destroy() before delete, and in that order. It flips isDestroyed and notifies,
        // which is what makes MessageGui drop itself out of ConsoleGui::message_widgets and
        // deleteLater() its widget. Freeing the Message without that leaves the editor
        // holding a widget pointed at freed memory.
        dying->Destroy();
        delete dying;
    }
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
        // Same order as EvictOverflow: Destroy() detaches the editor's widget, then the
        // Message itself goes. This used to stop at Destroy(), so every message ever logged
        // stayed allocated for the life of the process even after the panel looked empty.
        msg->Destroy();
        delete msg;
    }
    Get().messages.clear();
    Get().insertion_order.clear();
    Get().Notify(Console::CLEAR_EVENT);
    Console::Warn("Cleared");

}
