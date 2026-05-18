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
#include <QScrollArea>
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
    void RefreshHierarchy();
    void AddSceneTree(const std::string& scene_id);
    void RemoveSceneTree(const std::string& id);
    explicit HierarchyGui(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    ~HierarchyGui() override;
    QScrollArea*  scrollArea   = nullptr;
    QWidget*      scrollWidget = nullptr;
    QVBoxLayout*  scrollLayout = nullptr;
    std::map<std::string, SceneTree*> sceneTrees;


};
