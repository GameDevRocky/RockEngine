#pragma once
#include <QWidget>

class InspectorGui : public QWidget {
    Q_OBJECT

public:
    static InspectorGui* Get() {
        static InspectorGui* instance = nullptr;
        if (!instance) {
            instance = new InspectorGui(nullptr);
        }
        return instance;
    }
    void Init();
    explicit InspectorGui(QWidget* parent = nullptr);
private:
    ~InspectorGui() override = default;


};
