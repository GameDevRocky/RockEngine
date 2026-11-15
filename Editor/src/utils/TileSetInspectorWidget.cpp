#include "utils/TileSetInspectorWidget.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <unordered_set>

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "engine/tilemap/TileSet.hpp"
#include "utils/AssetThumbnails.hpp"
#include "utils/TileCollisionShapeEditor.hpp"
#include "utils/ProperyFactory.hpp"
#include "utils/RefDropFilter.hpp"

namespace {
constexpr std::array<const char*, 8> kDirections = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"
};
constexpr std::array<std::pair<int, int>, 8> kPositions = {
    std::pair{0, 1}, {0, 2}, {1, 2}, {2, 2},
    {2, 1}, {2, 0}, {1, 0}, {0, 0}
};

void MakeBold(QLabel* label) {
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
}

class RuleComboBox final : public QComboBox {
public:
    using QComboBox::QComboBox;

    QSize minimumSizeHint() const override {
        QSize hint = QComboBox::minimumSizeHint();
        hint.setWidth(0);
        return hint;
    }
};

QWidget* FieldRow(const QString& label, QWidget* editor, QWidget* parent,
                  int labelWidth = 108) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* caption = new QLabel(label, row);
    if (labelWidth > 0) caption->setMinimumWidth(labelWidth);
    MakeBold(caption);
    layout->addWidget(caption);
    layout->addWidget(editor, 1);
    return row;
}

class TileEntryWidget final : public PropertyWidget<TileSetTileEntry> {
public:
    explicit TileEntryWidget(std::function<void(TileId)> onSelected)
        : m_onSelected(std::move(onSelected)) {
        m_button = new QPushButton();
        m_button->setObjectName("TileSetTileSelect");
        m_button->setCheckable(true);
        m_button->setFixedHeight(38);
        m_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_button->setStyleSheet(
            "QPushButton#TileSetTileSelect { padding: 0; }"
            "QPushButton#TileSetTileSelect:checked { border: 1px solid #60a960; background: #355038; }");
        auto* content = new QHBoxLayout(m_button);
        content->setContentsMargins(8, 3, 8, 3);
        content->setSpacing(8);
        m_name = new QLabel(m_button);
        m_name->setObjectName("TileSetTileNameLabel");
        MakeBold(m_name);
        m_name->setStyleSheet("QLabel#TileSetTileNameLabel { background: transparent; border: none; }");
        m_name->setAttribute(Qt::WA_TransparentForMouseEvents);
        content->addWidget(m_name, 1);
        m_thumbnail = new QLabel(m_button);
        m_thumbnail->setObjectName("TileSetTileThumbnail");
        m_thumbnail->setFixedSize(30, 30);
        m_thumbnail->setAlignment(Qt::AlignCenter);
        m_thumbnail->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_thumbnail->setStyleSheet("background: #1d1d1d; border: 1px solid #444;");
        content->addWidget(m_thumbnail);
        QObject::connect(m_button, &QPushButton::clicked, [this] {
            if (m_value.id != EMPTY_TILE_ID) m_onSelected(m_value.id);
        });
    }

    QWidget* GetWidget() override { return m_button; }
    bool IsValid() override { return m_button != nullptr; }
    TileSetTileEntry GetValue() override { return m_value; }
    void SetValue(const TileSetTileEntry& value) override {
        m_value = value;
        m_name->setText(QString::fromStdString(value.name));
        m_button->setToolTip(QString("Tile ID %1").arg(value.id));
        m_thumbnail->clear();
        if (!value.defaultSpriteId.empty()) {
            const QPixmap pixmap = AssetThumbnails::forSprite(value.defaultSpriteId);
            if (!pixmap.isNull())
                m_thumbnail->setPixmap(pixmap.scaled(28, 28, Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation));
        }
        QSignalBlocker blocker(m_button);
        m_button->setChecked(value.selected);
    }

private:
    std::function<void(TileId)> m_onSelected;
    TileSetTileEntry m_value;
    QPushButton* m_button = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_thumbnail = nullptr;
};

