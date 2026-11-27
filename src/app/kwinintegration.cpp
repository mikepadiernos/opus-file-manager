// SPDX-License-Identifier: GPL-3.0-or-later

#include "kwinintegration.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPackage/PackageJob>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>

static void initKWinScriptResources()
{
    Q_INIT_RESOURCE(kwinscript);
}

namespace
{
const QString KWinService = QStringLiteral("org.kde.KWin");
const QString ScriptingPath = QStringLiteral("/Scripting");
const QString ScriptingInterface = QStringLiteral("org.kde.kwin.Scripting");
const QString BundledPackage = QStringLiteral(":/kwin/opusfilemanager");
constexpr int DBusTimeout = 1000;

QDBusMessage scriptingCall(const QString &method)
{
    return QDBusMessage::createMethodCall(KWinService, ScriptingPath, ScriptingInterface, method);
}

KConfigGroup pluginsGroup(KConfig &kwinrc)
{
    return kwinrc.group(QStringLiteral("Plugins"));
}

QString jobError(KJob *job)
{
    if (!job->error()) {
        return QString();
    }
    return job->errorString().isEmpty() ? i18nc("@info", "The KWin script package operation failed.") : job->errorString();
}
}

bool SystemKWinBackend::kwinRunning() const
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(KWinService).value();
}

std::optional<bool> SystemKWinBackend::scriptLoaded() const
{
    QDBusMessage message = scriptingCall(QStringLiteral("isScriptLoaded"));
    message << KWinScript::pluginId();
    const QDBusReply<bool> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, DBusTimeout);
    if (!reply.isValid()) {
        return std::nullopt;
    }
    return reply.value();
}

int SystemKWinBackend::installedVersion() const
{
    return KWinScript::installedVersion();
}

bool SystemKWinBackend::pluginEnabled() const
{
    KConfig kwinrc(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    return KWinScript::isEnabled(pluginsGroup(kwinrc));
}

void SystemKWinBackend::setPluginEnabled(bool enabled)
{
    KConfig kwinrc(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    KConfigGroup plugins = pluginsGroup(kwinrc);
    plugins.writeEntry(KWinScript::enabledKey(), enabled);
    kwinrc.sync();
}

void SystemKWinBackend::forgetPluginEnabled()
{
    KConfig kwinrc(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    KConfigGroup plugins = pluginsGroup(kwinrc);
    plugins.deleteEntry(KWinScript::enabledKey());
    kwinrc.sync();
}

void SystemKWinBackend::startScripts()
{
    QDBusConnection::sessionBus().asyncCall(scriptingCall(QStringLiteral("start")));
}

void SystemKWinBackend::unloadScript()
{
    QDBusMessage message = scriptingCall(QStringLiteral("unloadScript"));
    message << KWinScript::pluginId();
    QDBusConnection::sessionBus().call(message, QDBus::Block, DBusTimeout);
}

void SystemKWinBackend::invokeProcessShortcut()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/component/kwin"),
                                                          QStringLiteral("org.kde.kglobalaccel.Component"),
                                                          QStringLiteral("invokeShortcut"));
    message << KWinScript::processShortcutName();
    QDBusConnection::sessionBus().asyncCall(message);
}

void SystemKWinBackend::installPackage(const QString &sourceDirectory, bool update, const Callback &done)
{
    const QString format = QStringLiteral("KWin/Script");
    KPackage::PackageJob *job = update ? KPackage::PackageJob::update(format, sourceDirectory, KWinScript::packageRoot())
                                       : KPackage::PackageJob::install(format, sourceDirectory, KWinScript::packageRoot());
    QObject::connect(job, &KJob::result, job, [done](KJob *job) {
        done(jobError(job));
    });
}

void SystemKWinBackend::removePackage(const Callback &done)
{
    KPackage::PackageJob *job = KPackage::PackageJob::uninstall(QStringLiteral("KWin/Script"), KWinScript::pluginId(), KWinScript::packageRoot());
    QObject::connect(job, &KJob::result, job, [done](KJob *job) {
        done(jobError(job));
    });
}

KWinIntegration::KWinIntegration(std::unique_ptr<KWinBackend> backend, KSharedConfigPtr config, KSharedConfigPtr stateConfig, QObject *parent)
    : QObject(parent)
    , m_backend(std::move(backend))
    , m_config(std::move(config))
    , m_stateConfig(std::move(stateConfig))
{
    initKWinScriptResources();
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(0);
    connect(&m_syncTimer, &QTimer::timeout, this, [this] {
        if (m_backend->pluginEnabled() && m_backend->kwinRunning()) {
            m_backend->invokeProcessShortcut();
        }
    });
}

KWinIntegration::~KWinIntegration() = default;

bool KWinIntegration::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.registerObject(KWinScript::objectPath(), this, QDBusConnection::ExportScriptableSlots);
}

