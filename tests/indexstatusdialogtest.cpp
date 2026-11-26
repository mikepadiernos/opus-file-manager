// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexstatusdialog.h"
#include "indexbackend.h"

#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace
{
class FakeIndexBackend : public IndexBackend
{
public:
    using IndexBackend::IndexBackend;

    IndexStatus::Config config() const override
    {
        return settings;
    }
    IndexStatus::Service service() const override
    {
        return indexer;
    }
    bool isIndexed(const QString &path) const override
    {
        ++lookups;
        return indexed.contains(path);
    }
    bool hasExtractor(const QString &mimeType) const override
    {
        return extractors.contains(mimeType);
    }
    bool suspend(QString *) override
    {
        calls.append(QStringLiteral("suspend"));
        indexer.state = int(IndexStatus::State::Suspended);
        return true;
    }
    bool resume(QString *) override
    {
        calls.append(QStringLiteral("resume"));
        indexer.state = int(IndexStatus::State::Idle);
        return true;
    }
    bool checkUnindexedFiles(QString *error) override
    {
        calls.append(QStringLiteral("check"));
        *error = QStringLiteral("indexer refused");
        return false;
    }
    bool openSettings(QString *) override
    {
        calls.append(QStringLiteral("settings"));
        return true;
    }

    IndexStatus::Config settings;
    IndexStatus::Service indexer;
    QSet<QString> indexed;
    QSet<QString> extractors;
    QStringList calls;
    mutable int lookups = 0;
};

QString text(const QDialog &dialog, const char *name)
{
    auto *label = dialog.findChild<QLabel *>(QLatin1String(name));
    return label ? label->text() : QStringLiteral("<missing %1>").arg(QLatin1String(name));
}

QPushButton *button(const QDialog &dialog, const char *name)
{
    return dialog.findChild<QPushButton *>(QLatin1String(name));
}

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}
}

class IndexStatusDialogTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_directory;
    FakeIndexBackend m_backend;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_directory.isValid());
    }

    void init()
    {
        m_backend.settings = {};
        m_backend.settings.available = true;
        m_backend.settings.enabled = true;
        m_backend.settings.includeFolders = {m_directory.path()};
        m_backend.settings.excludeFolders = {m_directory.filePath(QStringLiteral("build"))};
        m_backend.settings.excludeFilters = {QStringLiteral("*.o")};
        m_backend.indexer = {};
        m_backend.indexer.running = true;
        m_backend.indexer.state = int(IndexStatus::State::Idle);
        m_backend.indexed.clear();
        m_backend.extractors.clear();
        m_backend.calls.clear();
        m_backend.lookups = 0;
    }

    void showsSettingsAndState()
    {
        IndexStatusDialog dialog(m_directory.path(), &m_backend);
        QCOMPARE(text(dialog, "indexService"), QStringLiteral("Idle"));
        QCOMPARE(text(dialog, "indexMode"), QStringLiteral("On: file names, contents and metadata"));
        QCOMPARE(text(dialog, "indexIncludes"), m_directory.path());
        QCOMPARE(text(dialog, "indexExcludes"), m_directory.filePath(QStringLiteral("build")));
        QVERIFY(text(dialog, "indexFilters").startsWith(QStringLiteral("1 file name pattern")));
        QVERIFY(text(dialog, "indexFolderCoverage").contains(QStringLiteral("in the indexed folders")));
        QVERIFY(text(dialog, "indexWarnings").isEmpty());
        QCOMPARE(text(dialog, "indexConfirmed"), QStringLiteral("Not checked."));
        // Nothing is looked up or changed until asked.
        QCOMPARE(m_backend.lookups, 0);
        QVERIFY(m_backend.calls.isEmpty());

        m_backend.indexer.state = int(IndexStatus::State::ContentIndexing);
        m_backend.indexer.currentFile = QStringLiteral("/home/user/a.pdf");
        Q_EMIT m_backend.serviceChanged();
        QVERIFY(text(dialog, "indexService").startsWith(QStringLiteral("Indexing file contents")));
        QCOMPARE(text(dialog, "indexProgress"), QStringLiteral("Indexing /home/user/a.pdf"));
        QVERIFY(text(dialog, "indexWarnings").contains(QStringLiteral("incomplete")));
    }

    void warnsWhenIndexingIsOff()
    {
        m_backend.settings.enabled = false;
        m_backend.indexer = {};
        IndexStatusDialog dialog(m_directory.path(), &m_backend);
        QVERIFY(text(dialog, "indexWarnings").contains(QStringLiteral("turned off")));
        QCOMPARE(text(dialog, "indexMode"), QStringLiteral("Off"));
        QVERIFY(!button(dialog, "indexCheckCoverage")->isEnabled());
        QVERIFY(!button(dialog, "indexSuspend")->isEnabled());
        QVERIFY(!button(dialog, "indexCheckUnindexed")->isEnabled());
        // KDE's settings stay available to turn indexing on.
        button(dialog, "indexOpenSettings")->click();
        QCOMPARE(m_backend.calls, QStringList{QStringLiteral("settings")});
    }

    void reportsExcludedFolders()
    {
        IndexStatusDialog dialog(m_directory.filePath(QStringLiteral("build")), &m_backend);
        QVERIFY(text(dialog, "indexFolderCoverage").contains(QStringLiteral("excluded folder")));
        QVERIFY(text(dialog, "indexWarnings").contains(QStringLiteral("excluded folder")));
        IndexStatusDialog remote(QString(), &m_backend);
        QVERIFY(text(remote, "indexFolderCoverage").contains(QStringLiteral("Not a local folder")));
        QVERIFY(!button(remote, "indexCheckCoverage")->isEnabled());
    }

    void confirmsCoverage()
    {
        QTemporaryDir folder(m_directory.filePath(QStringLiteral("sampleXXXXXX")));
        QVERIFY(folder.isValid());
        writeFile(folder.filePath(QStringLiteral("a.txt")), "text");
        writeFile(folder.filePath(QStringLiteral("b.png")), "\x89PNG\r\n\x1a\n");
        writeFile(folder.filePath(QStringLiteral("c.o")), "object");
        writeFile(folder.filePath(QStringLiteral(".hidden")), "hidden");
        m_backend.indexed = {folder.filePath(QStringLiteral("a.txt"))};
        m_backend.extractors = {QStringLiteral("text/plain")};
        IndexStatusDialog dialog(folder.path(), &m_backend);
        button(dialog, "indexCheckCoverage")->click();
        const QString confirmed = text(dialog, "indexConfirmed");
        QVERIFY2(confirmed.contains(QStringLiteral("1 of 2 files that the settings include are in the index")), qPrintable(confirmed));
        QVERIFY2(confirmed.contains(QStringLiteral("2 files are excluded")), qPrintable(confirmed));
        QVERIFY(confirmed.contains(QStringLiteral("waiting to be indexed")));
        QCOMPARE(m_backend.lookups, 2);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexConfirmed"))->toolTip().contains(QStringLiteral("b.png")));
        const QString extractors = text(dialog, "indexExtractors");
        QVERIFY2(extractors.contains(QStringLiteral("image/png")) && !extractors.contains(QStringLiteral("text/plain")), qPrintable(extractors));

        m_backend.settings.onlyBasic = true;
        IndexStatusDialog basic(folder.path(), &m_backend);
        button(basic, "indexCheckCoverage")->click();
        QVERIFY(text(basic, "indexExtractors").contains(QStringLiteral("only file names")));
        QVERIFY(text(basic, "indexWarnings").contains(QStringLiteral("contents and metadata")));

        QTemporaryDir empty(m_directory.filePath(QStringLiteral("emptyXXXXXX")));
        IndexStatusDialog none(empty.path(), &m_backend);
        button(none, "indexCheckCoverage")->click();
        QCOMPARE(text(none, "indexConfirmed"), QStringLiteral("This folder has no files to check."));
    }

    void usesControlsOnlyWhenAsked()
    {
        IndexStatusDialog dialog(m_directory.path(), &m_backend);
        QPushButton *suspend = button(dialog, "indexSuspend");
        QCOMPARE(suspend->text(), QStringLiteral("Suspend Indexing"));
        suspend->click();
        QCOMPARE(m_backend.calls, QStringList{QStringLiteral("suspend")});
        QCOMPARE(suspend->text(), QStringLiteral("Resume Indexing"));
        QVERIFY(text(dialog, "indexWarnings").contains(QStringLiteral("suspended")));
        QVERIFY(!button(dialog, "indexCheckUnindexed")->isEnabled());
        suspend->click();
        QCOMPARE(m_backend.calls.last(), QStringLiteral("resume"));
        QCOMPARE(suspend->text(), QStringLiteral("Suspend Indexing"));

        bool asked = false;
        bool accept = false;
        dialog.setConfirmation([&](const QString &, const QString &message, const QString &) {
            asked = true;
            return message.contains(QStringLiteral("all applications")) && accept;
        });
        button(dialog, "indexCheckUnindexed")->click();
        QVERIFY(asked);
        QCOMPARE(m_backend.calls.size(), 2);
        accept = true;
        button(dialog, "indexCheckUnindexed")->click();
        QCOMPARE(m_backend.calls.last(), QStringLiteral("check"));
        QCOMPARE(text(dialog, "indexActionStatus"), QStringLiteral("indexer refused"));
    }
};

QTEST_MAIN(IndexStatusDialogTest)

#include "indexstatusdialogtest.moc"