class RuleEntryWidget final : public PropertyWidget<TileSetRuleEntry> {
public:
    RuleEntryWidget() {
        m_card = new QWidget();
        m_card->setObjectName("TileSetRuleCard");
        m_card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto* cardLayout = new QVBoxLayout(m_card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(6);
        auto* rowHost = new QWidget(m_card);
        rowHost->setObjectName("TileRuleRow");
        cardLayout->addWidget(rowHost);
        auto* visual = new QHBoxLayout(rowHost);
        visual->setContentsMargins(0, 0, 0, 0);
        visual->setSpacing(8);

        auto* properties = new QWidget(m_card);
        properties->setObjectName("TileRuleProperties");
        auto* propertyLayout = new QVBoxLayout(properties);
        propertyLayout->setContentsMargins(0, 0, 0, 0);
        propertyLayout->setSpacing(5);
        propertyLayout->setAlignment(Qt::AlignTop);
        visual->addWidget(properties, 1);

        auto* visuals = new QWidget(m_card);
        visuals->setObjectName("TileRuleVisuals");
        auto* visualLayout = new QHBoxLayout(visuals);
        visualLayout->setContentsMargins(0, 0, 0, 0);
        visualLayout->setSpacing(8);
        visual->addWidget(visuals, 0, Qt::AlignTop);

        m_customEditor = new TileCollisionShapeEditor(m_card);
        m_customEditor->setObjectName("TileRuleCustomCollisionEditor");
        cardLayout->addWidget(m_customEditor, 0, Qt::AlignRight);
        m_customEditor->onChanged = [this](const TileCollisionPolygon& polygon) {
            m_value.customPolygon = polygon;
            if (onChanged) onChanged(m_value);
        };

        m_preview = new QLabel(visuals);
        m_preview->setObjectName("TileSetRulePreview");
        MakeBold(m_preview);
        m_preview->setFixedSize(72, 72);
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setStyleSheet(
            "QLabel#TileSetRulePreview { background: #1d1d1d; border: 1px solid #555; }");
        m_preview->setToolTip("Drop a sprite here; right-click to clear");
        m_preview->setAcceptDrops(true);
        m_preview->installEventFilter(new RefDropFilter(
            Properties::PropDesc().Tag(Properties::Tags::SPRITE),
            [this](const std::string& spriteId) { ChangeSprite(spriteId); }, m_preview));
        m_preview->setContextMenuPolicy(Qt::ActionsContextMenu);
        auto* clearSprite = new QAction("Clear Sprite", m_preview);
        m_preview->addAction(clearSprite);
        QObject::connect(clearSprite, &QAction::triggered, m_preview,
                         [this] { ChangeSprite({}); });
        visualLayout->addWidget(m_preview, 0, Qt::AlignVCenter);

        auto* gridHost = new QWidget(visuals);
        gridHost->setObjectName("TileRuleNeighbors");
        auto* grid = new QGridLayout(gridHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(2);
        for (std::size_t slot = 0; slot < m_cells.size(); ++slot) {
            auto* cell = new QToolButton(gridHost);
            cell->setObjectName(QString("TileRuleNeighbor%1").arg(kDirections[slot]));
            cell->setFixedSize(30, 30);
            cell->setPopupMode(QToolButton::InstantPopup);
            m_cells[slot] = cell;
            grid->addWidget(cell, kPositions[slot].first, kPositions[slot].second);
        }
        auto* center = new QLabel("Tile", gridHost);
        MakeBold(center);
        center->setAlignment(Qt::AlignCenter);
        center->setFixedSize(30, 30);
        center->setStyleSheet("background: #454545; color: #ccc; border-radius: 3px;");
        grid->addWidget(center, 1, 1);
        visualLayout->addWidget(gridHost, 0, Qt::AlignVCenter);

        m_output = new RuleComboBox(properties);
        m_output->setObjectName("TileRuleOutput");
        m_output->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_output->addItem("Fixed", static_cast<int>(TileOutputType::Fixed));
        m_output->addItem("Random", static_cast<int>(TileOutputType::Random));
        m_output->addItem("Animation", static_cast<int>(TileOutputType::Animation));
        propertyLayout->addWidget(FieldRow("Output", m_output, properties, 0));
        QObject::connect(m_output, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         [this](int index) { ChangeOutput(index); });

        m_collision = new RuleComboBox(properties);
        m_collision->setObjectName("TileRuleCollision");
        m_collision->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_collision->addItem("None", static_cast<int>(TileCollisionType::None));
        m_collision->addItem("Sprite", static_cast<int>(TileCollisionType::Sprite));
        m_collision->addItem("Grid", static_cast<int>(TileCollisionType::Grid));
        m_collision->addItem("Custom", static_cast<int>(TileCollisionType::Custom));
        propertyLayout->insertWidget(0, FieldRow("Collider", m_collision, properties, 0));
        QObject::connect(m_collision, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         [this](int index) {
            if (index < 0) return;
            m_value.collisionType = static_cast<TileCollisionType>(m_collision->itemData(index).toInt());
            m_customEditor->setVisible(m_value.collisionType == TileCollisionType::Custom);
            if (onChanged) onChanged(m_value);
        });

        Properties::PropDesc spriteListDesc;
        spriteListDesc.Tag(Properties::Tags::LIST).Element(
            Properties::PropDesc().Tag(Properties::Tags::SPRITE)
                                  .RefType(Properties::Tags::OBJECT_REF));
        m_spriteList.reset(PropertyFactory::Create<std::vector<std::string>>(spriteListDesc));
        m_spriteList->GetWidget()->setObjectName("TileRuleSpriteList");
        m_spriteList->onChanged = [this](const std::vector<std::string>& spriteIds) {
            m_value.spriteIds = spriteIds;
            UpdateCustomSprite();
            if (onChanged) onChanged(m_value);
        };
        propertyLayout->addWidget(m_spriteList->GetWidget());

        m_speedHost = new QWidget(properties);
        auto* speedLayout = new QHBoxLayout(m_speedHost);
        speedLayout->setContentsMargins(0, 0, 0, 0);
        speedLayout->setSpacing(4);
        auto* speedLabel = new QLabel("Speed", m_speedHost);
        MakeBold(speedLabel);
        speedLayout->addWidget(speedLabel);
        m_minSpeed = new QDoubleSpinBox(m_speedHost);
        m_minSpeed->setObjectName("TileRuleMinSpeed");
        m_minSpeed->setRange(0.01, 100.0);
        m_minSpeed->setValue(1.0);
        m_minSpeed->setSuffix("x min");
        m_maxSpeed = new QDoubleSpinBox(m_speedHost);
        m_maxSpeed->setObjectName("TileRuleMaxSpeed");
        m_maxSpeed->setRange(0.01, 100.0);
        m_maxSpeed->setValue(1.0);
        m_maxSpeed->setSuffix("x max");
        speedLayout->addWidget(m_minSpeed);
        speedLayout->addWidget(m_maxSpeed);
        propertyLayout->addWidget(m_speedHost);
        auto speedChanged = [this] {
            m_value.minAnimationSpeed = static_cast<float>(m_minSpeed->value());
            m_value.maxAnimationSpeed = static_cast<float>(m_maxSpeed->value());
            if (onChanged) onChanged(m_value);
        };
        QObject::connect(m_minSpeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), speedChanged);
        QObject::connect(m_maxSpeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), speedChanged);

    }

