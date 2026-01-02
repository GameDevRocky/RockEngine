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
    void Start();
    explicit HierarchyGui(QWidget* parent = nullptr);
    void SetScene(Scene* scene);


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
