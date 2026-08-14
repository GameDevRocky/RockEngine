#pragma once

#include "engine/core/BuildConfig.hpp"

#include <QWidget>
#include <string>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTimer;

// The "File -> Build Game..." window: authors a BuildConfig and runs GameBuilder on it.
//
// A plain top-level QWidget, NOT a QDialog, and it is never exec()'d. That is a hard rule
// in this editor, documented on LoadingOverlay: exec() spins a nested QEventLoop, inside
// which the vsync frame loop keeps firing frameSwapped -> Editor::FrameTick ->
// Engine::Update -> JobSystem::Pump, re-entering the pump from inside a job step. Since
// building IS a job, that would be a re-entrancy bug aimed directly at itself.
//
// Settings live in Domain/sandbox/project.build (project data, committed with the repo);
// only the window geometry goes to QSettings (per-user machine state).
class BuildWindow : public QWidget {
    Q_OBJECT
public:
    static BuildWindow* Get();

    // Loads settings, then shows/raises. Safe to call repeatedly.
    void ShowCentered();

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    explicit BuildWindow(QWidget* parent = nullptr);

    void BuildUi();
    void LoadSettings();
    void SaveSettings();
    void RefreshSceneList();
    void RefreshSummary();
    void SetBusy(bool busy);
    void StartBuild(bool runAfter);

    // Pulls the current BuildConfig out of the widgets.
    BuildConfig ConfigFromUi() const;

    // Absolute path of the project.build file this window persists to.
    static std::string SettingsPath();

    QLineEdit*    m_gameName    = nullptr;
    QLineEdit*    m_version     = nullptr;
    QComboBox*    m_startupScene= nullptr;
    QSpinBox*     m_width       = nullptr;
    QSpinBox*     m_height      = nullptr;
    QCheckBox*    m_fullscreen  = nullptr;
    QCheckBox*    m_resizable   = nullptr;
    QCheckBox*    m_vsync       = nullptr;
    QComboBox*    m_platform    = nullptr;
    QLineEdit*    m_outputDir   = nullptr;
    QLabel*       m_summary     = nullptr;
    QLabel*       m_status      = nullptr;
    QProgressBar* m_progress    = nullptr;
    QPushButton*  m_buildBtn    = nullptr;
    QPushButton*  m_buildRunBtn = nullptr;
    QPushButton*  m_openBtn     = nullptr;

    // Polls JobSystem::SnapshotActive() while a build runs. A pull, matching how
    // LoadingOverlay is driven -- JobSystem deliberately has no Observable channel.
    QTimer* m_pollTimer = nullptr;

    std::string m_lastOutputDir;
    bool m_building = false;
};