    QWidget* GetWidget() override { return m_card; }
    bool IsValid() override { return m_card != nullptr; }
    TileSetRuleEntry GetValue() override { return m_value; }
    void SetValue(const TileSetRuleEntry& value) override {
        m_value = value;
        QSignalBlocker outputBlock(m_output), collisionBlock(m_collision),
            minSpeedBlock(m_minSpeed), maxSpeedBlock(m_maxSpeed);
        m_output->setCurrentIndex(m_output->findData(static_cast<int>(value.outputType)));
        m_collision->setCurrentIndex(m_collision->findData(static_cast<int>(value.collisionType)));
        m_spriteList->SetValue(value.spriteIds);
        m_minSpeed->setValue(value.minAnimationSpeed);
        m_maxSpeed->setValue(value.maxAnimationSpeed);
        UpdatePreview();
        UpdateCustomSprite();
        UpdateOutputVisibility();
        m_customEditor->SetPolygon(value.customPolygon);
        m_customEditor->setVisible(value.collisionType == TileCollisionType::Custom);

        for (std::size_t slot = 0; slot < m_cells.size(); ++slot) {
            auto* cell = m_cells[slot];
            UpdateCell(slot);
            if (QMenu* old = cell->menu()) {
                cell->setMenu(nullptr);
                delete old;
            }
            auto* menu = new QMenu(cell);
            auto* any = menu->addAction("Any (ignore neighbor)");
            QObject::connect(any, &QAction::triggered, cell,
                             [this, slot] { ChangeNeighbor(slot, std::nullopt); });
            auto* empty = menu->addAction("Empty (ID 0)");
            QObject::connect(empty, &QAction::triggered, cell,
                             [this, slot] { ChangeNeighbor(slot, EMPTY_TILE_ID); });
            if (!value.options.empty()) menu->addSeparator();
            for (const auto& option : value.options) {
                QIcon icon;
                if (!option.defaultSpriteId.empty())
                    icon = QIcon(AssetThumbnails::forSprite(option.defaultSpriteId));
                auto* action = menu->addAction(icon,
                    QString("%1 (ID %2)").arg(QString::fromStdString(option.name)).arg(option.id));
                QObject::connect(action, &QAction::triggered, cell,
                    [this, slot, id = option.id] { ChangeNeighbor(slot, id); });
            }
            cell->setMenu(menu);
        }
    }

private:
    void ChangeOutput(int index) {
        if (index < 0) return;
        const auto output = static_cast<TileOutputType>(m_output->itemData(index).toInt());
        if (m_value.outputType == output) return;
        if (output == TileOutputType::Fixed && m_value.spriteId.empty() && !m_value.spriteIds.empty())
            m_value.spriteId = m_value.spriteIds.front();
        if (output != TileOutputType::Fixed && m_value.spriteIds.empty() && !m_value.spriteId.empty())
            m_value.spriteIds.push_back(m_value.spriteId);
        m_value.outputType = output;
        m_spriteList->SetValue(m_value.spriteIds);
        UpdatePreview();
        UpdateCustomSprite();
        UpdateOutputVisibility();
        if (onChanged) onChanged(m_value);
    }

