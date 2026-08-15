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
#include <QPainter>
#include <QStyle>

namespace EditorUtils {

class CustomIconProvider : public QFileIconProvider {
public:
    QIcon icon(const QFileInfo &info) const override;

    // Shared semantic icons used outside QFileSystemModel-backed views. Keeping
    // these here prevents the Hierarchy, property pickers, and AI attachments
    // from each hard-coding their own bundled icon paths.
    static QIcon gameObjectIcon();
    static QIcon componentIcon(const std::string& typeName);
    static QIcon aiSendIcon();
    static QIcon aiSettingsIcon();
    static QIcon disclosureArrowRightIcon();
    static QIcon disclosureArrowDownIcon();
};

void OpenInVSCode(const std::string& fullPath);

// Normalises a raw identifier into a human-readable inspector label.
//
//   "startOffset"   -> "Start Offset"      (camelCase split)
//   "start_offset"  -> "Start Offset"      ('_' and '-' become spaces)
//   "uvMin"         -> "Uv Min"
//   "UVMin"         -> "UV Min"            (acronym kept whole)
//   "Color: "       -> "Color"             (trailing ':' / spaces dropped)
//   "order in layer"-> "Order In Layer"
//
// Every alphabetic run gets its first letter capitalised and the rest left alone, so
// existing all-caps words ("UV", "X") survive instead of being flattened to "Uv"/"x".
// A trailing colon is stripped rather than preserved so callers can append their own
// separator uniformly without risking "Color:: ".
std::string FormatLabel(const std::string& raw);

// A QLabel that truncates its text with an ellipsis instead of forcing its column
// wider than the layout wants to give it.
//
// The minimumSizeHint override is what actually makes this work, not the painting:
// QLabel's natural minimum width IS its full text width, so inside a
// stretch-proportioned grid (see InspectorVisitor's 1:2 label/widget columns) a long
// label would silently win the argument and push the column past its share rather
// than ever being squeezed. Reporting no minimum width hands that decision back to
// the layout; paintEvent then fits the text to whatever width it was given.
//
// text() stays the single source of truth -- deliberately no cached copy of the
// string, so a later setText() through a QLabel* can't leave this drawing stale text.
class ElidingLabel : public QLabel {
public:
    explicit ElidingLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(text, parent) {
        // The untruncated text stays reachable on hover, so eliding never hides
        // information outright.
        setToolTip(text);
    }

    QSize minimumSizeHint() const override {
        return QSize(0, QLabel::minimumSizeHint().height());
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        const QString elided = fontMetrics().elidedText(
            text(), Qt::ElideRight, contentsRect().width());
        // drawItemText rather than a raw drawText: it routes through the style, so the
        // stylesheet's colour and the greyed-out disabled state are honoured exactly as
        // a plain QLabel would render them.
        style()->drawItemText(&painter, contentsRect(), alignment(), palette(),
                              isEnabled(), elided, foregroundRole());
    }
};

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
