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
#include "utils/SceneTree.hpp"

class HierarchyTreeModel;
class Scene;

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
    explicit HierarchyGui(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    ~HierarchyGui() override = default;

    QVBoxLayout* layout = nullptr;
    QTreeView* treeView = nullptr;
    std::vector<SceneTree*> sceneTrees;
};