    void UpdateOutputVisibility() {
        const bool fixed = m_value.outputType == TileOutputType::Fixed;
        m_preview->setVisible(fixed);
        m_spriteList->GetWidget()->setVisible(!fixed);
        m_speedHost->setVisible(m_value.outputType == TileOutputType::Animation);
    }

    void ChangeSprite(const std::string& spriteId) {
        if (m_value.spriteId == spriteId) return;
        m_value.spriteId = spriteId;
        UpdatePreview();
        UpdateCustomSprite();
        if (onChanged) onChanged(m_value);
    }

    void UpdatePreview() {
        m_preview->clear();
        if (m_value.spriteId.empty()) {
            m_preview->setText("None");
            return;
        }
        const QPixmap pixmap = AssetThumbnails::forSprite(m_value.spriteId);
        if (pixmap.isNull()) m_preview->setText("Missing");
        else m_preview->setPixmap(pixmap.scaled(66, 66, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
    }

    void UpdateCustomSprite() {
        const std::string& spriteId = m_value.outputType == TileOutputType::Fixed
            ? m_value.spriteId
            : (m_value.spriteIds.empty() ? m_value.spriteId : m_value.spriteIds.front());
        m_customEditor->SetSprite(spriteId.empty() ? m_value.fallbackSpriteId : spriteId);
    }

    void UpdateCell(std::size_t slot) {
        auto* cell = m_cells[slot];
        const auto id = m_value.mask[slot];
        cell->setText(!id ? "*" : *id == EMPTY_TILE_ID ? "×" : QString::number(*id));
        cell->setStyleSheet(!id
            ? "QToolButton { color: #ccc; font-weight: bold; }"
            : *id == EMPTY_TILE_ID
                ? "QToolButton { color: #e67b7b; font-weight: bold; }"
                : "QToolButton { color: #82d582; font-weight: bold; }");
        cell->setToolTip(!id
            ? QString("%1 neighbor: Any").arg(kDirections[slot])
            : QString("%1 neighbor: exact tile ID %2").arg(kDirections[slot]).arg(*id));
    }

    void ChangeNeighbor(std::size_t slot, std::optional<TileId> id) {
        if (m_value.mask[slot] == id) return;
        m_value.mask[slot] = id;
        UpdateCell(slot);
        if (onChanged) onChanged(m_value);
    }

    TileSetRuleEntry m_value;
    QWidget* m_card = nullptr;
    QLabel* m_preview = nullptr;
    TileCollisionShapeEditor* m_customEditor = nullptr;
    QComboBox* m_output = nullptr;
    QComboBox* m_collision = nullptr;
    QWidget* m_speedHost = nullptr;
    QDoubleSpinBox* m_minSpeed = nullptr;
    QDoubleSpinBox* m_maxSpeed = nullptr;
    std::unique_ptr<PropertyWidget<std::vector<std::string>>> m_spriteList;
    std::array<QToolButton*, 8> m_cells{};
};
}

template<>
PropertyWidget<TileSetTileEntry>* PropertyFactory::Create<TileSetTileEntry>(
    const Properties::PropDesc&) {
    return new TileEntryWidget([](TileId) {});
}

template<>
PropertyWidget<TileSetRuleEntry>* PropertyFactory::Create<TileSetRuleEntry>(
    const Properties::PropDesc&) {
    return new RuleEntryWidget();
}

TileSetInspectorWidget::TileSetInspectorWidget(TileSet* tileSet, QWidget* parent)
    : QWidget(parent), m_tileSet(tileSet)
{
    setObjectName("TileSetInspector");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* tileTitle = new QLabel("Tiles", this);
    MakeBold(tileTitle);
    layout->addWidget(tileTitle);

    m_tiles.reset(new ListPropertyWidget<TileSetTileEntry>(
        Properties::PropDesc().Tag(Properties::Tags::LIST),
        [this] {
            return new TileEntryWidget([this](TileId id) {
                SelectTile(id);
                QueueRefresh();
            });
        }, false));
    m_tiles->GetWidget()->setObjectName("TileSetTileList");
    m_tiles->onChanged = [this](const auto& entries) { HandleTileListChanged(entries); };
    layout->addWidget(m_tiles->GetWidget());

    m_tileFields = new QWidget(this);
    auto* fields = new QVBoxLayout(m_tileFields);
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setSpacing(7);
    layout->addWidget(m_tileFields);

    m_idField = new QSpinBox(m_tileFields);
    m_idField->setObjectName("TileSetTileId");
    m_idField->setRange(1, std::numeric_limits<TileId>::max());
    fields->addWidget(FieldRow("Tile ID", m_idField, m_tileFields));
    connect(m_idField, &QSpinBox::editingFinished, this, [this] { CommitTileId(); });

    m_nameField = new QLineEdit(m_tileFields);
    m_nameField->setObjectName("TileSetTileName");
    fields->addWidget(FieldRow("Name", m_nameField, m_tileFields));
    connect(m_nameField, &QLineEdit::editingFinished, this, [this] { CommitTileName(); });

    m_defaultSprite.reset(PropertyFactory::Create<std::string>(
        Properties::PropDesc().Tag(Properties::Tags::SPRITE)
                              .RefType(Properties::Tags::OBJECT_REF)));
    m_defaultSprite->GetWidget()->setObjectName("TileSetDefaultSprite");
    fields->addWidget(FieldRow("Default Sprite", m_defaultSprite->GetWidget(), m_tileFields));
    if (QWidget* detail = m_defaultSprite->GetFullRowWidget()) fields->addWidget(detail);
    m_defaultSprite->onChanged = [this](const std::string& spriteId) {
        const Tile* current = m_tileSet->GetTile(m_selectedTileId);
        if (!current || current->defaultSpriteId == spriteId) return;
        Tile tile = *current;
        tile.defaultSpriteId = spriteId;
        m_defaultCustomEditor->SetSprite(spriteId);
        m_tileSet->SetTile(m_selectedTileId, std::move(tile));
        QueueRefresh();
    };

    m_collisionType = new QComboBox(m_tileFields);
    m_collisionType->setObjectName("TileSetCollisionType");
    m_collisionType->addItem("None", static_cast<int>(TileCollisionType::None));
    m_collisionType->addItem("Sprite", static_cast<int>(TileCollisionType::Sprite));
    m_collisionType->addItem("Grid", static_cast<int>(TileCollisionType::Grid));
    m_collisionType->addItem("Custom", static_cast<int>(TileCollisionType::Custom));
    fields->addWidget(FieldRow("Default Collider", m_collisionType, m_tileFields));
    connect(m_collisionType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        const Tile* current = m_tileSet->GetTile(m_selectedTileId);
        if (!current || index < 0) return;
        const auto collisionType = static_cast<TileCollisionType>(
            m_collisionType->itemData(index).toInt());
        if (current->collisionType == collisionType) return;
        Tile tile = *current;
        tile.collisionType = collisionType;
        m_tileSet->SetTile(m_selectedTileId, std::move(tile));
        m_defaultCustomEditor->setVisible(collisionType == TileCollisionType::Custom);
    });

