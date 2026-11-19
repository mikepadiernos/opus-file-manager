// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrenamepresets.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class BatchRenamePresetsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void savesValidRulesAndRejectsInvalidOrDuplicateNames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("presetsrc")), KConfig::SimpleConfig);
        const auto group = config->group(QStringLiteral("BatchRenamePresets"));
        const QList<BatchRenamePresets::Preset> presets{
            {QStringLiteral("Photos"), {QStringLiteral("IMG_(\\d+)"), QStringLiteral("photo-$1"), true, 3, 4}},
            {QStringLiteral("Audio"), {QStringLiteral("{artist}-{nn}"), {}, false, 1, 2}},
        };
        QString error;
        QVERIFY(BatchRenamePresets::save(group, presets, &error));
        config->reparseConfiguration();
        const auto loaded = BatchRenamePresets::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.first().rule.pattern, presets.first().rule.pattern);
        QCOMPARE(loaded.first().rule.replacement, presets.first().rule.replacement);
        QVERIFY(loaded.first().rule.regularExpression);
        QCOMPARE(loaded.first().rule.start, 3);
        QCOMPARE(loaded.first().rule.padding, 4);
        auto duplicate = presets;
        duplicate.append({QStringLiteral(" photos "), presets.first().rule});
        QVERIFY(!BatchRenamePresets::save(group, duplicate, &error));
        QVERIFY(!error.isEmpty());
        duplicate = presets;
        duplicate[0].rule.pattern = QStringLiteral("(");
        QVERIFY(!BatchRenamePresets::save(group, duplicate, &error));
        QCOMPARE(BatchRenamePresets::load(group, &error).size(), 2);
    }

    void reportsCorruptPresets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("presetsrc")), KConfig::SimpleConfig);
        auto group = config->group(QStringLiteral("BatchRenamePresets"));
        group.writeEntry("Presets", QByteArray(R"([{"name":"bad","pattern":"x","replacement":"","regex":true,"start":1.5,"padding":2}])"));
        QString error;
        QVERIFY(BatchRenamePresets::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(BatchRenamePresetsTest)
#include "batchrenamepresetstest.moc"
