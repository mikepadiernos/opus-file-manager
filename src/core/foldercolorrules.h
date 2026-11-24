// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

/**
 * Rules that colour folders by path, name pattern or tag, for folders
 * without their own colour. Rules are only read: they never change folders
 * or their contents. They are checked in order and the first match wins.
 */
class FolderColorRules
{
public:
    enum class Match {
        /// The folder's absolute path; "~" stands for the home folder.
        Path,
        /// A case-insensitive wildcard pattern for the folder's name.
        Name,
        /// A tag set on the folder, as shown in Dolphin and the Information panel.
        Tag,
    };

    struct Rule {
        Match match = Match::Name;
        QString pattern;
        QColor color;
        /// Whether folders below a matching folder take its colour too.
        bool subfolders = false;

        bool operator==(const Rule &other) const = default;
    };

    /// Reads the tags of a local path.
    using TagReader = std::function<QStringList(const QString &path)>;

    static constexpr int MaximumRules = 50;

    /// Valid rules from stored entries, without duplicates and up to MaximumRules.
    explicit FolderColorRules(const QStringList &entries = {});
    /// The rules in the FolderColorRules setting.
    static FolderColorRules configured();

    /// Parses one stored entry; returns nothing for an invalid rule.
    static std::optional<Rule> parse(const QString &entry);
    /// The stored form of @p rule.
    static QString entry(const Rule &rule);

    QList<Rule> rules() const;
    QStringList entries() const;
    bool isEmpty() const;

    /**
     * The colour of the first rule matching the local folder @p path, or an
     * invalid colour.
     * @param tags reads folder tags; defaults to the file's extended attributes
     */
    QColor colorFor(const QString &path, const TagReader &tags = {}) const;

private:
    QList<Rule> m_rules;
};