    m_defaultCustomEditor = new TileCollisionShapeEditor(m_tileFields);
    m_defaultCustomEditor->setObjectName("TileSetDefaultCustomCollisionEditor");
    fields->addWidget(m_defaultCustomEditor, 0, Qt::AlignRight);
    m_defaultCustomEditor->onChanged = [this](const TileCollisionPolygon& polygon) {
        const Tile* current = m_tileSet->GetTile(m_selectedTileId);
        if (!current) return;
        Tile tile = *current;
        tile.customPolygon = polygon;
        m_tileSet->SetTile(m_selectedTileId, std::move(tile));
    };

    auto* rulesTitle = new QLabel("Tiling Rules", m_tileFields);
    MakeBold(rulesTitle);
    fields->addWidget(rulesTitle);

    m_rules.reset(new ListPropertyWidget<TileSetRuleEntry>(
        Properties::PropDesc().Tag(Properties::Tags::LIST),
        [] { return new RuleEntryWidget(); }));
    m_rules->GetWidget()->setObjectName("TileSetRulesList");
    m_rules->onChanged = [this](const auto& entries) { HandleRuleListChanged(entries); };
    fields->addWidget(m_rules->GetWidget());

    Refresh();
}

void TileSetInspectorWidget::Refresh() { RefreshTiles(); }

