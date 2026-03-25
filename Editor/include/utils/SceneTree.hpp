#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QIcon>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QTreeView>
#include <QStandardItemModel>
#include "engine/core/Scene.hpp"

class SceneTree : public QTreeView{
    Q_OBJECT

    public:
        SceneTree(QWidget* parent = nullptr);
        void RebuildFromScene(Scene* scene);

    private:
    std::string scene_id;
    QStandardItemModel* model = nullptr;
};

