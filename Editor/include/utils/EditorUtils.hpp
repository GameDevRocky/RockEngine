#pragma once
#include <string>
#include <QLabel>
#include <QMouseEvent>
#include <QCursor>
#include <Qt>
#include <QEvent>
#include <QEnterEvent>
#include <QFont>
#include <QFileIconProvider>
#include <QFileInfo>

namespace EditorUtils {

class CustomIconProvider : public QFileIconProvider {
public:
    QIcon icon(const QFileInfo &info) const override;
};

void OpenInVSCode(const std::string& fullPath);

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget* parent = nullptr) : QLabel(parent) {
        setCursor(Qt::PointingHandCursor);
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

Q_SIGNALS:
    void clicked();
    void doubleClicked();

protected:
    

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton)
            Q_EMIT clicked();
        QLabel::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton)
            Q_EMIT doubleClicked();
        QLabel::mouseDoubleClickEvent(event);
    }

private:
    QString filePath;
    QString normalStyle;
    QString hoverStyle;
};

} // namespace EditorUtils