void TileSetInspectorWidget::RefreshTiles()
{
    std::vector<TileId> ids;
    for (const auto& [id, tile] : m_tileSet->GetTiles()) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    const TileId selected = m_tileSet->GetTile(m_selectedTileId)
        ? m_selectedTileId : (ids.empty() ? EMPTY_TILE_ID : ids.front());
    std::vector<TileSetTileEntry> entries;
    for (TileId id : ids) {
        const Tile& tile = *m_tileSet->GetTile(id);
        entries.push_back({id, tile.name, tile.defaultSpriteId, id == selected});
    }
    m_tiles->SetValue(entries);
    SelectTile(selected);
}

void TileSetInspectorWidget::SelectTile(TileId id)
{
    m_selectedTileId = id;
    const Tile* tile = m_tileSet->GetTile(id);
    m_tileFields->setVisible(tile != nullptr);
    if (!tile) {
        m_rules->SetValue({});
        return;
    }
    QSignalBlocker idBlock(m_idField), nameBlock(m_nameField), collisionBlock(m_collisionType);
    m_idField->setValue(id);
    m_nameField->setText(QString::fromStdString(tile->name));
    m_defaultSprite->SetValue(tile->defaultSpriteId);
    const int collisionIndex = m_collisionType->findData(static_cast<int>(tile->collisionType));
    m_collisionType->setCurrentIndex(collisionIndex >= 0 ? collisionIndex : 0);
    m_defaultCustomEditor->SetSprite(tile->defaultSpriteId);
    m_defaultCustomEditor->SetPolygon(tile->customPolygon);
    m_defaultCustomEditor->setVisible(tile->collisionType == TileCollisionType::Custom);
    RefreshRules();
}

void TileSetInspectorWidget::RefreshRules()
{
    const Tile* tile = m_tileSet->GetTile(m_selectedTileId);
    if (!tile) return;
    std::vector<TileSetTileOption> options;
    for (const auto& [id, candidate] : m_tileSet->GetTiles())
        options.push_back({id, candidate.name, candidate.defaultSpriteId});
    std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });
    std::vector<TileSetRuleEntry> rules;
    for (const AutoTileRule& rule : tile->autoTileRules) {
        TileSetRuleEntry entry;
        entry.mask = rule.neighbors;
        entry.spriteId = rule.spriteId;
        entry.fallbackSpriteId = tile->defaultSpriteId;
        entry.collisionType = rule.collisionType;
        entry.customPolygon = rule.customPolygon;
        entry.outputType = rule.outputType;
        entry.spriteIds = rule.spriteIds;
        entry.minAnimationSpeed = rule.minAnimationSpeed;
        entry.maxAnimationSpeed = rule.maxAnimationSpeed;
        entry.ownerId = m_selectedTileId;
        entry.options = options;
        entry.newlyAdded = false;
        rules.push_back(std::move(entry));
    }
    m_rules->SetValue(rules);
}

