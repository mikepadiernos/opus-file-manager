// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileMetaData/Properties>

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

namespace ImageDetails
{
struct Data {
    QList<QColor> colours;
    QList<int> histogram;
    KFileMetaData::PropertyMultiMap metadata;
};

Data analyze(const QImage &image, const QString &path);
KFileMetaData::PropertyMultiMap extractMetadata(const QString &path, const QString &mimeType);
}
