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
#include <QAbstractItemModel>
#include "engine/core/Scene.hpp"


class SceneModel : public QAbstractItemModel{
    explicit SceneModel(Scene* scene, QObject* parent = nullptr);
    ~SceneModel() override = default;

    



};

class SceneTree : public QTreeView{
    Q_OBJECT

    public:
        void Init(Scene* scene);
        SceneTree(QWidget* parent = nullptr);

    private:
        void Update();

    std::string scene_id;
    SceneModel* model = nullptr;
};