void TileSetInspectorWidget::QueueRefresh()
{
    if (m_refreshQueued) return;
    m_refreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshQueued = false;
        RefreshTiles();
    });
}

void TileSetInspectorWidget::HandleTileListChanged(const std::vector<TileSetTileEntry>& entries)
{
    std::unordered_set<TileId> retained;
    for (const auto& entry : entries)
        if (entry.id != EMPTY_TILE_ID) retained.insert(entry.id);
    std::vector<TileId> removed;
    for (const auto& [id, tile] : m_tileSet->GetTiles())
        if (!retained.contains(id)) removed.push_back(id);
    for (TileId id : removed) m_tileSet->RemoveTile(id);
    if (!removed.empty() && !m_tileSet->GetTile(m_selectedTileId))
        m_selectedTileId = EMPTY_TILE_ID;

    for (const auto& entry : entries) {
        if (entry.id != EMPTY_TILE_ID) continue;
        TileId id = 1;
        while (id < std::numeric_limits<TileId>::max() && m_tileSet->GetTile(id)) ++id;
        if (m_tileSet->GetTile(id)) break;
        Tile tile;
        tile.name = "Tile " + std::to_string(id);
        m_tileSet->SetTile(id, std::move(tile));
        m_selectedTileId = id;
    }
    QueueRefresh();
}

void TileSetInspectorWidget::HandleRuleListChanged(const std::vector<TileSetRuleEntry>& entries)
{
    const Tile* current = m_tileSet->GetTile(m_selectedTileId);
    if (!current) return;
    std::vector<AutoTileRule> rules;
    auto hasPattern = [&rules](const NeighborPattern& pattern) {
        return std::any_of(rules.begin(), rules.end(), [&](const AutoTileRule& rule) {
            return rule.neighbors == pattern;
        });
    };
    for (const auto& entry : entries) {
        if (entry.ownerId != EMPTY_TILE_ID && entry.ownerId != m_selectedTileId) {
            QueueRefresh();
            return;
        }
        NeighborPattern mask = entry.mask;
        if (entry.newlyAdded) {
            while (hasPattern(mask)) {
                const TileId north = mask[0].value_or(EMPTY_TILE_ID);
                if (north == std::numeric_limits<TileId>::max()) {
                    QueueRefresh();
                    return;
                }
                mask[0] = north + 1;
            }
        } else if (hasPattern(mask)) {
            QueueRefresh();
            return;
        }
        AutoTileRule rule;
        rule.neighbors = mask;
        rule.spriteId = entry.spriteId;
        rule.collisionType = entry.newlyAdded ? current->collisionType : entry.collisionType;
        rule.customPolygon = entry.newlyAdded ? current->customPolygon : entry.customPolygon;
        rule.outputType = entry.outputType;
        rule.spriteIds = entry.spriteIds;
        rule.minAnimationSpeed = std::max(0.01f, entry.minAnimationSpeed);
        rule.maxAnimationSpeed = std::max(0.01f, entry.maxAnimationSpeed);
        rules.push_back(std::move(rule));
    }
    if (rules != current->autoTileRules) {
        Tile tile = *current;
        tile.autoTileRules = std::move(rules);
        m_tileSet->SetTile(m_selectedTileId, std::move(tile));
    }
    QueueRefresh();
}

void TileSetInspectorWidget::CommitTileId()
{
    const TileId next = static_cast<TileId>(m_idField->value());
    if (next == m_selectedTileId) return;
    const Tile* current = m_tileSet->GetTile(m_selectedTileId);
    if (!current || m_tileSet->GetTile(next)) {
        m_idField->setValue(m_selectedTileId);
        return;
    }
    Tile tile = *current;
    m_tileSet->RemoveTile(m_selectedTileId);
    m_tileSet->SetTile(next, std::move(tile));
    m_selectedTileId = next;
    QueueRefresh();
}

void TileSetInspectorWidget::CommitTileName()
{
    const Tile* current = m_tileSet->GetTile(m_selectedTileId);
    if (!current) return;
    const std::string name = m_nameField->text().toStdString();
    if (current->name == name) return;
    Tile tile = *current;
    tile.name = name;
    m_tileSet->SetTile(m_selectedTileId, std::move(tile));
    QueueRefresh();
}
