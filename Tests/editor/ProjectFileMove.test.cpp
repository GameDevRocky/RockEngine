#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QListView>
#include <QMimeData>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QUrl>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include "dock-widgets/FileExplorerGui.hpp"
#include "dock-widgets/FolderViewGui.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/FolderAssetModel.hpp"
#include "utils/ProjectFileDropTarget.hpp"
#include "utils/ProjectFileRenamer.hpp"

namespace {
bool waitFor(const std::function<bool()>& condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 3000) QTest::qWait(10);
    return condition();
}

QString sandbox() {
    return QString::fromStdString(EngineUtils::GetAssetPath("Domain/sandbox"));
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

Qt::DropAction dropFiles(QWidget* target, const QPoint& pos, const QMimeData& mime) {
    constexpr auto actions = Qt::CopyAction | Qt::MoveAction;
    QDragEnterEvent enter(pos, actions, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted()) return Qt::IgnoreAction;
    QDragMoveEvent move(pos, actions, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    if (!move.isAccepted()) {
        QDragLeaveEvent leave;
        QApplication::sendEvent(target, &leave);
        return Qt::IgnoreAction;
    }
    QDropEvent drop(pos, actions, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    return drop.isAccepted() ? drop.dropAction() : Qt::IgnoreAction;
}

QModelIndex folderIndex(QListView* view, const QString& path) {
    for (int row = 0; row < view->model()->rowCount(); ++row) {
        const QModelIndex index = view->model()->index(row, 0);
        if (index.data(FolderAssetModel::FilePathRole).toString() == path) return index;
    }
    return {};
}
}

class ProjectFileMoveTest : public ::testing::Test {
protected:
    QTemporaryDir temp{sandbox() + "/FileMovesXXXXXX"};
    QString base, destination;

    void SetUp() override {
        ASSERT_TRUE(temp.isValid());
        base = temp.path();
        destination = base + "/Destination";
        ASSERT_TRUE(QDir().mkpath(destination));
    }
    void TearDown() override {
        auto* folder = FolderViewGui::Get();
        folder->hide();
        folder->SetProjectDirectory(sandbox().toStdString());
    }
};

TEST_F(ProjectFileMoveTest, MovesPairedAssetAndPreservesIdsAndStoredPaths) {
    const QString source = base + "/Image.png";
    const QString descriptor = source + ".texture";
    ASSERT_TRUE(writeFile(source, "image bytes"));
    YAML::Node node;
    node["id"] = "file-move-texture";
    node["path"] = EngineUtils::ToAssetRelative(source.toStdString());
    node["sprites"][0]["id"] = "file-move-sprite";
    YAML::Emitter emitter;
    emitter << node;
    ASSERT_TRUE(writeFile(descriptor, emitter.c_str()));

    QString error;
    // Both URLs can appear in an OS selection; the pair moves only once.
    ASSERT_TRUE(ProjectFileRenamer::Move({descriptor, source}, destination, &error))
        << error.toStdString();
    EXPECT_FALSE(QFileInfo::exists(source));
    EXPECT_FALSE(QFileInfo::exists(descriptor));
    EXPECT_EQ(readFile(destination + "/Image.png"), "image bytes");
    const auto moved = YAML::LoadFile((destination + "/Image.png.texture").toStdString());
    EXPECT_EQ(moved["id"].as<std::string>(), "file-move-texture");
    EXPECT_EQ(moved["sprites"][0]["id"].as<std::string>(), "file-move-sprite");
    EXPECT_EQ(moved["path"].as<std::string>(),
              EngineUtils::ToAssetRelative((destination + "/Image.png").toStdString()));
}

TEST_F(ProjectFileMoveTest, CollisionRejectsWholeSelectionIncludingPairedSource) {
    const QString first = base + "/First.tilemap";
    const QString source = base + "/Image.png";
    ASSERT_TRUE(writeFile(first, "id: first\n"));
    ASSERT_TRUE(writeFile(source, "original"));
    ASSERT_TRUE(writeFile(source + ".texture", "id: texture\n"));
    ASSERT_TRUE(writeFile(destination + "/Image.png", "existing"));
    QString error;
    EXPECT_FALSE(ProjectFileRenamer::Move({first, source + ".texture"}, destination, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(first));
    EXPECT_TRUE(QFileInfo::exists(source + ".texture"));
    EXPECT_FALSE(QFileInfo::exists(destination + "/First.tilemap"));
    EXPECT_EQ(readFile(destination + "/Image.png"), "existing");
}

TEST_F(ProjectFileMoveTest, RejectsProtectedPathsNoOpsAndFolderCycles) {
    const QString source = base + "/File.tilemap";
    ASSERT_TRUE(writeFile(source, "id: file\n"));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({source}, base));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({base}, destination));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({destination}, destination));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({sandbox()}, destination));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({sandbox() + "/scripts"}, destination));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({sandbox() + "/project.build"}, destination));
    EXPECT_FALSE(ProjectFileRenamer::CanMove({source}, sandbox() + "/../lib"));
}

