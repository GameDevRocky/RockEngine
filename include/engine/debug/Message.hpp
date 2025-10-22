#pragma once
#include <string>
#include <iostream>

struct Message {
    std::string text;
    std::string type;
    std::string created_at;
    std::string file_name;
    std::string line_num;
    int count = 1;
    float time_stamp;

    Message(
        const std::string& text = "",
        const std::string& type = "",
        const std::string& created_at = "",
        const std::string& file_name = "",
        const std::string& line_num = "",
        float time_stamp = 0.0f
    );
};
