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

// Qt::Popup floating picker — dismissed on outside-click (matches color picker pattern).
// Items are (displayName, id) pairs. On selection, onSelected(id) is called.
class AssetPickerWidget : public QWidget {
    Q_OBJECT
public:
    explicit AssetPickerWidget(
        std::vector<std::pair<std::string, std::string>> items,
        QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup), m_allItems(std::move(items))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setMinimumWidth(260);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        m_search = new QLineEdit(this);
        m_search->setPlaceholderText("Search…");
        m_search->setClearButtonEnabled(true);
        layout->addWidget(m_search);

        m_list = new QListWidget(this);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        layout->addWidget(m_list);

        // Populate full list initially
        repopulate("");

        connect(m_search, &QLineEdit::textChanged, this, &AssetPickerWidget::repopulate);
        connect(m_list, &QListWidget::itemDoubleClicked, this, &AssetPickerWidget::commitSelection);

        // Give search bar immediate focus
        m_search->setFocus();

        adjustSize();
    }

    // Callback invoked with the selected asset ID.
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
    void repopulate(const QString& filter) {
        m_list->clear();
        const QString lower = filter.toLower();
        for (const auto& [name, id] : m_allItems) {
            const QString display = QString::fromStdString(name);
            if (lower.isEmpty() || display.toLower().contains(lower)) {
                auto* item = new QListWidgetItem(display, m_list);
                item->setData(Qt::UserRole, QString::fromStdString(id));
            }
        }
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        adjustSize();
    }

    void commitSelection(QListWidgetItem* item) {
        if (!item) return;
        const std::string id = item->data(Qt::UserRole).toString().toStdString();
        if (onSelected) onSelected(id);
        close();
    }

private:
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    std::vector<std::pair<std::string, std::string>> m_allItems;
};
