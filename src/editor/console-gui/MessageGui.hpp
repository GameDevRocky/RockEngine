#pragma once
#include <QWidget>
#include <string>
#include <iostream>
#include <QLabel>
#include <QVBoxLayout>

#include "engine/debug/Message.hpp"  // Assuming this defines your Message struct

class ConsoleGui;
class MessageGui : public QWidget{

public:
    explicit MessageGui(ConsoleGui* parent = nullptr, Message* msg = nullptr);
    ConsoleGui* c_parent;
private:
    void Update();
    Message* msg;
    QLabel* count;
    QLabel* file_path;
    QLabel* text;
    QLabel* type;


};