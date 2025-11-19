#pragma once
#include <string>
#include <QLabel>
#include <QMouseEvent>
#include <QCursor>
#include <Qt>
#include <QEvent>
#include <QEnterEvent>
#include <QFont>
namespace EditorUtils {

// Open a file in VSCode
void OpenInVSCode(const std::string& fullPath);

// -----------------------------
// ClickableLabel Declaration
// -----------------------------
class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget* parent = nullptr) : QLabel(parent) {
        // Cursor for hover
        setCursor(Qt::PointingHandCursor);

        // Enable mouse tracking so hover events work
        setMouseTracking(true);
    }
    void enterEvent(QEnterEvent* event) override {
        QFont f = font();
        f.setUnderline(true);
        setFont(f);
        QLabel::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        QFont f = font();
        f.setUnderline(false);
        setFont(f);
        QLabel::leaveEvent(event);
    }


    void setFilePath(const QString& path) { filePath = path; }
    QString getFilePath() const { return filePath; }

signals:
    void clicked();
    void doubleClicked();

protected:
    

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton)
            emit clicked();
        QLabel::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton)
            emit doubleClicked();
        QLabel::mouseDoubleClickEvent(event);
    }

private:
    QString filePath;
    QString normalStyle;
    QString hoverStyle;
};

} // namespace EditorUtils
