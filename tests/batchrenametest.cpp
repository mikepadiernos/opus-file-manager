// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrename.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

class BatchRenameTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void previewsTokensAndMetadata()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile photo(root.filePath(QStringLiteral("photo.jpg")));
        QFile audio(root.filePath(QStringLiteral("song.mp3")));
        QVERIFY(photo.open(QIODevice::WriteOnly));
        QVERIFY(audio.open(QIODevice::WriteOnly));
        photo.close();
        audio.close();
        BatchRename::Input image{photo.fileName(), {}};
        image.metadata.insert(KFileMetaData::Property::Model, QStringLiteral("Camera"));
        BatchRename::Input song{audio.fileName(), {}};
        song.metadata.insert(KFileMetaData::Property::Artist, QStringLiteral("Artist"));
        auto planned = BatchRename::plan({image}, QStringLiteral("{camera}-{nn}"), 3, 3);
        QVERIFY(planned.valid());
        QCOMPARE(planned.rows.first().destination, root.filePath(QStringLiteral("Camera-003.jpg")));
        planned = BatchRename::plan({song}, QStringLiteral("{artist}-{name}.{ext}"), 1, 2);
        QVERIFY(planned.valid());
        QCOMPARE(planned.rows.first().destination, root.filePath(QStringLiteral("Artist-song.mp3")));
        QVERIFY(!BatchRename::plan({song}, QStringLiteral("{camera}"), 1, 2).valid());
        QVERIFY(!BatchRename::plan({song}, QStringLiteral("{unknown}"), 1, 2).valid());
        QVERIFY(!BatchRename::plan({song}, QStringLiteral("{name"), 1, 2).valid());
        song.metadata.insert(KFileMetaData::Property::Artist, QStringLiteral("bad/name"));
        QVERIFY(!BatchRename::plan({song}, QStringLiteral("{artist}"), 1, 2).valid());
    }

    void detectsConflictsAndNestedSources()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("folder")));
        QFile first(root.filePath(QStringLiteral("first.txt")));
        QFile second(root.filePath(QStringLiteral("second.txt")));
        QFile child(root.filePath(QStringLiteral("folder/child")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        QVERIFY(second.open(QIODevice::WriteOnly));
        QVERIFY(child.open(QIODevice::WriteOnly));
        first.close();
        second.close();
        child.close();
        const QList<BatchRename::Input> inputs{{first.fileName(), {}}, {second.fileName(), {}}};
        QVERIFY(!BatchRename::plan(inputs, QStringLiteral("same"), 1, 2).valid());
        QVERIFY(!BatchRename::plan({inputs.first()}, QStringLiteral("second"), 1, 2).valid());
        QVERIFY(!BatchRename::plan({{root.filePath(QStringLiteral("folder")), {}}, {child.fileName(), {}}}, QStringLiteral("{name}-new"), 1, 2).valid());
        QVERIFY(!BatchRename::plan({inputs.first(), inputs.first()}, QStringLiteral("{n}"), 1, 2).valid());
    }

    void previewsRegexCapturesAndRejectsInvalidRules()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile source(root.filePath(QStringLiteral("IMG_123.jpg")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.close();
        BatchRename::Rule rule{QStringLiteral("IMG_(\\d+)"), QStringLiteral("photo-$1-$$"), true, 1, 2};
        auto planned = BatchRename::plan({{source.fileName(), {}}}, rule);
        QVERIFY(planned.valid());
        QCOMPARE(planned.rows.first().destination, root.filePath(QStringLiteral("photo-123-$.jpg")));
        rule.replacement = QStringLiteral("$2");
        QVERIFY(!BatchRename::plan({{source.fileName(), {}}}, rule).valid());
        rule.pattern = QStringLiteral("[");
        QVERIFY(!BatchRename::plan({{source.fileName(), {}}}, rule).valid());
        rule.pattern = QStringLiteral("(IMG)");
        rule.replacement = QStringLiteral("$1-$1");
        planned = BatchRename::plan({{source.fileName(), {}}}, rule);
        QVERIFY(planned.valid());
        QCOMPARE(planned.rows.first().destination, root.filePath(QStringLiteral("IMG-IMG_123.jpg")));
    }

    void appliesWithoutOverwritingAndReportsPartialResults()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile first(root.filePath(QStringLiteral("a.txt")));
        QFile second(root.filePath(QStringLiteral("b.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        QCOMPARE(first.write("first"), 5);
        first.close();
        QVERIFY(second.open(QIODevice::WriteOnly));
        QCOMPARE(second.write("second"), 6);
        second.close();
        const auto plan = BatchRename::plan({{first.fileName(), {}}, {second.fileName(), {}}}, QStringLiteral("{nn}"), 1, 2);
        QVERIFY(plan.valid());
        QFile conflict(plan.rows.last().destination);
        QVERIFY(conflict.open(QIODevice::WriteOnly));
        QCOMPARE(conflict.write("preserve"), 8);
        conflict.close();
        const auto rejected = BatchRename::apply(plan);
        QVERIFY(!rejected.error.isEmpty());
        QVERIFY(QFile::exists(first.fileName()));
        QVERIFY(conflict.open(QIODevice::ReadOnly));
        QCOMPARE(conflict.readAll(), QByteArray("preserve"));
        conflict.close();
        QVERIFY(conflict.remove());
        int checks = 0;
        const auto partial = BatchRename::apply(plan, [&checks] {
            return ++checks > 1;
        });
        QCOMPARE(partial.renamed, QStringList{plan.rows.first().destination});
        QVERIFY(!partial.error.isEmpty());
        QVERIFY(QFile::exists(plan.rows.first().destination));
        QVERIFY(QFile::exists(second.fileName()));
    }

    void refusesDestinationCreatedAfterPreflight()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile source(root.filePath(QStringLiteral("source")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.close();
        const auto plan = BatchRename::plan({{source.fileName(), {}}}, QStringLiteral("destination"), 1, 2);
        QVERIFY(plan.valid());
        const auto result = BatchRename::apply(plan, [&] {
            QFile destination(plan.rows.first().destination);
            if (!destination.open(QIODevice::WriteOnly) || destination.write("preserve") != 8) {
                return true;
            }
            return false;
        });
        QVERIFY(!result.error.isEmpty());
        QVERIFY(QFile::exists(source.fileName()));
        QFile destination(plan.rows.first().destination);
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("preserve"));
    }

    void refusesReplacedSources()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile source(root.filePath(QStringLiteral("source")));
        QFile replacement(root.filePath(QStringLiteral("replacement")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.close();
        QVERIFY(replacement.open(QIODevice::WriteOnly));
        replacement.close();
        const auto plan = BatchRename::plan({{source.fileName(), {}}}, QStringLiteral("destination"), 1, 2);
        QVERIFY(plan.valid());
        QVERIFY(source.remove());
        QVERIFY(replacement.rename(source.fileName()));
        const auto result = BatchRename::apply(plan);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(QFile::exists(source.fileName()));
        QVERIFY(!QFile::exists(plan.rows.first().destination));
    }

    void doesNotOverwriteDanglingSymlinks()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile source(root.filePath(QStringLiteral("source")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.close();
        const QString destination = root.filePath(QStringLiteral("destination"));
        QCOMPARE(::symlink("missing-target", QFile::encodeName(destination).constData()), 0);
        const auto plan = BatchRename::plan({{source.fileName(), {}}}, QStringLiteral("destination"), 1, 2);
        QVERIFY(!plan.valid());
        QVERIFY(QFileInfo(destination).isSymLink());
        QVERIFY(QFile::exists(source.fileName()));
    }
};

QTEST_GUILESS_MAIN(BatchRenameTest)
#include "batchrenametest.moc"
