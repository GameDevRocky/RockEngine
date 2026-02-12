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
    void SetScene(Scene* scene);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    ~HierarchyGui() override = default;
    void CreateHeader();

    QTextEdit* filter = nullptr;
    QVBoxLayout* layout = nullptr;
    QTreeView* treeView = nullptr;
    HierarchyTreeModel* treeModel = nullptr;
    Scene* currentScene = nullptr;

    QWidget* header = nullptr;

};
