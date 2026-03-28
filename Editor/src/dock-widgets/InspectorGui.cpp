#include "dock-widgets/InspectorGui.hpp"
#include  "iostream"

InspectorGui::InspectorGui(QWidget* parent) : QWidget(parent){
    setMinimumWidth(200);
}

void InspectorGui::Init(){ 
    std::cout << "InspectorGui Initialized" << std::endl;
}