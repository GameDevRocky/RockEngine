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

class GameObject;

class SceneTree : public QTreeView{
    Q_OBJECT

    public:
        SceneTree(QWidget* parent = nullptr);
        void RebuildFromScene(Scene* scene);
        
        void AddItem(const std::string& parentId, GameObject* child);
        void RemoveItem(const std::string& id);
        void ReparentItem(const std::string& childId, const std::string& newParentId);

        QStandardItemModel* GetModel() const { return model; }
        const std::string& GetSceneId() const { return scene_id; }
        bool IsHandlingDrop() const { return handlingDrop; }

    protected:
        void dropEvent(QDropEvent* event) override;

    private slots:
        void OnHeaderClicked(int section);

    private:
        QModelIndex FindItemById(const std::string& id) const;
        
        std::string scene_id;
        QStandardItemModel* model = nullptr;
        bool handlingDrop = false;
        bool collapsed = false;
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