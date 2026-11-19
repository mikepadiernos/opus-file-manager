// SPDX-License-Identifier: GPL-3.0-or-later

#include "listingexport.h"

#include "safetextdocument.h"

#include <KLocalizedString>

#include <QPdfWriter>
#include <QSaveFile>

namespace
{
bool valid(const ListingExport::Table &table)
{
    if (table.headers.isEmpty()) {
        return false;
    }
    for (const QStringList &row : table.rows) {
        if (row.size() != table.headers.size()) {
            return false;
        }
    }
    return true;
}

QString csvCell(QString cell)
{
    const QString trimmed = cell.trimmed();
    if ((!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front())) || cell.startsWith(QLatin1Char('\t')) || cell.startsWith(QLatin1Char('\r'))) {
        cell.prepend(QLatin1Char('\''));
    }
    if (cell.contains(QLatin1Char(',')) || cell.contains(QLatin1Char('"')) || cell.contains(QLatin1Char('\r')) || cell.contains(QLatin1Char('\n'))
        || cell.contains(QLatin1Char(';'))) {
        cell.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        cell = QLatin1Char('"') + cell + QLatin1Char('"');
    }
    return cell;
}

QByteArray csvRow(const QStringList &cells)
{
    QStringList escaped;
    escaped.reserve(cells.size());
    for (const QString &cell : cells) {
        escaped.append(csvCell(cell));
    }
    return (escaped.join(QLatin1Char(',')) + QStringLiteral("\r\n")).toUtf8();
}
}

QString ListingExport::toCsv(const Table &table, const QString &destination)
{
    if (!valid(table)) {
        return i18nc("@info", "The listing has invalid columns.");
    }
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    const QByteArray headers = csvRow(table.headers);
    if (file.write("\xEF\xBB\xBF", 3) != 3 || file.write(headers) != headers.size()) {
        return file.errorString();
    }
    for (const QStringList &row : table.rows) {
        const QByteArray data = csvRow(row);
        if (file.write(data) != data.size()) {
            return file.errorString();
        }
    }
    if (!file.commit()) {
        return file.errorString();
    }
    return {};
}

QString ListingExport::toPdf(const Table &table, const QString &destination)
{
    if (!valid(table)) {
        return i18nc("@info", "The listing has invalid columns.");
    }
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    QString html = QStringLiteral("<html><body><table border=\"1\" cellspacing=\"0\" cellpadding=\"3\"><thead><tr>");
    for (const QString &header : table.headers) {
        html += QStringLiteral("<th>") + header.toHtmlEscaped() + QStringLiteral("</th>");
    }
    html += QStringLiteral("</tr></thead><tbody>");
    for (const QStringList &row : table.rows) {
        html += QStringLiteral("<tr>");
        for (const QString &cell : row) {
            html += QStringLiteral("<td>") + cell.toHtmlEscaped() + QStringLiteral("</td>");
        }
        html += QStringLiteral("</tr>");
    }
    html += QStringLiteral("</tbody></table></body></html>");
    {
        QPdfWriter writer(&file);
        SafeTextDocument document;
        document.setHtml(html);
        document.print(&writer);
    }
    if (file.error() != QFileDevice::NoError || !file.commit()) {
        return file.errorString();
    }
    return {};
}
