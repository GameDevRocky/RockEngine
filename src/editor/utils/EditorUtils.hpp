#pragma once
#include <string>
#include <QLabel>
#include <QMouseEvent>
#include <QCursor>

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

        // Default style
        normalStyle = "color: #EEEEEE; text-decoration: none;";
        hoverStyle  = "color: #EEEEEE; text-decoration: underline;";
        setStyleSheet(normalStyle);
    }

    void setFilePath(const QString& path) { filePath = path; }
    QString getFilePath() const { return filePath; }

signals:
    void clicked();
    void doubleClicked();

protected:
    void enterEvent(QEnterEvent* event) override {
        setStyleSheet(hoverStyle);
        QLabel::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        setStyleSheet(normalStyle);
        QLabel::leaveEvent(event);
    }

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
