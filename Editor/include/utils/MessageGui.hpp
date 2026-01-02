#pragma once
#include <QWidget>
#include <string>
#include <iostream>
#include <QLabel>
#include <QVBoxLayout>
#include "utils/EditorUtils.hpp"

#include "engine/debug/Message.hpp"  // Assuming this defines your Message struct

class ConsoleGui;
class MessageGui : public QWidget{

public:
    explicit MessageGui(ConsoleGui* parent = nullptr, Message* msg = nullptr);
    ConsoleGui* c_parent;

protected:


private:
    void Update();
    Message* msg;
    QLabel* count;
    EditorUtils::ClickableLabel* file_path;
    QLabel* text;
    QLabel* type;
    std::string fullPath;

};