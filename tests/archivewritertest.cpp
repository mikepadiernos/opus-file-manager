// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivewriter.h"
#include "archives.h"

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KTar>
#include <KZip>
#include <KZipFileEntry>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

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

    void encryptedZipRequiresPasswordAndExtractsSafely()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString zip = root.filePath(QStringLiteral("encrypted.zip"));
        QCOMPARE(
            ArchiveWriter::writeEncryptedZip(zip, Format::Zip, {path(QStringLiteral("photos")), path(QStringLiteral("note.txt"))}, QStringLiteral("secret")),
            QString());
        QString error;
        QCOMPARE(Archives::testIntegrity(zip, QString(), &error), Archives::TestResult::PasswordRequired);
        QCOMPARE(Archives::testIntegrity(zip, QStringLiteral("wrong"), &error), Archives::TestResult::Failed);
        QCOMPARE(Archives::testIntegrity(zip, QStringLiteral("secret"), &error), Archives::TestResult::Passed);
        QVERIFY(!ArchiveWriter::writeEncryptedZip(zip, Format::Zip, {path(QStringLiteral("note.txt"))}, QStringLiteral("secret")).isEmpty());
        QVERIFY(!Archives::extractProtected(zip, root.filePath(QStringLiteral("wrong")), QStringLiteral("wrong")).isEmpty());
        const QString target = root.filePath(QStringLiteral("extracted"));
        QCOMPARE(Archives::extractProtected(zip, target, QStringLiteral("secret")), QString());
        QFile note(QDir(target).filePath(QStringLiteral("note.txt")));
        QVERIFY(note.open(QIODevice::ReadOnly));
        QCOMPARE(note.readAll(), QByteArrayLiteral("hello"));
        QFile beach(QDir(target).filePath(QStringLiteral("photos/2026/beach.txt")));
        QVERIFY(beach.open(QIODevice::ReadOnly));
        QCOMPARE(beach.readAll(), QByteArrayLiteral("sand"));
        QVERIFY(!Archives::extractProtected(zip, target, QStringLiteral("secret")).isEmpty());
    }

    void encryptedStoredZipRoundTrips()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString zip = root.filePath(QStringLiteral("stored.zip"));
        QCOMPARE(ArchiveWriter::writeEncryptedZip(zip, Format::ZipStored, {path(QStringLiteral("note.txt"))}, QStringLiteral("secret")), QString());
        const QString target = root.filePath(QStringLiteral("output"));
        QCOMPARE(Archives::extractProtected(zip, target, QStringLiteral("secret")), QString());
        QFile note(QDir(target).filePath(QStringLiteral("note.txt")));
        QVERIFY(note.open(QIODevice::ReadOnly));
        QCOMPARE(note.readAll(), QByteArrayLiteral("hello"));
    }

    void encryptedZipRejectsUnsafeInput()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("source/nested")));
        const QString source = root.filePath(QStringLiteral("source"));
        for (const QString &inside : {root.filePath(QStringLiteral("source/result.zip")), root.filePath(QStringLiteral("source/nested/result.zip"))}) {
            QVERIFY(!ArchiveWriter::writeEncryptedZip(inside, Format::Zip, {source}, QStringLiteral("secret")).isEmpty());
            QVERIFY(!QFile::exists(inside));
        }
        const QString output = root.filePath(QStringLiteral("out.zip"));
        const QString nullPassword = QStringLiteral("secret") + QChar(QChar::Null) + QStringLiteral("suffix");
        QVERIFY(ArchiveWriter::writeEncryptedZip(output, Format::Zip, {path(QStringLiteral("note.txt"))}, nullPassword).contains(QStringLiteral("NUL")));
        QVERIFY(!QFile::exists(output));
        QVERIFY(!ArchiveWriter::writeEncryptedZip(output, Format::Zip, {}, QStringLiteral("secret")).isEmpty());
        QVERIFY(!QFile::exists(output));
        QCOMPARE(ArchiveWriter::writeEncryptedZip(output, Format::Zip, {source}, QStringLiteral("secret")), QString());
        QVERIFY(Archives::extractProtected(output, root.filePath(QStringLiteral("extract")), nullPassword).contains(QStringLiteral("NUL")));
        QString error;
        QCOMPARE(Archives::testIntegrity(output, nullPassword, &error), Archives::TestResult::Failed);
        QVERIFY(error.contains(QStringLiteral("NUL")));
    }

    void protectedExtractionRejectsTraversal()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString zip = root.filePath(QStringLiteral("unsafe.zip"));
        archive *writer = archive_write_new();
        QVERIFY(writer);
        QCOMPARE(archive_write_set_format_zip(writer), ARCHIVE_OK);
        const QByteArray path = QFile::encodeName(zip);
        QCOMPARE(archive_write_open_filename(writer, path.constData()), ARCHIVE_OK);
        archive_entry *entry = archive_entry_new();
        QVERIFY(entry);
        archive_entry_set_pathname(entry, "../escaped");
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0600);
        archive_entry_set_size(entry, 1);
        QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
        QCOMPARE(archive_write_data(writer, "x", 1), 1);
        archive_entry_free(entry);
        QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
        QCOMPARE(archive_write_free(writer), ARCHIVE_OK);
        QVERIFY(!Archives::extractProtected(zip, root.filePath(QStringLiteral("out")), QStringLiteral("secret")).isEmpty());
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("escaped"))));
    }
};

QTEST_GUILESS_MAIN(ArchiveWriterTest)

#include "archivewritertest.moc"
