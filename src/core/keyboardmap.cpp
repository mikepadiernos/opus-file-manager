// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardmap.h"

#include "keybindings.h"
#include "safetextdocument.h"

#include <KLocalizedString>

#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QTextDocumentFragment>

namespace KeyboardMap
{
namespace
{
/// @p text as the user reads it: plain, without accelerator markers, a trailing ellipsis or surrounding spaces.
QString plain(const QString &text)
{
    QString result = Qt::mightBeRichText(text) ? QTextDocumentFragment::fromHtml(text).toPlainText() : text;
    result = KLocalizedString::removeAcceleratorMarker(result).simplified();
    for (const QString &ellipsis : {QStringLiteral("…"), QStringLiteral("...")}) {
        if (result.endsWith(ellipsis)) {
            result.chop(ellipsis.size());
        }
    }
    while (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' '))) {
        result.chop(1);
    }
    return result;
}

QString cell(const QString &text)
{
    return QStringLiteral("<td>") + text.toHtmlEscaped() + QStringLiteral("</td>");
}

QString shortcutsHtml(const Entry &entry)
{
    if (!entry.keysText.isEmpty()) {
        return QStringLiteral("<td><kbd>") + entry.keysText.toHtmlEscaped() + QStringLiteral("</kbd></td>");
    }
    QStringList keys;
    for (const QKeySequence &shortcut : KeyBindings::keys(entry.shortcuts)) {
        keys.append(QStringLiteral("<kbd>") + shortcut.toString(QKeySequence::NativeText).toHtmlEscaped() + QStringLiteral("</kbd>"));
    }
    if (keys.isEmpty()) {
        return cell(i18nc("@item no keyboard shortcut", "None"));
    }
    return QStringLiteral("<td>") + keys.join(i18nc("@item separator of keyboard shortcuts", "; ").toHtmlEscaped()) + QStringLiteral("</td>");
}
}

QString description(const QString &text, const QString &toolTip, const QString &statusTip, const QString &whatsThis)
{
    const QString name = plain(text);
    for (const QString &candidate : {toolTip, statusTip, whatsThis}) {
        const QString shown = plain(candidate);
        if (!shown.isEmpty() && shown.compare(name, Qt::CaseInsensitive) != 0) {
            return Qt::mightBeRichText(candidate) ? QTextDocumentFragment::fromHtml(candidate).toPlainText().simplified() : candidate.simplified();
        }
    }
    return {};
}

QString shortcutsText(const Entry &entry)
{
    if (!entry.keysText.isEmpty()) {
        return entry.keysText;
    }
    QStringList texts;
    for (const QKeySequence &shortcut : KeyBindings::keys(entry.shortcuts)) {
        texts.append(shortcut.toString(QKeySequence::NativeText));
    }
    return texts.join(i18nc("@item separator of keyboard shortcuts", "; "));
}

bool matchesText(const Entry &entry, const QString &query)
{
    QStringList haystack{entry.text, entry.name, entry.description, entry.context, shortcutsText(entry)};
    for (const QKeySequence &shortcut : KeyBindings::keys(entry.shortcuts)) {
        haystack.append(shortcut.toString(QKeySequence::PortableText));
    }
    const QString searched = haystack.join(QLatin1Char('\n'));
    const QStringList words = query.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &word : words) {
        if (!searched.contains(word, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

bool matchesKeys(const Entry &entry, const QKeySequence &keys)
{
    if (keys.isEmpty()) {
        return true;
    }
    for (const QKeySequence &shortcut : KeyBindings::keys(entry.shortcuts)) {
        if (shortcut == keys || KeyBindings::startsWith(shortcut, keys)) {
            return true;
        }
    }
    return false;
}

QList<Entry> filter(const QList<Entry> &entries, const QString &query, const QKeySequence &keys, bool withShortcutsOnly)
{
    QList<Entry> found;
    for (const Entry &entry : entries) {
        if ((!withShortcutsOnly || !KeyBindings::keys(entry.shortcuts).isEmpty()) && matchesKeys(entry, keys) && matchesText(entry, query)) {
            found.append(entry);
        }
    }
    return found;
}

QString toHtml(const QList<Entry> &entries, const QString &title)
{
    QStringList contexts;
    for (const Entry &entry : entries) {
        if (!contexts.contains(entry.context)) {
            contexts.append(entry.context);
        }
    }
    QString html = QStringLiteral(
                       "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>%1</title>\n"
                       "<style>body { font-family: sans-serif; } table { border-collapse: collapse; width: 100%; margin-bottom: 1em; } "
                       "th, td { border: 1px solid #999; padding: 3px 6px; text-align: left; vertical-align: top; } "
                       "kbd { font-family: monospace; white-space: nowrap; }</style>\n"
                       "</head><body>\n<h1>%1</h1>\n")
                       .arg(title.toHtmlEscaped());
    if (entries.isEmpty()) {
        html += QStringLiteral("<p>") + i18nc("@info", "No actions match.").toHtmlEscaped() + QStringLiteral("</p>\n");
    }
    for (const QString &context : contexts) {
        html += QStringLiteral("<h2>") + context.toHtmlEscaped() + QStringLiteral("</h2>\n");
        html += QStringLiteral(
                    "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%\"><thead><tr><th>%1</th><th>%2</th><th>%3</th></tr></thead><tbody>\n")
                    .arg(i18nc("@title:column", "Action").toHtmlEscaped(),
                         i18nc("@title:column", "Shortcuts").toHtmlEscaped(),
                         i18nc("@title:column", "Description").toHtmlEscaped());
        for (const Entry &entry : entries) {
            if (entry.context == context) {
                html += QStringLiteral("<tr>") + cell(entry.text) + shortcutsHtml(entry) + cell(entry.description) + QStringLiteral("</tr>\n");
            }
        }
        html += QStringLiteral("</tbody></table>\n");
    }
    html += QStringLiteral("</body></html>\n");
    return html;
}

QString exportReference(const QList<Entry> &entries, const QString &title, const QString &path)
{
    QSaveFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::WriteOnly)) {
        return path.isEmpty() ? i18nc("@info", "No file was chosen.") : file.errorString();
    }
    const QString html = toHtml(entries, title);
    if (path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
        QPdfWriter writer(&file);
        writer.setTitle(title);
        writer.setCreator(QStringLiteral("Plasma Opus"));
        writer.setPageSize(QPageSize(QPageSize::A4));
        SafeTextDocument document;
        document.setHtml(html);
        document.print(&writer);
    } else {
        const QByteArray data = html.toUtf8();
        if (file.write(data) != data.size()) {
            return file.errorString();
        }
    }
    if (file.error() != QFileDevice::NoError || !file.commit()) {
        return file.errorString();
    }
    return {};
}
}
