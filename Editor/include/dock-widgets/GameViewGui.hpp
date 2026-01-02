#pragma once
#include <QWidget>

class GameViewGui : public QWidget {
    Q_OBJECT

public:
    static GameViewGui* Get() {
        static GameViewGui* instance = nullptr;
        if (!instance) {
            instance = new GameViewGui(nullptr);
        }
        return instance;
    }
    void Init();
    explicit GameViewGui(QWidget* parent = nullptr);
private:
    ~GameViewGui() override = default;


};
