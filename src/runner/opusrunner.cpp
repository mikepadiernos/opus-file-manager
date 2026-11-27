// SPDX-License-Identifier: GPL-3.0-or-later

#include "opusrunner.h"

#include "favoritesmodel.h"
#include "settings.h"

#include <KConfigGroup>
#include <KIO/Global>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QDBusMetaType>
#include <QProcess>
#include <QStandardPaths>

QDBusArgument &operator<<(QDBusArgument &argument, const RemoteMatch &match)
{
    argument.beginStructure();
    argument << match.id << match.text << match.iconName << match.categoryRelevance << match.relevance << match.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteMatch &match)
{
    argument.beginStructure();
    argument >> match.id >> match.text >> match.iconName >> match.categoryRelevance >> match.relevance >> match.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const RemoteAction &action)
{
    argument.beginStructure();
    argument << action.id << action.text << action.iconName;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteAction &action)
{
    argument.beginStructure();
    argument >> action.id >> action.text >> action.iconName;
    argument.endStructure();
    return argument;
}

namespace
{
void launchOpus(const QStringList &arguments, const QString &activationToken)
{
    QString program = QStandardPaths::findExecutable(QStringLiteral("opus-file-manager"), {QCoreApplication::applicationDirPath()});
    if (program.isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("opus-file-manager"));
    }
    if (program.isEmpty()) {
        qWarning("opus-file-manager-krunner: opus-file-manager was not found");
        return;
    }
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!activationToken.isEmpty()) {
        environment.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), activationToken);
        environment.insert(QStringLiteral("DESKTOP_STARTUP_ID"), activationToken);
    }
    process.setProcessEnvironment(environment);
    process.startDetached();
}
}

OpusRunner::OpusRunner(QObject *parent)
    : QObject(parent)
    , m_launcher(launchOpus)
{
    registerTypes();
}

void OpusRunner::registerTypes()
{
    qDBusRegisterMetaType<RemoteMatch>();
    qDBusRegisterMetaType<RemoteMatches>();
    qDBusRegisterMetaType<RemoteAction>();
    qDBusRegisterMetaType<RemoteActions>();
}

void OpusRunner::setLauncher(Launcher launcher)
{
    m_launcher = std::move(launcher);
}

RemoteActions OpusRunner::Actions()
{
    return {};
}

const QList<RunnerMatches::Target> &OpusRunner::targets()
{
    if (!m_targets) {
        Settings::self()->load();
        const KSharedConfig::Ptr config = Settings::self()->sharedConfig();
        WorkspaceCatalog::Catalog catalog;
        QString error;
        if (!WorkspaceCatalog::load(config->group(QStringLiteral("Workspaces")), catalog, error)) {
            catalog = {};
        }
        QList<RunnerMatches::Favorite> favorites;
        const FavoritesModel model(config->group(QStringLiteral("Favorites")));
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row);
            favorites.append({index.data(FavoritesModel::UrlRole).toUrl(), index.data(Qt::DisplayRole).toString()});
        }
        m_targets = RunnerMatches::targets(catalog, favorites, {Settings::runnerWorkspaces(), Settings::runnerFavorites()});
    }
    return *m_targets;
}

RemoteMatches OpusRunner::Match(const QString &query)
{
    RemoteMatches result;
    const QString category = i18nc("@title KRunner category", "Plasma Opus");
    for (const RunnerMatches::Match &match : RunnerMatches::match(targets(), query)) {
        const RunnerMatches::Target &target = match.target;
        RemoteMatch remote;
        remote.id = target.id;
        remote.text = target.text;
        remote.categoryRelevance = match.categoryRelevance;
        remote.relevance = match.relevance;
        remote.properties.insert(QStringLiteral("category"), category);
        remote.properties.insert(QStringLiteral("actions"), QStringList());
        if (target.kind == RunnerMatches::Kind::Workspace) {
            remote.iconName = QStringLiteral("view-grid");
            remote.properties.insert(QStringLiteral("subtext"), i18nc("@info KRunner match", "Plasma Opus workspace"));
        } else {
            remote.iconName = KIO::iconNameForUrl(target.url);
            remote.properties.insert(QStringLiteral("subtext"), RunnerMatches::subtext(target));
            remote.properties.insert(QStringLiteral("urls"), QStringList{target.url.adjusted(QUrl::RemoveUserInfo).toString()});
        }
        result.append(remote);
    }
    return result;
}

void OpusRunner::Run(const QString &matchId, const QString &actionId)
{
    Q_UNUSED(actionId)
    // Read the settings again, so a workspace or Favorite removed since the
    // match was shown is not opened.
    m_targets.reset();
    const std::optional<RunnerMatches::Target> target = RunnerMatches::resolve(targets(), matchId);
    const QString token = std::exchange(m_activationToken, QString());
    if (!target || !m_launcher) {
        return;
    }
    m_launcher(RunnerMatches::arguments(*target), token);
}

void OpusRunner::Teardown()
{
    m_targets.reset();
}

void OpusRunner::SetActivationToken(const QString &token)
{
    m_activationToken = token;
}