TEST_F(ProjectFileMoveTest, MovesFolderOnceAndUpdatesNestedPathsAndStartupScene) {
    const QString group = base + "/Group";
    ASSERT_TRUE(QDir().mkpath(group));
    const QString scene = group + "/Level.scene";
    const QString source = group + "/Sound.wav";
    ASSERT_TRUE(writeFile(scene, "id: level\ncomponents: []\n"));
    ASSERT_TRUE(writeFile(source, "sound"));
    YAML::Node descriptor;
    descriptor["id"] = "sound";
    descriptor["source_path"] = EngineUtils::ToAssetRelative(source.toStdString());
    YAML::Emitter emitter;
    emitter << descriptor;
    ASSERT_TRUE(writeFile(source + ".audio", emitter.c_str()));
    YAML::Node config;
    config["startupScene"] = EngineUtils::ToAssetRelative(scene.toStdString());
    YAML::Emitter build;
    build << config;
    ASSERT_TRUE(writeFile(base + "/project.build", build.c_str()));

    QString error;
    ASSERT_TRUE(ProjectFileRenamer::Move({scene, group}, destination, &error)) << error.toStdString();
    EXPECT_FALSE(QFileInfo::exists(group));
    EXPECT_TRUE(QFileInfo::exists(destination + "/Group/Level.scene"));
    const auto moved = YAML::LoadFile((destination + "/Group/Sound.wav.audio").toStdString());
    EXPECT_EQ(moved["source_path"].as<std::string>(), EngineUtils::ToAssetRelative(
        (destination + "/Group/Sound.wav").toStdString()));
    const auto updated = YAML::LoadFile((base + "/project.build").toStdString());
    EXPECT_EQ(updated["startupScene"].as<std::string>(), EngineUtils::ToAssetRelative(
        (destination + "/Group/Level.scene").toStdString()));
}

TEST_F(ProjectFileMoveTest, KeepsLoadedResourceAndSavePathAfterMoving) {
    const QString path = base + "/Map.tilemap";
    ASSERT_TRUE(writeFile(path, "type: TileMap\nid: file-move-map\ncells: []\n"));
    auto& assets = AssetManager::Get();
    assets.LoadAssetFromFile(path.toStdString());
    struct Cleanup { ~Cleanup() { AssetManager::Get().RemoveAsset("file-move-map"); } } cleanup;
    auto* map = assets.GetTileMap("file-move-map");
    ASSERT_NE(map, nullptr);
    QString error;
    ASSERT_TRUE(ProjectFileRenamer::Move({path}, destination, &error)) << error.toStdString();
    EXPECT_EQ(assets.GetTileMap("file-move-map"), map);
    EXPECT_EQ(map->GetFilePath(), (destination + "/Map.tilemap").toStdString());
    EXPECT_TRUE(assets.SaveResource(map));
    EXPECT_FALSE(QFileInfo::exists(path));
}

TEST_F(ProjectFileMoveTest, InvalidMovedYamlLeavesAllFilesInPlace) {
    const QString source = base + "/Image.png";
    ASSERT_TRUE(writeFile(source, "image"));
    ASSERT_TRUE(writeFile(source + ".texture", "path: [unterminated"));
    QString error;
    EXPECT_FALSE(ProjectFileRenamer::Move({source + ".texture"}, destination, &error));
    EXPECT_TRUE(QFileInfo::exists(source));
    EXPECT_EQ(readFile(source + ".texture"), "path: [unterminated");
    EXPECT_FALSE(QFileInfo::exists(destination + "/Image.png"));
}

TEST_F(ProjectFileMoveTest, RelocatesSerializedScriptModulesAndStillSupportsRename) {
    const QString script = base + "/Behavior.py";
    const QString module = "Domain.sandbox." + QFileInfo(base).fileName() + ".Behavior";
    ASSERT_TRUE(writeFile(script, "class Behavior: pass\n"));
    ASSERT_TRUE(writeFile(base + "/Level.scene",
        ("components:\n  - type: ScriptComponent\n    module: " + module + "\n").toUtf8()));
    QString error;
    ASSERT_TRUE(ProjectFileRenamer::Move({script}, destination, &error)) << error.toStdString();
    auto scene = YAML::LoadFile((base + "/Level.scene").toStdString());
    EXPECT_EQ(scene["components"][0]["module"].as<std::string>(),
              module.toStdString().substr(0, module.size() - 8) + "Destination.Behavior");
    ASSERT_TRUE(ProjectFileRenamer::Rename(destination + "/Behavior.py", "Renamed", &error))
        << error.toStdString();
    scene = YAML::LoadFile((base + "/Level.scene").toStdString());
    EXPECT_TRUE(scene["components"][0]["module"].as<std::string>().ends_with(".Destination.Renamed"));
}

