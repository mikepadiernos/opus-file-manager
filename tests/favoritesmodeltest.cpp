// SPDX-License-Identifier: GPL-3.0-or-later

#include "favoritesmodel.h"

#include <KConfig>

#include <QMimeData>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class FavoritesModelTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    const QUrl m_a = QUrl::fromLocalFile(QStringLiteral("/data/a"));
    const QUrl m_b = QUrl::fromLocalFile(QStringLiteral("/data/b"));
    const QUrl m_c = QUrl::fromLocalFile(QStringLiteral("/data/c"));

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("favoritesrc"));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        QFile::remove(configPath());
    }

    void addAppendsAndRefusesDuplicates()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        QVERIFY(model.add(m_a));
        QVERIFY(model.add(m_b));
        QVERIFY(!model.add(m_a));
        QVERIFY(!model.add(QUrl()));
        QCOMPARE(model.urls(), QList<QUrl>({m_a, m_b}));
        QVERIFY(model.contains(m_b));
        QVERIFY(!model.contains(m_c));
    }

    void addAtRow()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        model.add(m_a);
        model.add(m_b);
        QVERIFY(model.add(m_c, 1));
        QCOMPARE(model.urls(), QList<QUrl>({m_a, m_c, m_b}));
    }

    void defaultTitles()
    {
        QCOMPARE(FavoritesModel::defaultTitle(QUrl::fromLocalFile(QStringLiteral("/home/me/Music"))), QStringLiteral("Music"));
        QCOMPARE(FavoritesModel::defaultTitle(QUrl::fromLocalFile(QStringLiteral("/home/me/Music/"))), QStringLiteral("Music"));
        QCOMPARE(FavoritesModel::defaultTitle(QUrl::fromLocalFile(QStringLiteral("/"))), QStringLiteral("/"));
    }

    void renameAndRestoreDefaultTitle()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        model.add(m_a);
        const QModelIndex index = model.index(0);
        QCOMPARE(index.data().toString(), QStringLiteral("a"));
        QVERIFY(index.flags() & Qt::ItemIsEditable);

        QVERIFY(model.setData(index, QStringLiteral("Archive")));
        QCOMPARE(index.data().toString(), QStringLiteral("Archive"));
        QCOMPARE(index.data(FavoritesModel::CustomTitleRole).toString(), QStringLiteral("Archive"));

        QVERIFY(model.setData(index, QStringLiteral("  ")));
        QCOMPARE(index.data().toString(), QStringLiteral("a"));
        QVERIFY(index.data(FavoritesModel::CustomTitleRole).toString().isEmpty());
        QCOMPARE(index.data(FavoritesModel::UrlRole).toUrl(), m_a);
    }

    void removeFavorites()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        model.add(m_a);
        model.add(m_b);
        model.add(m_c);
        QVERIFY(model.remove(m_b));
        QVERIFY(!model.remove(m_b));
        QVERIFY(model.removeRows(0, 1));
        QCOMPARE(model.urls(), QList<QUrl>({m_c}));
    }

    void moveRowsReorders()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        model.add(m_a);
        model.add(m_b);
        model.add(m_c);
        QVERIFY(model.moveRows(QModelIndex(), 2, 1, QModelIndex(), 0));
        QCOMPARE(model.urls(), QList<QUrl>({m_c, m_a, m_b}));
        QVERIFY(model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3));
        QCOMPARE(model.urls(), QList<QUrl>({m_a, m_b, m_c}));
    }

    void everyChangeIsSaved()
    {
        {
            KConfig config(configPath(), KConfig::SimpleConfig);
            FavoritesModel model(config.group(QStringLiteral("Favorites")));
            model.add(m_a);
            model.add(m_b);
            model.setData(model.index(1), QStringLiteral("Backups"));
            model.moveRows(QModelIndex(), 1, 1, QModelIndex(), 0);
        }
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel loaded(config.group(QStringLiteral("Favorites")));
        QCOMPARE(loaded.urls(), QList<QUrl>({m_b, m_a}));
        QCOMPARE(loaded.index(0).data().toString(), QStringLiteral("Backups"));
        QCOMPARE(loaded.index(1).data().toString(), QStringLiteral("a"));
    }

    void dragEncodesUrls()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        model.add(m_a);
        model.add(m_b);
        std::unique_ptr<QMimeData> mime(model.mimeData({model.index(1)}));
        QVERIFY(mime);
        QCOMPARE(mime->urls(), QList<QUrl>({m_b}));
        QVERIFY(model.supportedDragActions() & Qt::CopyAction);
    }

    void dropMovesFavoritesAndAddsOthers()
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        model.add(m_a);
        model.add(m_b);
        QMimeData mime;
        mime.setUrls({m_b, m_c});
        QVERIFY(model.canDropMimeData(&mime, Qt::CopyAction, 0, 0, QModelIndex()));
        QVERIFY(model.dropMimeData(&mime, Qt::CopyAction, 0, 0, QModelIndex()));
        QCOMPARE(model.urls(), QList<QUrl>({m_b, m_c, m_a}));

        QMimeData append;
        append.setUrls({m_a});
        QVERIFY(model.dropMimeData(&append, Qt::CopyAction, -1, -1, QModelIndex()));
        QCOMPARE(model.urls(), QList<QUrl>({m_b, m_c, m_a}));
    }

    void dropIgnoresLocalFiles()
    {
        QFile file(m_dir.filePath(QStringLiteral("note.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        KConfig config(configPath(), KConfig::SimpleConfig);
        FavoritesModel model(config.group(QStringLiteral("Favorites")));
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(file.fileName()), QUrl::fromLocalFile(m_dir.path())});
        model.dropMimeData(&mime, Qt::CopyAction, -1, -1, QModelIndex());
        QCOMPARE(model.urls(), QList<QUrl>({QUrl::fromLocalFile(m_dir.path())}));
    }
};

QTEST_GUILESS_MAIN(FavoritesModelTest)

#include "favoritesmodeltest.moc"
