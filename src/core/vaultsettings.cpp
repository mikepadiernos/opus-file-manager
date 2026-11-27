// SPDX-License-Identifier: GPL-3.0-or-later

#include "vaultsettings.h"
#include "settings.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace VaultSettings
{
namespace
{
QString cleanRoot(const QString &root)
{
    return root.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(root).absoluteFilePath());
}

void readBool(const QJsonObject &json, const QString &key, bool *value)
{
    if (const QJsonValue found = json.value(key); found.isBool()) {
        *value = found.toBool();
    }
}
}

Options general()
{
    Options options;
    options.visual = Settings::editorMarkdownMode() == Settings::EnumEditorMarkdownMode::Visual;
    options.preview = Settings::editorMarkdownPreviewOnOpen();
    options.dialect = Settings::editorMarkdownDialect() == Settings::EnumEditorMarkdownDialect::CommonMark ? Dialect::CommonMark : Dialect::GitHub;
    options.wikiLinks = Settings::editorWikiLinks();
    options.backlinks = Settings::editorVaultBacklinks();
    options.links = Settings::editorVaultLinks();
    options.tags = Settings::editorVaultTags();
    return options;
}

QString vaultFolder()
{
    // Before the setting existed, the Vault menu remembered its folder in the state configuration.
    KConfigGroup legacy = KSharedConfig::openStateConfig()->group(QStringLiteral("EditorVault"));
    if (legacy.hasKey("Root")) {
        const QString root = cleanRoot(legacy.readEntry("Root", QString()));
        legacy.deleteEntry("Root");
        legacy.sync();
        if (Settings::editorVaultFolder().isEmpty() && !root.isEmpty()) {
            setVaultFolder(root);
        }
    }
    const QUrl url = Settings::editorVaultFolder();
    return url.isLocalFile() ? cleanRoot(url.toLocalFile()) : QString();
}

void setVaultFolder(const QString &root)
{
    const QString clean = cleanRoot(root);
    Settings::setEditorVaultFolder(clean.isEmpty() ? QUrl() : QUrl::fromLocalFile(clean));
    Settings::self()->save();
}

QString unavailableReason(const QString &root)
{
    if (root.isEmpty()) {
        return {};
    }
    const QFileInfo info(root);
    if (!info.exists()) {
        return i18nc("@info %1 is a folder path", "The vault folder %1 doesn't exist. It may be on a disconnected drive.", root);
    }
    if (!info.isDir()) {
        return i18nc("@info %1 is a path", "The vault %1 isn't a folder.", root);
    }
    if (!info.isReadable() || !info.isExecutable()) {
        return i18nc("@info %1 is a folder path", "The vault folder %1 can't be read.", root);
    }
    return {};
}

QJsonObject toJson(const Options &options)
{
    return QJsonObject{
        {QStringLiteral("visual"), options.visual},
        {QStringLiteral("preview"), options.preview},
        {QStringLiteral("dialect"), options.dialect == Dialect::CommonMark ? QStringLiteral("commonmark") : QStringLiteral("github")},
        {QStringLiteral("wikiLinks"), options.wikiLinks},
        {QStringLiteral("backlinks"), options.backlinks},
        {QStringLiteral("links"), options.links},
        {QStringLiteral("tags"), options.tags},
    };
}

Options fromJson(const QJsonObject &json, const Options &fallback)
{
    Options options = fallback;
    readBool(json, QStringLiteral("visual"), &options.visual);
    readBool(json, QStringLiteral("preview"), &options.preview);
    const QString dialect = json.value(QStringLiteral("dialect")).toString();
    if (dialect == QLatin1String("commonmark")) {
        options.dialect = Dialect::CommonMark;
    } else if (dialect == QLatin1String("github")) {
        options.dialect = Dialect::GitHub;
    }
    readBool(json, QStringLiteral("wikiLinks"), &options.wikiLinks);
    readBool(json, QStringLiteral("backlinks"), &options.backlinks);
    readBool(json, QStringLiteral("links"), &options.links);
    readBool(json, QStringLiteral("tags"), &options.tags);
    return options;
}
}
