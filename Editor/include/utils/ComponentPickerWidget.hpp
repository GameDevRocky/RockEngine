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

// A lightweight popup for choosing one name from a flat list (e.g. the component
// types from SerializableFactory). Mirrors AssetPickerWidget's popup behaviour
// (Qt::Popup + WA_DeleteOnClose + search filter + onSelected callback) but renders
// a plain text list instead of a thumbnail grid.
class ComponentPickerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComponentPickerWidget(std::vector<std::string> items, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup),
          m_allItems(std::move(items))
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
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        root->addWidget(m_list);

        buildAllItems();

        setMinimumWidth(180);

        connect(m_search, &QLineEdit::textChanged, this, &ComponentPickerWidget::filterItems);
        // Single click commits — this reads as a menu, not a grid you browse.
        connect(m_list, &QListWidget::itemClicked, this, &ComponentPickerWidget::commitSelection);

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
        // Keep a valid current item so Enter always has a target.
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
        if (!item || item->isHidden()) return;
        const std::string name = item->text().toStdString();
        if (onSelected) onSelected(name);
        close();
    }

private:
    void buildAllItems() {
        for (const auto& name : m_allItems)
            m_list->addItem(QString::fromStdString(name));
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
    }

    QLineEdit*   m_search = nullptr;
    QListWidget* m_list   = nullptr;
    std::vector<std::string> m_allItems;
};