KWinBackend *KWinIntegration::backend() const
{
    return m_backend.get();
}

void KWinIntegration::setBackend(std::unique_ptr<KWinBackend> backend)
{
    m_backend = std::move(backend);
    m_runningVersion = -1;
    Q_EMIT stateChanged();
}

KWinScript::State KWinIntegration::state() const
{
    KWinScript::State state;
    state.kwinRunning = m_backend->kwinRunning();
    state.bundledVersion = bundledVersion();
    state.installedVersion = m_backend->installedVersion();
    state.enabled = m_backend->pluginEnabled();
    state.runningVersion = m_runningVersion;
    if (state.kwinRunning) {
        const std::optional<bool> loaded = m_backend->scriptLoaded();
        state.scriptingAvailable = loaded.has_value();
        state.loaded = loaded.value_or(false);
    }
    return state;
}

KWinScript::Status KWinIntegration::status() const
{
    return KWinScript::status(state());
}

QString KWinIntegration::statusText() const
{
    const KWinScript::State current = state();
    switch (KWinScript::status(current)) {
    case KWinScript::Status::KWinUnavailable:
        return i18nc("@info", "KWin is not running, so window management is unavailable. File management is not affected.");
    case KWinScript::Status::ScriptingUnavailable:
        return i18nc("@info", "KWin's scripting interface is unavailable, so window management is unavailable.");
    case KWinScript::Status::NotInstalled:
        return i18nc("@info", "The Plasma Opus KWin script is not installed.");
    case KWinScript::Status::UpdateAvailable:
        return i18nc("@info", "Version %1 of the script is installed; this build includes version %2.", current.installedVersion, current.bundledVersion);
    case KWinScript::Status::Disabled:
        return i18nc("@info", "Version %1 of the script is installed but disabled.", current.installedVersion);
    case KWinScript::Status::NotRunning:
        return i18nc("@info", "Version %1 of the script is enabled but has not contacted Plasma Opus yet.", current.installedVersion);
    case KWinScript::Status::RestartRequired:
        return i18nc("@info",
                     "KWin is running version %1 of the script; disable and enable it to load version %2.",
                     current.runningVersion,
                     current.installedVersion);
    case KWinScript::Status::Running:
        return i18nc("@info", "Version %1 of the script is running.", current.runningVersion);
    }
    return QString();
}

int KWinIntegration::bundledVersion()
{
    initKWinScriptResources();
    QFile file(BundledPackage + QLatin1String("/metadata.json"));
    return file.open(QIODevice::ReadOnly) ? KWinScript::packageVersion(file.readAll()) : -1;
}

QString KWinIntegration::extractPackage(const QString &directory)
{
    initKWinScriptResources();
    const QString root = directory + QLatin1Char('/') + KWinScript::pluginId();
    QDirIterator it(BundledPackage, QDir::Files, QDirIterator::Subdirectories);
    bool any = false;
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = root + source.mid(BundledPackage.size());
        QFile input(source);
        QFile output(target);
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)
            || output.write(input.readAll()) < 0) {
            return QString();
        }
        any = true;
    }
    return any ? root : QString();
}

KWinScript::Options KWinIntegration::options() const
{
    return KWinScript::readOptions(m_config->group(QStringLiteral("KWinScript")));
}

void KWinIntegration::setOptions(const KWinScript::Options &options)
{
    KConfigGroup group = m_config->group(QStringLiteral("KWinScript"));
    KWinScript::writeOptions(group, options);
    group.sync();
    requestSync();
}

KWinScript::DropDown KWinIntegration::dropDown() const
{
    return KWinScript::readDropDown(m_config->group(QStringLiteral("KWinScript")));
}

void KWinIntegration::setDropDown(const KWinScript::DropDown &dropDown)
{
    KConfigGroup group = m_config->group(QStringLiteral("KWinScript"));
    KWinScript::writeDropDown(group, dropDown);
    group.sync();
    requestSync();
}

QMap<QString, QString> KWinIntegration::desktopWorkspaces() const
{
    return KWinScript::readDesktopWorkspaces(m_config->group(QStringLiteral("KWinVirtualDesktops")));
}

void KWinIntegration::setDesktopWorkspaces(const QMap<QString, QString> &assignments)
{
    KConfigGroup group = m_config->group(QStringLiteral("KWinVirtualDesktops"));
    KWinScript::writeDesktopWorkspaces(group, assignments);
    m_config->sync();
}

