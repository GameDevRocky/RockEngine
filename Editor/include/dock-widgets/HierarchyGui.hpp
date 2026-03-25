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
#include <unordered_map>
#include "utils/SceneTree.hpp"

class HierarchyTreeModel;
class Scene;
class Callback;
class Transform;
class Container;

class HierarchyGui : public QWidget {
    Q_OBJECT

public:
    static HierarchyGui* Get() {
        static HierarchyGui* instance = nullptr;
        if (!instance) {
            instance = new HierarchyGui(nullptr);
        }
        return instance;
    }
    void Init();
    void PostInit();
    void UpdateHierarchy();
    explicit HierarchyGui(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    ~HierarchyGui() override;

    Scene* GetActiveScene() const;
    void RequestHierarchyRefresh();
    void SyncTransformSubscriptions(Scene* scene);
    void ClearTransformSubscriptions();

    QVBoxLayout* layout = nullptr;
    QTreeView* treeView = nullptr;
    std::vector<SceneTree*> sceneTrees;
    std::unordered_map<std::string, std::pair<Transform*, Callback*>> transformSubscriptions;
    std::string activeSceneId;
    const Scene* activeScenePtr = nullptr;
    const Container* activeContainerPtr = nullptr;
    bool hierarchyDirty = true;
};
