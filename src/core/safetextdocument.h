// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QTextDocument>

class SafeTextDocument : public QTextDocument
{
public:
    using QTextDocument::QTextDocument;

protected:
    QVariant loadResource(int, const QUrl &) override
    {
        // An invalid variant would allow QTextDocument to load local files.
        return QImage();
    }
};
