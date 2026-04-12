#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QColorDialog>
#include "engine/utils/Properties.hpp"

class PropertyFactory {
public:
    static QWidget* Create(Properties::Tags tag, const Properties::PropDesc& desc) {
        switch (tag) {
            case Properties::Tags::FLOAT:
            case Properties::Tags::ANGLE:
                return createFloatWidget(desc);

            case Properties::Tags::VECTOR2:
                return createVec2Widget(desc);

            case Properties::Tags::TOGGLE:
                return new QCheckBox();
            case Properties::Tags::COLOR: // New Case
                return createColorWidget(desc);

            case Properties::Tags::READONLY:
                return createReadOnlyWidget();

            default:
                return new QLabel("Unknown Type");
        }
    }

private:
    static void updateButtonStyle(QPushButton* btn, const QColor& color) {
        QString qss = QString("background-color: %1; border: 1px solid #333; height: 20px;")
                        .arg(color.name());
        btn->setStyleSheet(qss);
    }
    
    static QWidget* createColorWidget(const Properties::PropDesc& desc) {
        QPushButton* colorBtn = new QPushButton();
        colorBtn->setFixedWidth(60);
        return colorBtn;
    }

    static QWidget* createFloatWidget(const Properties::PropDesc& desc) {
        auto* spin = new QDoubleSpinBox();
        spin->setRange(desc.min, desc.max);
        spin->setSingleStep(desc.step);
        spin->setDecimals(2);
        return spin;
    }

    static QWidget* createVec2Widget(const Properties::PropDesc& desc) {
        QWidget* container = new QWidget();
        container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        QHBoxLayout* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);

        for (int i = 0; i < 2; ++i) {
            auto* spin = new QDoubleSpinBox();
            spin->setMaximumWidth(128);
            spin->setRange(desc.min, desc.max);
            spin->setSingleStep(desc.step);
            spin->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            
            spin->setObjectName(i == 0 ? "vec_x" : "vec_y");
            auto* label = new QLabel(i == 0 ? "X" : "Y");
            auto font = label->font(); 
            font.setBold(true);
            label->setFont(font);
            label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            layout->addWidget(label);
            layout->addWidget(spin);
        }

        return container;
    }

    static QWidget* createReadOnlyWidget() {
        auto* label = new QLabel("---");
        label->setStyleSheet("color: gray;");
        return label;
    }
};