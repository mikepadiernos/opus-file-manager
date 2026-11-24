// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercolorrules.h"

#include "settings.h"

#include <KFileMetaData/UserMetaData>

#include <QDir>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace
{
const QString MatchKey = QStringLiteral("match");
const QString PatternKey = QStringLiteral("pattern");
const QString ColorKey = QStringLiteral("color");
const QString SubfoldersKey = QStringLiteral("subfolders");

QString matchName(FolderColorRules::Match match)
{
    switch (match) {
    case FolderColorRules::Match::Path:
        return QStringLiteral("path");
    case FolderColorRules::Match::Tag:
        return QStringLiteral("tag");
    case FolderColorRules::Match::Name:
        break;
    }
    return QStringLiteral("name");
}

QString expandedPath(const QString &pattern)
{
    QString path = pattern;
    if (path == u'~' || path.startsWith(QStringLiteral("~/"))) {
        path.replace(0, 1, QDir::homePath());
    }
    return QDir::cleanPath(path);
}

/// The path and its ancestors, nearest first.
QStringList folderChain(const QString &path)
{
    QStringList chain;
    QString current = path;
    while (!current.isEmpty()) {
        chain.append(current);
        if (current == u'/') {
            break;
        }
        const qsizetype slash = current.lastIndexOf(u'/');
        current = slash <= 0 ? QStringLiteral("/") : current.first(slash);
    }
    return chain;
}

QString folderName(const QString &path)
{
    return path.sliced(path.lastIndexOf(u'/') + 1);
}
}

FolderColorRules::FolderColorRules(const QStringList &entries)
{
    for (const QString &stored : entries) {
        if (m_rules.size() >= MaximumRules) {
            break;
        }
        if (const std::optional<Rule> rule = parse(stored); rule && !m_rules.contains(*rule)) {
            m_rules.append(*rule);
        }
    }
}

FolderColorRules FolderColorRules::configured()
{
    return FolderColorRules(Settings::folderColorRules());
}

std::optional<FolderColorRules::Rule> FolderColorRules::parse(const QString &entry)
{
    const QJsonObject object = QJsonDocument::fromJson(entry.toUtf8()).object();
    Rule rule;
    const QString match = object.value(MatchKey).toString();
    if (match == QLatin1String("path")) {
        rule.match = Match::Path;
    } else if (match == QLatin1String("tag")) {
        rule.match = Match::Tag;
    } else if (match != QLatin1String("name")) {
        return std::nullopt;
    }
    rule.pattern = object.value(PatternKey).toString().trimmed();
    rule.color = QColor::fromString(object.value(ColorKey).toString());
    rule.subfolders = object.value(SubfoldersKey).toBool();
    if (rule.pattern.isEmpty() || !rule.color.isValid()) {
        return std::nullopt;
    }
    if (rule.match == Match::Path) {
        if (!rule.pattern.startsWith(u'/') && rule.pattern != u'~' && !rule.pattern.startsWith(QStringLiteral("~/"))) {
            return std::nullopt;
        }
        rule.pattern = QDir::cleanPath(rule.pattern);
    }
    if (rule.match == Match::Name && (rule.pattern.contains(u'/') || !QRegularExpression::fromWildcard(rule.pattern).isValid())) {
        return std::nullopt;
    }
    rule.color.setAlpha(255);
    return rule;
}

QString FolderColorRules::entry(const Rule &rule)
{
    const QJsonObject object{{MatchKey, matchName(rule.match)},
                             {PatternKey, rule.pattern},
                             {ColorKey, rule.color.isValid() ? rule.color.name(QColor::HexRgb) : QString()},
                             {SubfoldersKey, rule.subfolders}};
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QList<FolderColorRules::Rule> FolderColorRules::rules() const
{
    return m_rules;
}

QStringList FolderColorRules::entries() const
{
    QStringList result;
    for (const Rule &rule : m_rules) {
        result.append(entry(rule));
    }
    return result;
}

bool FolderColorRules::isEmpty() const
{
    return m_rules.isEmpty();
}

QColor FolderColorRules::colorFor(const QString &path, const TagReader &tags) const
{
    if (m_rules.isEmpty() || path.isEmpty()) {
        return {};
    }
    const QString folder = QDir::cleanPath(path);
    const QStringList chain = folderChain(folder);
    QHash<QString, QStringList> tagCache;
    const auto tagsOf = [&tags, &tagCache](const QString &candidate) {
        auto found = tagCache.constFind(candidate);
        if (found == tagCache.constEnd()) {
            found = tagCache.insert(candidate, tags ? tags(candidate) : KFileMetaData::UserMetaData(candidate).tags());
        }
        return *found;
    };
    for (const Rule &rule : m_rules) {
        const qsizetype depth = rule.subfolders ? chain.size() : 1;
        switch (rule.match) {
        case Match::Path: {
            const QString target = expandedPath(rule.pattern);
            if (folder == target || (rule.subfolders && (target == u'/' || folder.startsWith(target + u'/')))) {
                return rule.color;
            }
            break;
        }
        case Match::Name: {
            const QRegularExpression pattern(QRegularExpression::fromWildcard(rule.pattern, Qt::CaseInsensitive));
            for (qsizetype i = 0; i < depth; ++i) {
                if (chain.at(i) != u'/' && pattern.match(folderName(chain.at(i))).hasMatch()) {
                    return rule.color;
                }
            }
            break;
        }
        case Match::Tag:
            for (qsizetype i = 0; i < depth; ++i) {
                if (tagsOf(chain.at(i)).contains(rule.pattern, Qt::CaseInsensitive)) {
                    return rule.color;
                }
            }
            break;
        }
    }
    return {};
}