QList<KWinScript::Desktop> KWinIntegration::desktops() const
{
    return m_desktops;
}

void KWinIntegration::setWorkspaceProvider(std::function<QString()> provider)
{
    m_workspaceProvider = std::move(provider);
}

bool KWinIntegration::restoreWorkspace(const QString &workspaceId)
{
    if (!options().restoreWindows) {
        return false;
    }
    const std::optional<KWinScript::Placement> kept = placement(workspaceId);
    if (!kept) {
        return false;
    }
    m_commands.append(KWinScript::restoreCommand(*kept));
    requestSync();
    return true;
}

std::optional<KWinScript::Placement> KWinIntegration::placement(const QString &workspaceId) const
{
    return KWinScript::readPlacement(m_stateConfig->group(QStringLiteral("KWinWindowPlacements")), workspaceId);
}

void KWinIntegration::requestSync()
{
    m_syncTimer.start();
}

bool KWinIntegration::scriptActive() const
{
    const KWinScript::Status current = status();
    return current == KWinScript::Status::Running || current == KWinScript::Status::RestartRequired;
}

void KWinIntegration::install()
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    auto directory = std::make_shared<QTemporaryDir>();
    const QString source = directory->isValid() ? extractPackage(directory->path()) : QString();
    if (source.isEmpty()) {
        finishOperation(i18nc("@info", "Could not prepare the KWin script package."));
        return;
    }
    const bool update = m_backend->installedVersion() >= 0;
    QPointer<KWinIntegration> self(this);
    m_backend->installPackage(source, update, [self, directory](const QString &error) {
        if (!self) {
            return;
        }
        if (error.isEmpty()) {
            self->m_backend->setPluginEnabled(true);
            // Reload, so KWin runs the version just installed.
            self->m_backend->unloadScript();
            self->m_runningVersion = -1;
            self->m_backend->startScripts();
            self->requestSync();
        }
        self->finishOperation(error);
    });
}

void KWinIntegration::remove()
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    m_backend->forgetPluginEnabled();
    m_backend->unloadScript();
    m_runningVersion = -1;
    QPointer<KWinIntegration> self(this);
    m_backend->removePackage([self](const QString &error) {
        if (self) {
            self->finishOperation(error);
        }
    });
}

void KWinIntegration::setEnabled(bool enabled)
{
    m_backend->setPluginEnabled(enabled);
    if (enabled) {
        m_backend->startScripts();
        requestSync();
    } else {
        m_backend->unloadScript();
        m_runningVersion = -1;
    }
    Q_EMIT stateChanged();
}

void KWinIntegration::refresh()
{
    Q_EMIT stateChanged();
}

void KWinIntegration::finishOperation(const QString &error)
{
    m_busy = false;
    Q_EMIT operationFinished(error);
    Q_EMIT stateChanged();
}

QString KWinIntegration::Sync(const QString &scriptVersion)
{
    bool ok = false;
    const int version = scriptVersion.toInt(&ok);
    const int previous = m_runningVersion;
    m_runningVersion = ok && version > 0 ? version : -1;
    const QJsonArray commands = std::exchange(m_commands, QJsonArray());
    if (previous != m_runningVersion) {
        QTimer::singleShot(0, this, &KWinIntegration::stateChanged);
    }
    return KWinScript::request(QCoreApplication::applicationPid(), options(), dropDown(), {}, commands);
}

void KWinIntegration::WindowChanged(const QString &placement)
{
    const QString workspaceId = m_workspaceProvider ? m_workspaceProvider() : QString();
    const std::optional<KWinScript::Placement> parsed = KWinScript::placementFromJson(placement);
    if (workspaceId.isEmpty() || !parsed) {
        return;
    }
    KConfigGroup group = m_stateConfig->group(QStringLiteral("KWinWindowPlacements"));
    KWinScript::writePlacement(group, workspaceId, *parsed);
    group.sync();
}

void KWinIntegration::DesktopsChanged(const QString &desktops)
{
    const QList<KWinScript::Desktop> parsed = KWinScript::desktopsFromJson(desktops);
    if (parsed != m_desktops) {
        m_desktops = parsed;
        Q_EMIT desktopsChanged();
    }
}

void KWinIntegration::CurrentDesktopChanged(const QString &desktopId)
{
    if (!options().followDesktops) {
        return;
    }
    const QString workspaceId = desktopWorkspaces().value(desktopId);
    if (!workspaceId.isEmpty()) {
        Q_EMIT desktopWorkspaceRequested(workspaceId);
    }
}

void KWinIntegration::EdgeActivated()
{
    Q_EMIT edgeActivated();
}
