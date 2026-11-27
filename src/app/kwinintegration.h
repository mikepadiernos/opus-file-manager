// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "kwinscript.h"

#include <KSharedConfig>
#include <QJsonArray>
#include <QObject>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>

/**
 * What KWinIntegration needs from KWin, KGlobalAccel and KPackage, so tests
 * can replace it.
 */
class KWinBackend
{
public:
    using Callback = std::function<void(const QString &error)>;
    virtual ~KWinBackend() = default;

    virtual bool kwinRunning() const = 0;
    /// Whether KWin has loaded the script, or std::nullopt when scripting is unavailable.
    virtual std::optional<bool> scriptLoaded() const = 0;
    virtual int installedVersion() const = 0;
    virtual bool pluginEnabled() const = 0;
    /// Writes only the script's own entry in kwinrc's [Plugins] group.
    virtual void setPluginEnabled(bool enabled) = 0;
    virtual void forgetPluginEnabled() = 0;
    /// Loads the enabled scripts that are not loaded yet.
    virtual void startScripts() = 0;
    virtual void unloadScript() = 0;
    virtual void invokeProcessShortcut() = 0;
    virtual void installPackage(const QString &sourceDirectory, bool update, const Callback &done) = 0;
    virtual void removePackage(const Callback &done) = 0;
};

class SystemKWinBackend : public KWinBackend
{
public:
    bool kwinRunning() const override;
    std::optional<bool> scriptLoaded() const override;
    int installedVersion() const override;
    bool pluginEnabled() const override;
    void setPluginEnabled(bool enabled) override;
    void forgetPluginEnabled() override;
    void startScripts() override;
    void unloadScript() override;
    void invokeProcessShortcut() override;
    void installPackage(const QString &sourceDirectory, bool update, const Callback &done) override;
    void removePackage(const Callback &done) override;
};

/**
 * Installs, enables, updates and removes the optional Plasma Opus KWin script
 * on explicit request, and serves its `org.kde.opusfilemanager.KWin1` D-Bus
 * interface: it answers the script's Sync calls with the options and pending
 * commands, and receives window placements, virtual desktops and screen-edge
 * activations from it.
 */
class KWinIntegration : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.opusfilemanager.KWin1")

public:
    KWinIntegration(std::unique_ptr<KWinBackend> backend, KSharedConfigPtr config, KSharedConfigPtr stateConfig, QObject *parent = nullptr);
    ~KWinIntegration() override;

    /// Exports the interface on the session bus under the application's own name.
    bool registerOnBus();

    KWinBackend *backend() const;
    /// Replaces the backend, for tests.
    void setBackend(std::unique_ptr<KWinBackend> backend);
    KWinScript::State state() const;
    KWinScript::Status status() const;
    /// A sentence describing status() for the user.
    QString statusText() const;
    /// The version bundled with this build.
    static int bundledVersion();
    /// Writes the bundled package to @p directory and returns its path, or an empty string.
    static QString extractPackage(const QString &directory);

    KWinScript::Options options() const;
    void setOptions(const KWinScript::Options &options);
    KWinScript::DropDown dropDown() const;
    void setDropDown(const KWinScript::DropDown &dropDown);
    QMap<QString, QString> desktopWorkspaces() const;
    void setDesktopWorkspaces(const QMap<QString, QString> &assignments);
    QList<KWinScript::Desktop> desktops() const;

    /// Supplies the current workspace id, under which reported placements are kept.
    void setWorkspaceProvider(std::function<QString()> provider);
    /// Asks the script to restore the placement kept for @p workspaceId.
    bool restoreWorkspace(const QString &workspaceId);
    std::optional<KWinScript::Placement> placement(const QString &workspaceId) const;
    /// Asks the script to process pending requests, once the event loop runs.
    void requestSync();
    /// Whether the script answers, so window management requests take effect.
    bool scriptActive() const;

    /// Installs or updates the bundled script, then enables it.
    void install();
    void remove();
    void setEnabled(bool enabled);
    /// Reads KWin's state again and notifies listeners.
    void refresh();

public Q_SLOTS:
    Q_SCRIPTABLE QString Sync(const QString &scriptVersion);
    Q_SCRIPTABLE void WindowChanged(const QString &placement);
    Q_SCRIPTABLE void DesktopsChanged(const QString &desktops);
    Q_SCRIPTABLE void CurrentDesktopChanged(const QString &desktopId);
    Q_SCRIPTABLE void EdgeActivated();

Q_SIGNALS:
    void stateChanged();
    void desktopsChanged();
    /// The current virtual desktop has an assigned workspace, which should be applied.
    void desktopWorkspaceRequested(const QString &workspaceId);
    void edgeActivated();
    void operationFinished(const QString &error);

private:
    void finishOperation(const QString &error);

    std::unique_ptr<KWinBackend> m_backend;
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_stateConfig;
    std::function<QString()> m_workspaceProvider;
    QList<KWinScript::Desktop> m_desktops;
    QJsonArray m_commands;
    QTimer m_syncTimer;
    int m_runningVersion = -1;
    bool m_busy = false;
};
