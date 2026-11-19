// SPDX-License-Identifier: GPL-3.0-or-later

#include "symbolicicon.h"
#include "testicontheme.h"

#include <KIconUtils>
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
        QVERIFY(install(m_themeDir.path(), {QStringLiteral("bar")}, {QStringLiteral("foo")}, {QStringLiteral("emblem")}));
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

    void preservesOverlaysAtAllSizes()
    {
        const QStringList overlays{QStringLiteral("emblem")};
        const QIcon original = KIconUtils::addOverlays(QIcon::fromTheme(QStringLiteral("foo")), overlays);
        const QIcon icon = SymbolicIcon::wrap(original, QStringLiteral("foo"), overlays);
        QVERIFY(containsColor(icon, 16, SymbolicColor));
        QVERIFY(containsColor(icon, 16, EmblemColor));
        QVERIFY(containsColor(icon, 48, RegularColor));
        QVERIFY(containsColor(icon, 48, EmblemColor));
        QVERIFY(containsColor(icon, 16, SymbolicColor, 2.0));
        QVERIFY(containsColor(icon, 16, EmblemColor, 2.0));
        QPixmap thumbnail(16, 16);
        thumbnail.fill(Qt::yellow);
        const QIcon preview(thumbnail);
        QCOMPARE(SymbolicIcon::wrap(preview, QStringLiteral("foo"), overlays).cacheKey(), preview.cacheKey());
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
