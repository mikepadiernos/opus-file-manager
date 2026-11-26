// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexbackend.h"

#include <KFileMetaData/ExtractorCollection>
#include <KLocalizedString>
#include <KPluginMetaData>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QProcess>
#include <QStandardPaths>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/File>
#endif

namespace
{
const QString BalooService = QStringLiteral("org.kde.baloo");
const QString SchedulerPath = QStringLiteral("/scheduler");
const QString SchedulerInterface = QStringLiteral("org.kde.baloo.scheduler");
const QString IndexerPath = QStringLiteral("/fileindexer");
const QString IndexerInterface = QStringLiteral("org.kde.baloo.fileindexer");
// Calls block the window, so give up quickly on an unresponsive indexer.
constexpr int Timeout = 2000;

QVariant dbusProperty(const QString &path, const QString &interface, const QString &name)
{
    QDBusMessage message = QDBusMessage::createMethodCall(BalooService, path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    message << interface << name;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, Timeout);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return reply.arguments().constFirst().value<QDBusVariant>().variant();
}
}

struct SystemIndexBackend::Private {
    QDBusServiceWatcher *watcher = nullptr;
    mutable std::unique_ptr<KFileMetaData::ExtractorCollection> extractors;
};

SystemIndexBackend::SystemIndexBackend(QObject *parent)
    : IndexBackend(parent)
    , d(std::make_unique<Private>())
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    d->watcher = new QDBusServiceWatcher(BalooService, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(d->watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &IndexBackend::serviceChanged);
    bus.connect(BalooService, SchedulerPath, SchedulerInterface, QStringLiteral("stateChanged"), this, SLOT(notifyStateChanged(int)));
}

SystemIndexBackend::~SystemIndexBackend() = default;

IndexStatus::Config SystemIndexBackend::config() const
{
    return IndexStatus::Config::read();
}

IndexStatus::Service SystemIndexBackend::service() const
{
    IndexStatus::Service result;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface() || !bus.interface()->isServiceRegistered(BalooService)) {
        return result;
    }
    result.running = true;
    bool ok = false;
    const int state = dbusProperty(SchedulerPath, SchedulerInterface, QStringLiteral("state")).toInt(&ok);
    result.state = ok ? state : -1;
    if (IndexStatus::isBusy(result.state)) {
        const QDBusMessage reply = bus.call(QDBusMessage::createMethodCall(BalooService, SchedulerPath, SchedulerInterface, QStringLiteral("getRemainingTime")),
                                            QDBus::Block,
                                            Timeout);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            result.remainingMs = reply.arguments().constFirst().toUInt();
        }
        result.currentFile = dbusProperty(IndexerPath, IndexerInterface, QStringLiteral("currentFile")).toString();
    }
    return result;
}

bool SystemIndexBackend::isIndexed(const QString &path) const
{
#ifdef OPUSFM_HAVE_BALOO
    Baloo::File file(path);
    return file.load();
#else
    Q_UNUSED(path)
    return false;
#endif
}

bool SystemIndexBackend::hasExtractor(const QString &mimeType) const
{
    if (!d->extractors) {
        d->extractors = std::make_unique<KFileMetaData::ExtractorCollection>();
    }
    return !d->extractors->fetchExtractors(mimeType).isEmpty();
}

bool SystemIndexBackend::suspend(QString *error)
{
    return callScheduler(QStringLiteral("suspend"), error);
}

bool SystemIndexBackend::resume(QString *error)
{
    return callScheduler(QStringLiteral("resume"), error);
}

bool SystemIndexBackend::checkUnindexedFiles(QString *error)
{
    return callScheduler(QStringLiteral("checkUnindexedFiles"), error);
}

bool SystemIndexBackend::openSettings(QString *error)
{
    return launchSettings(error);
}

bool SystemIndexBackend::launchSettings(QString *error)
{
    const auto plugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_baloofile"));
    const auto widgetPlugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings_qwidgets"), QStringLiteral("kcm_baloofile"));
    if (!plugin.isValid() && !widgetPlugin.isValid()) {
        *error = i18nc("@info", "KDE File Search settings are not installed.");
        return false;
    }
    const QString launcher = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
    if (launcher.isEmpty() || !QProcess::startDetached(launcher, {QStringLiteral("kcm_baloofile")})) {
        *error = i18nc("@info", "Could not launch KDE File Search settings.");
        return false;
    }
    return true;
}

void SystemIndexBackend::notifyStateChanged(int state)
{
    Q_UNUSED(state)
    Q_EMIT serviceChanged();
}

bool SystemIndexBackend::callScheduler(const QString &method, QString *error)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface() || !bus.interface()->isServiceRegistered(BalooService)) {
        *error = i18nc("@info", "The file indexer is not running.");
        return false;
    }
    const QDBusMessage reply = bus.call(QDBusMessage::createMethodCall(BalooService, SchedulerPath, SchedulerInterface, method), QDBus::Block, Timeout);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        *error = i18nc("@info", "The file indexer did not respond: %1", reply.errorMessage());
        return false;
    }
    return true;
}
