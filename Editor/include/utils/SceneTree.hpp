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
#include <QDropEvent>
#include <QTreeView>
#include <QStandardItem>
#include <QStandardItemModel>
#include "engine/core/Scene.hpp"

class SceneTree : public QTreeView{
    Q_OBJECT

    public:
        SceneTree(QWidget* parent = nullptr);
        void RebuildFromScene(Scene* scene);

    protected:
        void dropEvent(QDropEvent* event) override;

    private:
    std::string scene_id;
    QStandardItemModel* model = nullptr;
};

class GamobjectItem : public QStandardItem {
    public:
        static constexpr int Type = QStandardItem::UserType + 1;

        GamobjectItem() = default;
        explicit GamobjectItem(const QString& text) : QStandardItem(text) {}

        int type() const override { return Type; }

        void SetGameObjectId(const std::string& id) {
            gameobject_id = id;
            setData(QString::fromStdString(id), Qt::UserRole + 1);
        }

        const std::string& GetGameObjectId() const { return gameobject_id; }

    private:
        std::string gameobject_id;
};