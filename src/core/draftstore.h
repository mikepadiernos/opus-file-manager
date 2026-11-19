// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

#include <map>
#include <memory>

class QLockFile;

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
    ~DraftStore();
    QString write(const Draft &draft);
    QString remove(const QString &id, const Draft *expected = nullptr);
    QList<Draft> drafts(const QUrl &source, QString *error) const;
    QString prune(int retentionDays, qint64 now);
    void release(const QString &id);

private:
    QString prepareDirectory() const;
    QString m_directory;
    std::map<QString, std::unique_ptr<QLockFile>> m_locks;
};
