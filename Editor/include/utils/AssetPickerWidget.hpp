#pragma once
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QKeyEvent>
#include <QPixmap>
#include <QIcon>

class AssetPickerWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int kThumbSize  = 72;
    static constexpr int kCellWidth  = kThumbSize + 18;
    static constexpr int kCellHeight = kThumbSize + 30;

    // thumbnailGen:    optional GL/image render → QPixmap.
    // fallbackIconGen: optional — used when thumbnailGen returns null (e.g. shaders via CustomIconProvider).
    // Grey placeholder is used only when both return nothing.
    explicit AssetPickerWidget(
        std::vector<std::pair<std::string, std::string>> items,
        std::function<QPixmap(const std::string& id)> thumbnailGen = nullptr,
        std::function<QIcon(const std::string& id)>   fallbackIconGen = nullptr,
        QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup),
          m_allItems(std::move(items)),
          m_thumbnailGen(std::move(thumbnailGen)),
          m_fallbackIconGen(std::move(fallbackIconGen))
    {
        setAttribute(Qt::WA_DeleteOnClose);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        m_search = new QLineEdit(this);
        m_search->setPlaceholderText("Search…");
        m_search->setClearButtonEnabled(true);
        root->addWidget(m_search);

        m_list = new QListWidget(this);
        m_list->setViewMode(QListView::IconMode);
        m_list->setIconSize(QSize(kThumbSize, kThumbSize));
        m_list->setGridSize(QSize(kCellWidth, kCellHeight));
        m_list->setResizeMode(QListView::Adjust);
        m_list->setWordWrap(true);
        m_list->setSpacing(2);
        m_list->setMovement(QListView::Static);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setUniformItemSizes(true);
        root->addWidget(m_list);

        buildAllItems();

        // Show ~3 rows by default; let the popup resize vertically.
        setMinimumWidth(kCellWidth * 10);
        setMinimumHeight(kCellHeight * 5 + m_search->sizeHint().height() + 16);

        connect(m_search, &QLineEdit::textChanged, this, &AssetPickerWidget::filterItems);
        connect(m_list, &QListWidget::itemDoubleClicked,
                this, &AssetPickerWidget::commitSelection);

        m_search->setFocus();
    }

    std::function<void(const std::string&)> onSelected;

protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            commitSelection(m_list->currentItem());
        } else if (e->key() == Qt::Key_Escape) {
            close();
        } else {
            QWidget::keyPressEvent(e);
        }
    }

private slots:
    void filterItems(const QString& filter) {
        const QString lower = filter.toLower();
        for (int i = 0; i < m_list->count(); ++i) {
            auto* item = m_list->item(i);
            item->setHidden(!lower.isEmpty() &&
                            !item->text().toLower().contains(lower));
        }
        // Keep a valid current item.
        if (!m_list->currentItem() || m_list->currentItem()->isHidden()) {
            for (int i = 0; i < m_list->count(); ++i) {
                if (!m_list->item(i)->isHidden()) {
                    m_list->setCurrentRow(i);
                    break;
                }
            }
        }
    }

    void commitSelection(QListWidgetItem* item) {
        if (!item) return;
        const std::string id = item->data(Qt::UserRole).toString().toStdString();
        if (onSelected) onSelected(id);
        close();
    }

private:
    void buildAllItems() {
        for (const auto& [name, id] : m_allItems) {
            auto* item = new QListWidgetItem(QString::fromStdString(name));
            item->setData(Qt::UserRole, QString::fromStdString(id));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

            QPixmap px;
            if (m_thumbnailGen)
                px = m_thumbnailGen(id);

            if (!px.isNull()) {
                item->setIcon(QIcon(px.scaled(kThumbSize, kThumbSize,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            } else if (m_fallbackIconGen) {
                QIcon icon = m_fallbackIconGen(id);
                if (!icon.isNull()) {
                    item->setIcon(icon);
                } else {
                    QPixmap placeholder(kThumbSize, kThumbSize);
                    placeholder.fill(QColor(70, 70, 70));
                    item->setIcon(QIcon(placeholder));
                }
            } else {
                QPixmap placeholder(kThumbSize, kThumbSize);
                placeholder.fill(QColor(70, 70, 70));
                item->setIcon(QIcon(placeholder));
            }
            m_list->addItem(item);
        }
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
    }

    QLineEdit*  m_search = nullptr;
    QListWidget* m_list  = nullptr;
    std::vector<std::pair<std::string, std::string>> m_allItems;
    std::function<QPixmap(const std::string& id)>    m_thumbnailGen;
    std::function<QIcon(const std::string& id)>      m_fallbackIconGen;
};