TEST_F(ProjectFileMoveTest, DropsIntoFolderCellThenOutThroughAncestorBreadcrumb) {
    const QString path = base + "/Map.tilemap";
    ASSERT_TRUE(writeFile(path, "id: map\ncells: []\n"));
    auto* folder = FolderViewGui::Get();
    folder->resize(700, 400);
    folder->SetProjectDirectory(sandbox().toStdString());
    folder->Navigate(base.toStdString());
    folder->show();
    auto* grid = folder->findChild<QListView*>();
    ASSERT_NE(grid, nullptr);
    ASSERT_TRUE(waitFor([&] { return folderIndex(grid, destination).isValid(); }));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    EXPECT_EQ(dropFiles(grid->viewport(), grid->visualRect(folderIndex(grid, destination)).center(), mime),
              Qt::MoveAction);
    ASSERT_TRUE(QFileInfo::exists(destination + "/Map.tilemap"));
    folder->Navigate(destination.toStdString());
    QPushButton* parentCrumb = nullptr;
    for (auto* button : folder->findChildren<QPushButton*>())
        if (button->toolTip() == base) parentCrumb = button;
    ASSERT_NE(parentCrumb, nullptr);
    mime.setUrls({QUrl::fromLocalFile(destination + "/Map.tilemap")});
    EXPECT_EQ(dropFiles(parentCrumb, parentCrumb->rect().center(), mime), Qt::MoveAction);
    EXPECT_TRUE(QFileInfo::exists(path));
    EXPECT_FALSE(QFileInfo::exists(destination + "/Map.tilemap"));
}

TEST_F(ProjectFileMoveTest, DropsIntoFileExplorerDirectory) {
    const QString path = base + "/Map.tilemap";
    ASSERT_TRUE(writeFile(path, "id: map\ncells: []\n"));
    FileExplorerGui explorer;
    explorer.resize(400, 300);
    explorer.SetProjectDirectory(base);
    explorer.show();
    auto* tree = explorer.findChild<QTreeView*>();
    ASSERT_NE(tree, nullptr);
    auto* model = qobject_cast<QFileSystemModel*>(tree->model());
    ASSERT_NE(model, nullptr);
    ASSERT_TRUE(waitFor([&] { return model->rowCount(tree->rootIndex()) > 0; }));
    const QModelIndex index = model->index(destination);
    ASSERT_TRUE(index.isValid());
    ASSERT_TRUE(waitFor([&] { return !tree->visualRect(index).isEmpty(); }));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    EXPECT_EQ(dropFiles(tree->viewport(), tree->visualRect(index).center(), mime), Qt::MoveAction);
    EXPECT_TRUE(QFileInfo::exists(destination + "/Map.tilemap"));
    EXPECT_FALSE(QFileInfo::exists(path));
}

TEST_F(ProjectFileMoveTest, SpriteAndMixedSpriteDragsNeverMoveBackingFiles) {
    const QString path = base + "/Map.tilemap";
    ASSERT_TRUE(writeFile(path, "id: map\n"));
    QWidget surface;
    new ProjectFileDropTarget(&surface, [this](const QPoint&) {
        return ProjectFileDropTarget::Target{destination, {}};
    });
    QMimeData mime;
    mime.setData(kSpriteMimeType, "sprite-id");
    EXPECT_EQ(dropFiles(&surface, {4, 4}, mime), Qt::IgnoreAction);
    mime.setUrls({QUrl::fromLocalFile(path)});
    EXPECT_EQ(dropFiles(&surface, {4, 4}, mime), Qt::IgnoreAction);
    EXPECT_TRUE(QFileInfo::exists(path));
}

TEST_F(ProjectFileMoveTest, ExternalAssetDropImportsACopy) {
    QTemporaryDir external;
    ASSERT_TRUE(external.isValid());
    const QString source = external.path() + "/Clip.wav";
    // A mono 16-bit PCM WAV containing one silent sample; no GL context needed.
    const QByteArray wave = QByteArray::fromHex(
        "524946462600000057415645666d74201000000001000100401f0000803e00000200100064617461020000000000");
    ASSERT_TRUE(writeFile(source, wave));
    QWidget surface;
    new ProjectFileDropTarget(&surface, [this](const QPoint&) {
        return ProjectFileDropTarget::Target{destination, {}};
    });
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(source)});
    EXPECT_EQ(dropFiles(&surface, {4, 4}, mime), Qt::CopyAction);
    EXPECT_EQ(readFile(source), wave);
    EXPECT_EQ(readFile(destination + "/Clip.wav"), wave);
    const QString descriptor = destination + "/Clip.wav.audio";
    ASSERT_TRUE(QFileInfo::exists(descriptor));
    const auto node = YAML::LoadFile(descriptor.toStdString());
    AssetManager::Get().RemoveAsset(node["id"].as<std::string>());
}
