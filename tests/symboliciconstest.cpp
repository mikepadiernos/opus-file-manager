// SPDX-License-Identifier: GPL-3.0-or-later

#include "symbolicicon.h"
#include "testicontheme.h"

#include <QPixmap>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace TestIconTheme;

class SymbolicIconTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_themeDir.isValid());
        QVERIFY(install(m_themeDir.path(), {QStringLiteral("bar")}, {QStringLiteral("foo")}));
    }

    void smallSizesUseSymbolicVariant()
    {
        const QIcon icon = SymbolicIcon::fromTheme(QStringLiteral("foo"));
        QCOMPARE(colorAt(icon, 16), SymbolicColor);
        QCOMPARE(colorAt(icon, SymbolicIcon::MaxSize), SymbolicColor);
    }

    void smallSizesUseSymbolicVariantOnHighDpi()
    {
        QCOMPARE(colorAt(SymbolicIcon::fromTheme(QStringLiteral("foo")), 16, 2.0), SymbolicColor);
    }

    void largerSizesUseRegularIcon()
    {
        const QIcon icon = SymbolicIcon::fromTheme(QStringLiteral("foo"));
        QCOMPARE(colorAt(icon, SymbolicIcon::MaxSize + 1), RegularColor);
        QCOMPARE(colorAt(icon, 48), RegularColor);
    }

    void keepsRegularName()
    {
        QCOMPARE(SymbolicIcon::fromTheme(QStringLiteral("foo")).name(), QStringLiteral("foo"));
    }

    void withoutSymbolicVariantKeepsIcon()
    {
        const QIcon regular = QIcon::fromTheme(QStringLiteral("bar"));
        QCOMPARE(SymbolicIcon::wrap(regular).cacheKey(), regular.cacheKey());
        QCOMPARE(colorAt(SymbolicIcon::fromTheme(QStringLiteral("bar")), 16), RegularColor);
    }

    void alreadySymbolicIsUnchanged()
    {
        const QIcon symbolic = QIcon::fromTheme(QStringLiteral("foo-symbolic"));
        QCOMPARE(SymbolicIcon::wrap(symbolic).cacheKey(), symbolic.cacheKey());
    }

    void unnamedIconIsUnchanged()
    {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::green);
        const QIcon icon(pixmap);
        QCOMPARE(SymbolicIcon::wrap(icon).cacheKey(), icon.cacheKey());
    }

private:
    QTemporaryDir m_themeDir;
};

QTEST_MAIN(SymbolicIconTest)

#include "symboliciconstest.moc"
