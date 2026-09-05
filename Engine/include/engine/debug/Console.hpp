#pragma once
#include "engine/core/System.hpp"
#include <string>
#include <unordered_map>
#include <deque>
#include <iostream>
#include <source_location>
#include "engine/core/TimeManager.hpp"
#include "Message.hpp"

class Console : public System {
public:
    static inline const Event NEW_MESSAGE_EVENT = Console::CreateEvent();
    static inline const Event CLEAR_EVENT = Console::CreateEvent();

    static Console& Get() {
        static Console instance;
        return instance;
    }
    ~Console() = default;
    void Update() override;
    void Shutdown() override;

    static void Clear();

    static void Comment(const std::string &message,
                    const std::source_location& loc = std::source_location::current());
    static void Warn(const std::string &message,
                    const std::source_location& loc = std::source_location::current());
    static void Alert(const std::string &message,
                    const std::source_location& loc = std::source_location::current());
    static std::unordered_map<std::string, Message*>& GetMessages(){return Get().messages;}

    // Hard ceiling on distinct messages retained. Repeats are free -- they collapse onto an
    // existing entry's count -- so this only bounds messages whose TEXT differs.
    //
    // It exists because nothing else bounds them. Every distinct message allocates a Message
    // here and a MessageGui widget in the editor, both alive until Clear(), and ConsoleGui
    // relayouts on each addition from inside Engine::Update(). A script logging a value that
    // changes (a position, a timer, a counter) therefore degraded the frame rate steadily
    // the longer it ran -- the cost grew with the number of lines ever logged.
    static constexpr std::size_t kMaxMessages = 500;


protected:
    Console():System(){};


private:
    static void CreateMessage(std::string message, std::string type, const std::source_location loc);

    // Drop oldest-first until at most kMaxMessages remain.
    static void EvictOverflow();

    std::unordered_map<std::string, Message*> messages;

    // Insertion order of the keys in `messages`, so eviction can be FIFO -- an unordered_map
    // has no notion of "oldest". Holds keys rather than pointers so a stale entry left by
    // Clear() is just a failed lookup rather than a dangling read.
    std::deque<std::string> insertion_order;
};
