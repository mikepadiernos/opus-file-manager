// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QRegularExpression>
#include <QString>

/**
 * The text typed into a pane's filter bar and which kinds of items it
 * applies to. Plain text matches anywhere in a name. Text containing *, ?
 * or [ is a list of glob patterns separated by spaces, commas or
 * semicolons; a name passes when any pattern matches all of it. Matching
 * ignores case. Items of a kind outside the scope always pass.
 */
class NameFilter
{
public:
    enum class Scope {
        Files,
        Folders,
        FilesAndFolders,
    };

    NameFilter() = default;

    /**
     * @param text the filter text; surrounding spaces are ignored
     * @param scope which kinds of items the text applies to
     */
    explicit NameFilter(const QString &text, Scope scope = Scope::Files);

    QString text() const;
    Scope scope() const;
    bool isEmpty() const;

    /**
     * Whether the filter text is applied to an item of this kind.
     * @param isFolder whether the item is a folder
     * @returns false for an empty filter or a kind outside the scope
     */
    bool appliesTo(bool isFolder) const;

    /**
     * Whether an item stays visible.
     * @param name the item's file name
     * @param isFolder whether the item is a folder
     * @returns true when the filter is empty, the item is outside the
     *          scope, or the name matches
     */
    bool matches(const QString &name, bool isFolder) const;
    /// Also matches the folder metadata syntax: #label and *rating threshold.
    bool matches(const QString &name, bool isFolder, const QString &label, int rating) const;

    bool operator==(const NameFilter &other) const;

private:
    QString m_text;
    Scope m_scope = Scope::Files;
    QList<QRegularExpression> m_patterns;
};
