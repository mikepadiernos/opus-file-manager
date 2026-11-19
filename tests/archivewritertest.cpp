// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivewriter.h"

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KTar>
#include <KZip>
#include <KZipFileEntry>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using ArchiveWriter::Format;

class ArchiveWriterTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString path(const QString &relative) const
    {
        return m_dir.filePath(relative);
    }

    static std::unique_ptr<KArchive> open(const QString &archivePath, Format format)
    {
        std::unique_ptr<KArchive> archive;
        if (format == Format::Zip || format == Format::ZipStored) {
            archive = std::make_unique<KZip>(archivePath);
        } else {
            archive = std::make_unique<KTar>(archivePath);
        }
        return archive->open(QIODevice::ReadOnly) ? std::move(archive) : nullptr;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("photos/2026")));
        QFile photo(path(QStringLiteral("photos/2026/beach.txt")));
        QVERIFY(photo.open(QIODevice::WriteOnly));
        photo.write("sand");
        photo.close();
        QFile note(path(QStringLiteral("note.txt")));
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("hello");
    }

    void suffixes()
    {
        QCOMPARE(ArchiveWriter::formats().size(), 5);
        QCOMPARE(ArchiveWriter::suffix(Format::Zip), QStringLiteral("zip"));
        QCOMPARE(ArchiveWriter::suffix(Format::ZipStored), QStringLiteral("zip"));
        QCOMPARE(ArchiveWriter::suffix(Format::TarGz), QStringLiteral("tar.gz"));
        QCOMPARE(ArchiveWriter::suffix(Format::TarXz), QStringLiteral("tar.xz"));
        QCOMPARE(ArchiveWriter::suffix(Format::TarZstd), QStringLiteral("tar.zst"));
    }

    void writesEveryFormat_data()
    {
        QTest::addColumn<Format>("format");
        for (Format format : ArchiveWriter::formats()) {
            QTest::addRow("%s", qPrintable(ArchiveWriter::suffix(format) + QString::number(static_cast<int>(format)))) << format;
        }
    }

    void writesEveryFormat()
    {
        QFETCH(Format, format);
        const QString archivePath = path(QStringLiteral("out-%1.%2").arg(static_cast<int>(format)).arg(ArchiveWriter::suffix(format)));
        QCOMPARE(ArchiveWriter::write(archivePath, format, {path(QStringLiteral("photos")), path(QStringLiteral("note.txt"))}), QString());

        const std::unique_ptr<KArchive> archive = open(archivePath, format);
        QVERIFY(archive);
        const auto *beach = archive->directory()->file(QStringLiteral("photos/2026/beach.txt"));
        QVERIFY(beach);
        QCOMPARE(beach->data(), QByteArrayLiteral("sand"));
        const auto *note = archive->directory()->file(QStringLiteral("note.txt"));
        QVERIFY(note);
        QCOMPARE(note->data(), QByteArrayLiteral("hello"));
    }

    void storedZipDoesNotCompress()
    {
        const QString stored = path(QStringLiteral("stored.zip"));
        QCOMPARE(ArchiveWriter::write(stored, Format::ZipStored, {path(QStringLiteral("note.txt"))}), QString());
        KZip zip(stored);
        QVERIFY(zip.open(QIODevice::ReadOnly));
        const auto *note = static_cast<const KZipFileEntry *>(zip.directory()->file(QStringLiteral("note.txt")));
        QVERIFY(note);
        QCOMPARE(note->encoding(), 0);
    }

    void reportsErrors()
    {
        QVERIFY(!ArchiveWriter::write(path(QStringLiteral("missing/folder/out.zip")), Format::Zip, {path(QStringLiteral("note.txt"))}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ArchiveWriterTest)

#include "archivewritertest.moc"
