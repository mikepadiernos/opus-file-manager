// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

class DraftStore
{
public:
    struct Draft {
        QString id;
        QString text;
        QUrl source;
        qint64 timestamp = 0;
    };

    explicit DraftStore(const QString &directory);
    QString write(const Draft &draft);
    QString remove(const QString &id);
    QList<Draft> drafts(const QUrl &source, QString *error) const;

private:
    QString prepareDirectory() const;
    QString m_directory;
};
