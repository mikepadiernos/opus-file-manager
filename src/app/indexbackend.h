// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "indexstatus.h"

#include <QObject>

#include <memory>

/**
 * Reads the shared Baloo index and its indexer and offers KDE's indexing
 * controls, so tests can replace them. Nothing here changes the shared
 * settings; the controls act only when called.
 */
class IndexBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual IndexStatus::Config config() const = 0;
    virtual IndexStatus::Service service() const = 0;
    /// Whether @p path has an entry in the index: confirmed rather than configured coverage.
    virtual bool isIndexed(const QString &path) const = 0;
    /// Whether an installed KFileMetaData extractor handles @p mimeType.
    virtual bool hasExtractor(const QString &mimeType) const = 0;
    /// Asks the indexer to pause; @returns false with @p error when it can't.
    virtual bool suspend(QString *error) = 0;
    virtual bool resume(QString *error) = 0;
    /// Asks the indexer to look for files in the indexed folders that are missing from the index.
    virtual bool checkUnindexedFiles(QString *error) = 0;
    /// Opens KDE File Search settings.
    virtual bool openSettings(QString *error) = 0;

Q_SIGNALS:
    /// The indexer started, stopped or changed state.
    void serviceChanged();
};

/// Talks to Baloo's file indexer over D-Bus and reads the index with Baloo.
class SystemIndexBackend : public IndexBackend
{
    Q_OBJECT

public:
    explicit SystemIndexBackend(QObject *parent = nullptr);
    ~SystemIndexBackend() override;

    IndexStatus::Config config() const override;
    IndexStatus::Service service() const override;
    bool isIndexed(const QString &path) const override;
    bool hasExtractor(const QString &mimeType) const override;
    bool suspend(QString *error) override;
    bool resume(QString *error) override;
    bool checkUnindexedFiles(QString *error) override;
    bool openSettings(QString *error) override;

    /// Starts KDE File Search settings; @returns false with @p error when they are missing or fail to start.
    static bool launchSettings(QString *error);

private Q_SLOTS:
    void notifyStateChanged(int state);

private:
    bool callScheduler(const QString &method, QString *error);

    struct Private;
    std::unique_ptr<Private> d;
};
