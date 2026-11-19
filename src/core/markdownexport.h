// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QUrl>

namespace MarkdownExport
{
// Returns an error message on failure.
QString toPdf(const QString &text, const QUrl &source, const QString &destination);
}
