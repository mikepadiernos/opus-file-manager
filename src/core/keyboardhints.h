// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/**
 * Label generation and matching for keyboard hints, which overlay short key
 * sequences on visible targets so the keyboard can reach them directly.
 */
namespace KeyboardHints
{

/// The keyboard layout whose home row supplies the label keys.
enum class Layout {
    Qwerty,
    Azerty,
    Dvorak,
    Colemak,
};

/// The label keys for @p layout: home-row keys first, most comfortable first.
QString alphabet(Layout layout);

/**
 * Returns @p count distinct labels built from @p alphabet.
 *
 * The labels are prefix-free, so typing one never completes another early.
 * They use the fewest keys possible: all labels have one or two lengths, and
 * the shorter labels come first. The result depends only on the arguments.
 * Duplicate letters in @p alphabet are ignored, and letters are lowercased.
 * An alphabet with fewer than two letters yields labels only when they fit.
 */
QStringList labels(int count, const QString &alphabet);

/// The kind of target a label points at, in the order labels are assigned.
enum class Kind {
    Item,
    Location,
    Tab,
    Place,
};

/**
 * Returns labels for targets of the given kinds, one per entry of @p kinds.
 * Shorter labels go to items, then locations, tabs and places; targets of
 * one kind keep their order.
 */
QStringList assign(const QList<Kind> &kinds, const QString &alphabet);

/// Narrows a set of labels as keys are typed.
class Matcher
{
public:
    enum class Result {
        /// The typed keys begin one or more labels.
        Pending,
        /// The typed keys complete a label.
        Matched,
        /// The key starts no label; the typed keys are unchanged.
        NoMatch,
    };

    explicit Matcher(const QStringList &labels = {});

    /// Adds @p key, ignoring case.
    Result feed(QChar key);
    /// Removes the last typed key; returns false when nothing was typed.
    bool backspace();
    void reset();

    QString typed() const;
    /// The index of the completed label, or -1.
    int matched() const;
    /// Whether label @p index still begins with the typed keys.
    bool isCandidate(int index) const;
    /// The number of labels that still begin with the typed keys.
    int candidateCount() const;

private:
    QStringList m_labels;
    QString m_typed;
    int m_matched = -1;
};

} // namespace KeyboardHints
