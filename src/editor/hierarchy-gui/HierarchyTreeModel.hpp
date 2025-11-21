#pragma once
#include <QAbstractItemModel>
#include <QModelIndex>
#include <string>
#include <vector>
#include <memory>

class Scene;
class GameObject;

struct HierarchyTreeItem {
    explicit HierarchyTreeItem(const std::string& objId, HierarchyTreeItem* parent = nullptr);
    ~HierarchyTreeItem();

    HierarchyTreeItem* parent;
    std::vector<std::unique_ptr<HierarchyTreeItem>> children;
    std::string gameObjectId;
    
    HierarchyTreeItem* child(int row) const;
    int childCount() const;
    int row() const;
    void appendChild(std::unique_ptr<HierarchyTreeItem> child);
    void clear();
};

class HierarchyTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit HierarchyTreeModel(Scene* scene, QObject* parent = nullptr);
    ~HierarchyTreeModel() override = default;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void SetScene(Scene* scene);
    Scene* GetScene() const { return scene; }

    void Rebuild();

private:
    Scene* scene;
    std::unique_ptr<HierarchyTreeItem> rootItem;
    void BuildTreeFromScene();
    void AddGameObjectAndChildren(HierarchyTreeItem* parentItem, GameObject* gameObj);
};
