#pragma once

#include <QObject>
#include <QRect>
#include <QString>
#include <functional>

class QWidget;
class QRubberBand;

// Shared drop routing for the folder grid, breadcrumbs and directory tree.
// The resolver uses coordinates local to the receiving widget.
class ProjectFileDropTarget final : public QObject {
public:
    struct Target { QString directory; QRect highlight; };
    using Resolver = std::function<Target(const QPoint&)>;
    ProjectFileDropTarget(QWidget* surface, Resolver resolver);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* m_surface;
    QRubberBand* m_highlight;
    Resolver m_resolve;
};
