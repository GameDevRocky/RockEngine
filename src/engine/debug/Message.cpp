#include "engine/debug/Message.hpp"

Message::Message(
    const std::string& text,
    const std::string& type,
    const std::string& created_at,
    const std::string& file_name,
    const std::string& line_num,
    float time_stamp
)
    : text(text),
      type(type),
      created_at(created_at),
      file_name(file_name),
      line_num(line_num),
      time_stamp(time_stamp)
{}
