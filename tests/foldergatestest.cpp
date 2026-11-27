// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldergates.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

class FolderGatesTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_root;

    KSharedConfigPtr config(const QString &name)
    {
        return KSharedConfig::openConfig(m_dir.filePath(name), KConfig::SimpleConfig);
    }

    QString makeFolder(const QString &relative)
    {
        const QString path = m_root + u'/' + relative;
        QDir().mkpath(path);
        return path;
    }

    std::unique_ptr<FolderGates> gates(const QString &configName)
    {
        auto result = std::make_unique<FolderGates>(config(configName));
        result->setIterations(10);
        return result;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_root = QFileInfo(m_dir.path()).canonicalFilePath() + QStringLiteral("/tree");
        QDir().mkpath(m_root);
    }

    void verifierRoundTrip()
    {
        const QString verifier = FolderGates::makeVerifier(QStringLiteral("sésame"), 50);
        QVERIFY(verifier.startsWith(QLatin1String("pbkdf2-sha256$50$")));
        QVERIFY(!verifier.contains(QLatin1String("sésame")));
        QCOMPARE(FolderGates::verify(verifier, QStringLiteral("sésame")), FolderGates::Check::Unlocked);
        // Composed and decomposed forms of the same text are the same password.
        QCOMPARE(FolderGates::verify(verifier, QStringLiteral("se\u0301same")), FolderGates::Check::Unlocked);
        QCOMPARE(FolderGates::verify(verifier, QStringLiteral("sesame")), FolderGates::Check::Wrong);
        QVERIFY(FolderGates::makeVerifier(QStringLiteral("a"), 5) != FolderGates::makeVerifier(QStringLiteral("a"), 5));
    }

    void damagedVerifiers_data()
    {
        QTest::addColumn<QString>("verifier");
        QTest::newRow("empty") << QString();
        QTest::newRow("scheme") << QStringLiteral("md5$1$c2FsdA==$aGFzaA==");
        QTest::newRow("iterations") << QStringLiteral("pbkdf2-sha256$x$c2FsdA==$aGFzaA==");
        QTest::newRow("zero iterations") << QStringLiteral("pbkdf2-sha256$0$c2FsdA==$aGFzaA==");
        QTest::newRow("base64") << QStringLiteral("pbkdf2-sha256$10$!!$aGFzaA==");
        QTest::newRow("hash size") << QStringLiteral("pbkdf2-sha256$10$c2FsdA==$aGFzaA==");
    }

    void damagedVerifiers()
    {
        QFETCH(QString, verifier);
        QCOMPARE(FolderGates::verify(verifier, QStringLiteral("a")), FolderGates::Check::Damaged);
    }

    void localPaths()
    {
        QCOMPARE(FolderGates::localPath(QUrl::fromLocalFile(QStringLiteral("/a/b/../c"))), QStringLiteral("/a/c"));
        QCOMPARE(FolderGates::localPath(QUrl(QStringLiteral("admin:/etc/x"))), QStringLiteral("/etc/x"));
        QCOMPARE(FolderGates::localPath(QUrl(QStringLiteral("zip:/home/me/a.zip/inner"))), QStringLiteral("/home/me/a.zip/inner"));
        QUrl search(QStringLiteral("filenamesearch:"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("search"), QStringLiteral("x"));
        query.addQueryItem(QStringLiteral("url"), QUrl::fromLocalFile(QStringLiteral("/srv/data")).toString());
        search.setQuery(query);
        QCOMPARE(FolderGates::localPath(search), QStringLiteral("/srv/data"));
        QVERIFY(FolderGates::localPath(QUrl(QStringLiteral("trash:/x"))).isEmpty());
        QVERIFY(FolderGates::localPath(QUrl(QStringLiteral("sftp://host/x"))).isEmpty());
    }

    void canonicalPaths()
    {
        const QString real = makeFolder(QStringLiteral("canonical/real"));
        QVERIFY(QFile::link(real, m_root + QStringLiteral("/canonical/link")));
        QCOMPARE(FolderGates::canonicalPath(m_root + QStringLiteral("/canonical/link")), real);
        QCOMPARE(FolderGates::canonicalPath(m_root + QStringLiteral("/canonical/link/missing/x")), real + QStringLiteral("/missing/x"));
    }

    void protectAndUnlock()
    {
        const QString secret = makeFolder(QStringLiteral("protect/secret [1], x"));
        makeFolder(QStringLiteral("protect/secret [1], x/inner"));
        makeFolder(QStringLiteral("protect/secretive"));
        auto g = gates(QStringLiteral("protectrc"));
        QSignalSpy changed(g.get(), &FolderGates::changed);

        QVERIFY(!g->protect(secret, QString()).isEmpty());
        QVERIFY(g->protect(secret, QStringLiteral("pw")).isEmpty());
        QCOMPARE(changed.count(), 1);
        QVERIFY(!g->protect(secret, QStringLiteral("pw")).isEmpty());
        QCOMPARE(g->folders(), QStringList{secret});
        QVERIFY(g->hasLocked());

        const QUrl gate = QUrl::fromLocalFile(secret);
        const QUrl inner = QUrl::fromLocalFile(secret + QStringLiteral("/inner"));
        const QUrl sibling = QUrl::fromLocalFile(m_root + QStringLiteral("/protect/secretive"));
        const QUrl parent = QUrl::fromLocalFile(m_root + QStringLiteral("/protect"));
        QVERIFY(g->isLocked(gate));
        QVERIFY(g->isLocked(inner));
        QVERIFY(!g->isLocked(sibling));
        QVERIFY(!g->hidesContents(gate));
        QVERIFY(g->hidesContents(inner));
        QVERIFY(!g->hidesContents(sibling));
        QCOMPARE(g->lockedGateFor(inner), secret);
        QCOMPARE(g->lockedWithin(parent), QStringList{secret});
        QVERIFY(g->lockedWithin(sibling).isEmpty());
        QVERIFY(g->touchesLocked(parent));
        QCOMPARE(g->gateAt(gate), secret);
        QVERIFY(g->gateAt(inner).isEmpty());
        QVERIFY(g->hidesContents(QUrl(QStringLiteral("zip:") + secret + QStringLiteral("/a.zip/b"))));

        QCOMPARE(g->unlock(secret, QStringLiteral("nope")), FolderGates::Check::Wrong);
        QVERIFY(g->isLocked(inner));
        QCOMPARE(g->unlock(secret, QStringLiteral("pw")), FolderGates::Check::Unlocked);
        QVERIFY(!g->isLocked(inner));
        QVERIFY(!g->hidesContents(inner));
        QVERIFY(g->hasUnlocked());
        QVERIFY(!g->hasLocked());
        g->lockAll();
        QVERIFY(g->isLocked(inner));

        // Another instance reads the stored gate, locked, and the password is not stored.
        auto reread = std::make_unique<FolderGates>(config(QStringLiteral("protectrc")));
        QCOMPARE(reread->folders(), QStringList{secret});
        QVERIFY(reread->isLocked(inner));
        QFile file(m_dir.filePath(QStringLiteral("protectrc")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("pw\n"));
    }

    void symbolicLinksReachGates()
    {
        const QString secret = makeFolder(QStringLiteral("links/secret/inner"));
        QVERIFY(QFile::link(m_root + QStringLiteral("/links/secret"), m_root + QStringLiteral("/links/alias")));
        auto g = gates(QStringLiteral("linksrc"));
        QVERIFY(g->protect(m_root + QStringLiteral("/links/alias"), QStringLiteral("pw")).isEmpty());
        QCOMPARE(g->folders(), QStringList{m_root + QStringLiteral("/links/secret")});
        QVERIFY(g->hidesContents(QUrl::fromLocalFile(m_root + QStringLiteral("/links/alias/inner"))));
        QVERIFY(g->hidesContents(QUrl::fromLocalFile(secret)));
    }

    void refusesUnsafeFolders()
    {
        auto g = gates(QStringLiteral("refusesrc"));
        QVERIFY(!g->protect(QStringLiteral("/"), QStringLiteral("pw")).isEmpty());
        QVERIFY(!g->protect(QDir::homePath(), QStringLiteral("pw")).isEmpty());
        QVERIFY(!g->protect(QFileInfo(QDir::homePath()).absolutePath(), QStringLiteral("pw")).isEmpty());
        QVERIFY(!g->protect(m_root + QStringLiteral("/missing"), QStringLiteral("pw")).isEmpty());
        QVERIFY(!g->protect(QStringLiteral("relative"), QStringLiteral("pw")).isEmpty());
        QVERIFY(g->folders().isEmpty());
    }

    void changeAndRemove()
    {
        const QString secret = makeFolder(QStringLiteral("change/secret"));
        auto g = gates(QStringLiteral("changerc"));
        QVERIFY(g->protect(secret, QStringLiteral("old")).isEmpty());
        QVERIFY(!g->changePassword(secret, QStringLiteral("wrong"), QStringLiteral("new")).isEmpty());
        QVERIFY(!g->changePassword(secret, QStringLiteral("old"), QString()).isEmpty());
        QVERIFY(g->changePassword(secret, QStringLiteral("old"), QStringLiteral("new")).isEmpty());
        QCOMPARE(g->unlock(secret, QStringLiteral("old")), FolderGates::Check::Wrong);
        QVERIFY(!g->removeProtection(secret, QStringLiteral("old")).isEmpty());
        QCOMPARE(g->folders(), QStringList{secret});
        QVERIFY(g->removeProtection(secret, QStringLiteral("new")).isEmpty());
        QVERIFY(g->folders().isEmpty());
        QVERIFY(!g->isLocked(QUrl::fromLocalFile(secret)));
        QVERIFY(std::make_unique<FolderGates>(config(QStringLiteral("changerc")))->folders().isEmpty());
    }

    void damagedGateStaysLocked()
    {
        const QString secret = makeFolder(QStringLiteral("damaged/secret"));
        KSharedConfigPtr c = config(QStringLiteral("damagedrc"));
        c->group(QStringLiteral("FolderGates")).writeEntry("Gates", QStringList{secret + QStringLiteral("\tgarbage")});
        c->sync();
        FolderGates g(c);
        QVERIFY(g.isLocked(QUrl::fromLocalFile(secret)));
        QCOMPARE(g.unlock(secret, QStringLiteral("garbage")), FolderGates::Check::Damaged);
        QVERIFY(g.isLocked(QUrl::fromLocalFile(secret)));
        QVERIFY(!g.removeProtection(secret, QStringLiteral("garbage")).isEmpty());
    }

    void nestedGates()
    {
        const QString outer = makeFolder(QStringLiteral("nested/outer"));
        const QString inner = makeFolder(QStringLiteral("nested/outer/inner"));
        auto g = gates(QStringLiteral("nestedrc"));
        QVERIFY(g->protect(outer, QStringLiteral("a")).isEmpty());
        QVERIFY(g->protect(inner, QStringLiteral("b")).isEmpty());
        const QUrl deep = QUrl::fromLocalFile(inner + QStringLiteral("/x"));
        QCOMPARE(g->lockedGateFor(deep), outer);
        QCOMPARE(g->unlock(outer, QStringLiteral("a")), FolderGates::Check::Unlocked);
        QCOMPARE(g->lockedGateFor(deep), inner);
        QVERIFY(!g->hidesContents(QUrl::fromLocalFile(inner)));
        QVERIFY(g->hidesContents(deep));
        g->lock(outer);
        QCOMPARE(g->lockedWithin(QUrl::fromLocalFile(m_root + QStringLiteral("/nested"))), (QStringList{outer, inner}));
    }

    void relocate()
    {
        const QString secret = makeFolder(QStringLiteral("relocate/a/secret"));
        makeFolder(QStringLiteral("relocate/b"));
        auto g = gates(QStringLiteral("relocaterc"));
        QVERIFY(g->protect(secret, QStringLiteral("pw")).isEmpty());
        QCOMPARE(g->unlock(secret, QStringLiteral("pw")), FolderGates::Check::Unlocked);
        QVERIFY(QDir().rename(m_root + QStringLiteral("/relocate/a"), m_root + QStringLiteral("/relocate/b/a")));
        g->relocate(m_root + QStringLiteral("/relocate/a"), m_root + QStringLiteral("/relocate/b/a"));
        const QString moved = m_root + QStringLiteral("/relocate/b/a/secret");
        QCOMPARE(g->folders(), QStringList{moved});
        QVERIFY(!g->isGateLocked(moved));
        QCOMPARE(std::make_unique<FolderGates>(config(QStringLiteral("relocaterc")))->folders(), QStringList{moved});
    }

    void otherUrlsAreUnaffected()
    {
        const QString secret = makeFolder(QStringLiteral("other/secret"));
        auto g = gates(QStringLiteral("otherrc"));
        QVERIFY(g->protect(secret, QStringLiteral("pw")).isEmpty());
        QVERIFY(!g->isLocked(QUrl(QStringLiteral("trash:/secret"))));
        QVERIFY(!g->hidesContents(QUrl()));
    }

    void defaultIterations()
    {
        const QString verifier = FolderGates::makeVerifier(QStringLiteral("pw"));
        QVERIFY(verifier.startsWith(QStringLiteral("pbkdf2-sha256$%1$").arg(FolderGates::DefaultIterations)));
    }
};

QTEST_GUILESS_MAIN(FolderGatesTest)

#include "foldergatestest.moc"
