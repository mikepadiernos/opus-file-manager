// SPDX-License-Identifier: GPL-3.0-or-later

#include "informationpanel.h"

#include <KLocalizedString>

#include <QFormLayout>
#include <QLabel>
#include <QStandardPaths>
#include <QTest>

class InformationTranslationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void translatesFieldLabelsAfterLanguageInitialization()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("opus-file-manager"), QStringLiteral(OPUSFM_TEST_LOCALE_DIR));
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
        KLocalizedString::setLanguages({QStringLiteral("fa")});

        InformationPanel panel;
        panel.setFields({QStringLiteral("size")});
        auto *form = qobject_cast<QFormLayout *>(panel.layout());
        QVERIFY(form);
        auto *value = panel.findChild<QLabel *>(QStringLiteral("informationValue_size"));
        QVERIFY(value);
        auto *label = qobject_cast<QLabel *>(form->labelForField(value));
        QVERIFY(label);
        QCOMPARE(label->text(), QString::fromUtf8("اندازه"));
        KLocalizedString::clearLanguages();
    }
};

QTEST_MAIN(InformationTranslationTest)

#include "informationtranslationtest.moc"
