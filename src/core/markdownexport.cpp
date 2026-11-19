// SPDX-License-Identifier: GPL-3.0-or-later

#include "markdownexport.h"
#include "safetextdocument.h"

#include <KLocalizedString>

#include <QFileInfo>
#include <QPdfWriter>
#include <QSaveFile>

QString MarkdownExport::toPdf(const QString &text, const QUrl &source, const QString &destination)
{
    const QFileInfo target(destination);
    if (source.isLocalFile()) {
        const QFileInfo original(source.toLocalFile());
        if (target.absoluteFilePath() == original.absoluteFilePath()
            || (!target.canonicalFilePath().isEmpty() && target.canonicalFilePath() == original.canonicalFilePath())) {
            return i18nc("@info", "The PDF destination must not be the source file.");
        }
    }

    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    {
        QPdfWriter writer(&file);
        SafeTextDocument document;
        document.setMarkdown(text);
        document.print(&writer);
    }
    if (file.error() != QFileDevice::NoError || !file.commit()) {
        return file.errorString();
    }
    return {};
}
