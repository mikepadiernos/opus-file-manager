// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcutstorage.h"

#include "keybindings.h"

#include <QDomDocument>
#include <QDomElement>

namespace ShortcutStorage
{
namespace
{
const QString None = QStringLiteral("none");
const QString Separator = QStringLiteral("; ");
}

std::optional<QList<QKeySequence>> fromText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    if (trimmed == None) {
        return QList<QKeySequence>();
    }
    QList<QKeySequence> shortcuts;
    for (const QString &part : trimmed.split(Separator)) {
        const QKeySequence sequence = QKeySequence::fromString(part.trimmed(), QKeySequence::PortableText);
        if (sequence.isEmpty()) {
            return std::nullopt;
        }
        for (int i = 0; i < sequence.count(); ++i) {
            if (sequence[i].key() == Qt::Key_unknown || sequence[i].toCombined() == 0) {
                return std::nullopt;
            }
        }
        shortcuts.append(sequence);
    }
    return shortcuts;
}

QString toText(const QList<QKeySequence> &shortcuts)
{
    const QString text = QKeySequence::listToString(KeyBindings::keys(shortcuts), QKeySequence::PortableText);
    return text.isEmpty() ? None : text;
}

std::optional<QList<QKeySequence>> read(const KConfigGroup &group, const QString &name)
{
    if (name == QLatin1String(VersionKey)) {
        return std::nullopt;
    }
    return fromText(group.readEntry(name, QString()));
}

void write(KConfigGroup &group, const QString &name, const QList<QKeySequence> &shortcuts, const QList<QKeySequence> &defaults)
{
    if (name.isEmpty() || name == QLatin1String(VersionKey)) {
        return;
    }
    if (KeyBindings::keys(shortcuts) == KeyBindings::keys(defaults)) {
        if (group.hasKey(name)) {
            group.deleteEntry(name);
        }
        return;
    }
    group.writeEntry(name, toText(shortcuts));
}

QStringList actionNames(const KConfigGroup &group)
{
    QStringList names = group.keyList();
    names.removeAll(QLatin1String(VersionKey));
    return names;
}

bool isCurrent(const KConfigGroup &group)
{
    return group.readEntry(VersionKey, 0) >= Version;
}

void markCurrent(KConfigGroup &group)
{
    group.writeEntry(VersionKey, Version);
}

XmlGuiShortcuts takeXmlGuiShortcuts(const QString &uiRc, const QString &scheme)
{
    XmlGuiShortcuts found;
    QDomDocument document;
    if (uiRc.isEmpty() || !document.setContent(uiRc)) {
        return found;
    }
    const QString shortcut = QStringLiteral("shortcut");
    bool removed = false;
    for (QDomElement properties = document.documentElement().firstChildElement(); !properties.isNull(); properties = properties.nextSiblingElement()) {
        // KXmlGui compares the tag without case and treats a missing scheme as the default one.
        if (properties.tagName().compare(QLatin1String("ActionProperties"), Qt::CaseInsensitive) != 0
            || properties.attribute(QStringLiteral("scheme"), QStringLiteral("Default")) != scheme) {
            continue;
        }
        QDomElement action = properties.firstChildElement();
        while (!action.isNull()) {
            const QDomElement next = action.nextSiblingElement();
            if (action.tagName().compare(QLatin1String("Action"), Qt::CaseInsensitive) == 0 && action.hasAttribute(shortcut)) {
                const QString name = action.attribute(QStringLiteral("name"));
                if (!name.isEmpty()) {
                    // KDE writes an empty attribute for no shortcut; an unreadable one gives the defaults back.
                    const QString text = action.attribute(shortcut).trimmed();
                    if (text.isEmpty()) {
                        found.shortcuts.append({name, {}});
                    } else if (const auto keys = fromText(text)) {
                        found.shortcuts.append({name, KeyBindings::keys(*keys)});
                    }
                }
                action.removeAttribute(shortcut);
                removed = true;
                if (action.attributes().count() <= 1) {
                    properties.removeChild(action);
                }
            }
            action = next;
        }
    }
    if (removed) {
        found.stripped = document.toString();
    }
    return found;
}

void migrate(KConfigGroup &group, const XmlGuiShortcuts &found)
{
    for (const auto &[name, shortcuts] : found.shortcuts) {
        if (name != QLatin1String(VersionKey)) {
            group.writeEntry(name, toText(shortcuts));
        }
    }
    markCurrent(group);
}
}
