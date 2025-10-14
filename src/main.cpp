#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[]) {
    QApplication *app = new QApplication(argc, argv);
    QMainWindow *window = new QMainWindow();
    window->showMaximized();
    app->exec();

    delete window;
    delete app;
}
