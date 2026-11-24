// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercolors.h"
#include "foldercolorrules.h"
#include "foldermetadata.h"
#include "labelcolor.h"
#include "settings.h"
#include "symbolicicon.h"
#include "testicontheme.h"

#include <KFileItem>
#include <QDir>
#include <QPixmap>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <sys/stat.h>

using namespace TestIconTheme;

namespace
{
const QColor VariantColor = Qt::yellow;

QColor pixelAt(const QIcon &icon, int size, int x, int y)
{
    return icon.pixmap(QSize(size, size)).toImage().pixelColor(x, y);
}
}

class FolderColorsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_themeDir.isValid());
        QVERIFY(writeIcon(m_themeDir.path(), 16, QStringLiteral("folder-red"), VariantColor));
        QVERIFY(writeIcon(m_themeDir.path(), 48, QStringLiteral("folder-red"), VariantColor));
        QVERIFY(install(m_themeDir.path(), {QStringLiteral("folder-documents")}, {QStringLiteral("folder")}, {QStringLiteral("emblem")}));
    }

    void readsAndCachesFolderColours()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        FolderColors::clearCache();
        QVERIFY(!FolderColors::color(folder.path()).isValid());
        QVERIFY(!FolderColors::color(QString()).isValid());
        QVERIFY(!FolderColors::color(QUrl(QStringLiteral("sftp://host/folder"))).isValid());

        const QColor red = LabelColors::rgb(LabelColor::Red);
        QVERIFY(FolderMetadata::setFolderColor(folder.path(), red.name()));
        QVERIFY2(!FolderColors::color(folder.path()).isValid(), "a cached colour is reused until the cache is cleared or expires");
        FolderColors::clearCache();
        QCOMPARE(FolderColors::color(folder.path()), red);
        QCOMPARE(FolderColors::color(QUrl::fromLocalFile(folder.path())), red);
    }

    void ownColourTakesPrecedenceOverRules()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            FolderColors::clearCache();
        });
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("own/child")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("plain/child")));
        const QColor green = LabelColors::rgb(LabelColor::Green);
        const QColor red = LabelColors::rgb(LabelColor::Red);
        QVERIFY(FolderMetadata::setFolderColor(root.filePath(QStringLiteral("own")), red.name()));
        Settings::setFolderColorRules({FolderColorRules::entry({FolderColorRules::Match::Path, root.path(), green, true})});
        FolderColors::clearCache();
        QCOMPARE(FolderColors::color(root.filePath(QStringLiteral("own"))), red);
        QCOMPARE(FolderColors::color(root.filePath(QStringLiteral("own/child"))), green);
        QCOMPARE(FolderColors::color(root.filePath(QStringLiteral("plain/child"))), green);
        Settings::setFolderColorRules({});
        QCOMPARE(FolderColors::color(root.filePath(QStringLiteral("plain"))), green);
        FolderColors::clearCache();
        QVERIFY2(!FolderColors::color(root.filePath(QStringLiteral("plain"))).isValid(), "rules are read again when the cache is cleared");
        QVERIFY(FolderMetadata::read(root.filePath(QStringLiteral("plain"))).isEmpty());
    }

    void findsThemedFolderVariants()
    {
        const QColor red = LabelColors::rgb(LabelColor::Red);
        QCOMPARE(FolderColors::themedIconName(QStringLiteral("folder"), red), QStringLiteral("folder-red"));
        QCOMPARE(FolderColors::themedIconName(QStringLiteral("inode-directory"), QColor(0xe0, 0x40, 0x50)), QStringLiteral("folder-red"));
        QVERIFY(FolderColors::themedIconName(QStringLiteral("folder"), LabelColors::rgb(LabelColor::Blue)).isEmpty());
        QVERIFY(FolderColors::themedIconName(QStringLiteral("folder-documents"), red).isEmpty());
        QVERIFY(FolderColors::themedIconName(QStringLiteral("folder"), QColor()).isEmpty());
    }

    void iconStyleUsesVariantOrTint()
    {
        const QColor red = LabelColors::rgb(LabelColor::Red);
        const QIcon folder = SymbolicIcon::fromTheme(QStringLiteral("folder"));
        const QIcon colored = FolderColors::decorate(folder, QStringLiteral("folder"), {}, red, FolderColors::Style::Icon);
        QCOMPARE(colorAt(colored, 48), VariantColor);
        QCOMPARE(colorAt(colored, 16), red);
        QCOMPARE(colorAt(colored, 16, 2.0), red);

        const QColor blue = LabelColors::rgb(LabelColor::Blue);
        const QIcon tinted = FolderColors::decorate(folder, QStringLiteral("folder"), {}, blue, FolderColors::Style::Icon);
        const QColor large = colorAt(tinted, 48);
        QVERIFY2(large != RegularColor && large != blue, "full colour icons are partly tinted");
        QCOMPARE(colorAt(tinted, 16), blue);

        const QIcon documents = QIcon::fromTheme(QStringLiteral("folder-documents"));
        QVERIFY(colorAt(FolderColors::decorate(documents, QStringLiteral("folder-documents"), {}, red, FolderColors::Style::Icon), 48) != RegularColor);
    }

    void badgeStyleKeepsIconAndAddsDot()
    {
        const QColor green = LabelColors::rgb(LabelColor::Green);
        const QIcon folder = QIcon::fromTheme(QStringLiteral("folder"));
        const QIcon badged = FolderColors::decorate(folder, QStringLiteral("folder"), {}, green, FolderColors::Style::Badge);
        QCOMPARE(colorAt(badged, 48), RegularColor);
        QCOMPARE(pixelAt(badged, 48, 38, 38), green);
        QCOMPARE(pixelAt(badged, 48, 2, 2), RegularColor);
    }

    void leavesIconsUnchangedWhenNotShown()
    {
        const QColor red = LabelColors::rgb(LabelColor::Red);
        const QIcon folder = QIcon::fromTheme(QStringLiteral("folder"));
        QCOMPARE(FolderColors::decorate(folder, QStringLiteral("folder"), {}, red, FolderColors::Style::Name).cacheKey(), folder.cacheKey());
        QCOMPARE(FolderColors::decorate(folder, QStringLiteral("folder"), {}, red, FolderColors::Style::Hidden).cacheKey(), folder.cacheKey());
        QCOMPARE(FolderColors::decorate(folder, QStringLiteral("folder"), {}, QColor(), FolderColors::Style::Icon).cacheKey(), folder.cacheKey());

        QPixmap thumbnail(48, 48);
        thumbnail.fill(Qt::cyan);
        const QIcon preview(thumbnail);
        QCOMPARE(FolderColors::decorate(preview, QStringLiteral("folder"), {}, red, FolderColors::Style::Icon).cacheKey(), preview.cacheKey());
    }

    void dotIsFilledWithColour()
    {
        const QColor purple = LabelColors::rgb(LabelColor::Purple);
        const QIcon dot = FolderColors::dot(purple);
        QVERIFY(!dot.isNull());
        QCOMPARE(colorAt(dot, 16), purple);
        QVERIFY(FolderColors::dot(QColor()).isNull());
    }

    void followsConfiguredStyleForItems()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
        });
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QColor yellow = LabelColors::rgb(LabelColor::Yellow);
        QVERIFY(FolderMetadata::setFolderColor(folder.path(), yellow.name()));
        FolderColors::clearCache();
        const KFileItem item(QUrl::fromLocalFile(folder.path()), QStringLiteral("inode/directory"), S_IFDIR);
        QVERIFY(item.isDir());
        const QIcon icon = QIcon::fromTheme(QStringLiteral("folder"));

        Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Hidden);
        QCOMPARE(FolderColors::configured(), FolderColors::Style::Hidden);
        QCOMPARE(FolderColors::decorate(icon, item).cacheKey(), icon.cacheKey());
        QVERIFY(!FolderColors::nameColor(item, Qt::white).isValid());

        Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Badge);
        QVERIFY(FolderColors::decorate(icon, item).cacheKey() != icon.cacheKey());
        QVERIFY(!FolderColors::nameColor(item, Qt::white).isValid());

        Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Name);
        QCOMPARE(FolderColors::decorate(icon, item).cacheKey(), icon.cacheKey());
        const QColor name = FolderColors::nameColor(item, Qt::white);
        QVERIFY(name.isValid());
        QVERIFY2(name != yellow, "light folder colours are darkened to stay readable on a light background");

        const KFileItem file(QUrl::fromLocalFile(folder.filePath(QStringLiteral("file.txt"))), QStringLiteral("text/plain"), S_IFREG);
        QVERIFY(!FolderColors::nameColor(file, Qt::white).isValid());
    }

private:
    QTemporaryDir m_themeDir;
};

QTEST_MAIN(FolderColorsTest)
#include "foldercolorstest.moc"
