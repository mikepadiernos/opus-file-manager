// SPDX-License-Identifier: GPL-3.0-or-later

#include "ark.h"

#include <QJsonDocument>
#include <QSet>
#include <QTest>

namespace
{
QJsonObject plugin(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

// Shaped like Ark's kerfuffle_cli7z.json.
const char cli7z[] = R"({
    "KPlugin": {"Id": "kerfuffle_cli7z", "MimeTypes": ["application/x-7z-compressed", "application/zip"]},
    "X-KDE-Kerfuffle-ReadOnlyExecutables": ["7z"],
    "X-KDE-Kerfuffle-ReadWrite": true,
    "X-KDE-Kerfuffle-ReadWriteExecutables": ["7z"],
    "application/x-7z-compressed": {"EncryptionMethods": ["AES256"], "HeaderEncryption": true, "SupportsMultiVolume": true, "SupportsTesting": true},
    "application/zip": {"Encryption": true, "EncryptionMethods": ["AES256", "ZipCrypto"], "SupportsMultiVolume": true, "SupportsTesting": true}
})";

// Shaped like Ark's kerfuffle_clirar.json: unrar reads, rar writes.
const char clirar[] = R"({
    "KPlugin": {"Id": "kerfuffle_clirar", "MimeTypes": ["application/vnd.rar"]},
    "X-KDE-Kerfuffle-ReadOnlyExecutables": ["unrar"],
    "X-KDE-Kerfuffle-ReadWrite": true,
    "X-KDE-Kerfuffle-ReadWriteExecutables": ["rar"],
    "application/vnd.rar": {"EncryptionMethods": ["AES128"], "HeaderEncryption": true, "SupportsMultiVolume": true}
})";

const char libarchiveReadOnly[] = R"({
    "KPlugin": {"Id": "kerfuffle_libarchive_readonly", "MimeTypes": ["application/x-iso9660-image", "application/vnd.debian.binary-package"]}
})";

const char disabled[] = R"({
    "KPlugin": {"Id": "kerfuffle_disabled", "EnabledByDefault": false, "MimeTypes": ["application/x-arj"]},
    "X-KDE-Kerfuffle-ReadWrite": true
})";

std::function<bool(const QString &)> installed(const QStringList &programs)
{
    return [programs](const QString &program) {
        return programs.contains(program);
    };
}
}

class ArkTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsAndWritesWithTheirExecutables()
    {
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(cli7z), plugin(clirar)}, installed({QStringLiteral("7z"), QStringLiteral("unrar")}));
        const Ark::FormatSupport sevenZip = backends.support(QStringLiteral("application/x-7z-compressed"));
        QVERIFY(sevenZip.read);
        QVERIFY(sevenZip.write);
        QVERIFY(sevenZip.encryption);
        QVERIFY(sevenZip.headerEncryption);
        QVERIFY(sevenZip.multiVolume);
        QVERIFY(sevenZip.testing);
        QCOMPARE(sevenZip.plugins, QStringList{QStringLiteral("kerfuffle_cli7z")});
        QVERIFY(backends.support(QStringLiteral("application/zip")).encryption);
        QVERIFY(!backends.support(QStringLiteral("application/zip")).headerEncryption);

        const Ark::FormatSupport rar = backends.support(QStringLiteral("application/vnd.rar"));
        QVERIFY(rar.read);
        QVERIFY(!rar.write);
        QVERIFY(!rar.encryption);
        QCOMPARE(backends.readOnlyMimeTypes(), QStringList{QStringLiteral("application/vnd.rar")});
        QCOMPARE(backends.writableMimeTypes(), (QStringList{QStringLiteral("application/x-7z-compressed"), QStringLiteral("application/zip")}));
    }

    void writesRarWhenRarIsInstalled()
    {
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(clirar)}, installed({QStringLiteral("unrar"), QStringLiteral("rar")}));
        QVERIFY(backends.canWrite(QStringLiteral("application/vnd.rar")));
        QVERIFY(backends.readOnlyMimeTypes().isEmpty());
    }

    void skipsPluginsMissingTheirReadExecutables()
    {
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(cli7z), plugin(clirar)}, installed({}));
        QVERIFY(backends.isEmpty());
        QVERIFY(!backends.canRead(QStringLiteral("application/zip")));
    }

    void readOnlyPluginsOnlyRead()
    {
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(libarchiveReadOnly)}, installed({}));
        QVERIFY(backends.canRead(QStringLiteral("application/x-iso9660-image")));
        QVERIFY(!backends.canWrite(QStringLiteral("application/x-iso9660-image")));
        QCOMPARE(backends.readableMimeTypes().size(), 2);
    }

    void skipsDisabledPlugins()
    {
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(disabled)}, installed({}));
        QVERIFY(!backends.canRead(QStringLiteral("application/x-arj")));
    }

    void mergesPluginsForTheSameFormat()
    {
        const char libzipReadOnly[] = R"({"KPlugin": {"Id": "kerfuffle_libzip", "MimeTypes": ["application/zip"]}})";
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(libzipReadOnly), plugin(cli7z)}, installed({QStringLiteral("7z")}));
        const Ark::FormatSupport zip = backends.support(QStringLiteral("application/zip"));
        QVERIFY(zip.write);
        QCOMPARE(QSet<QString>(zip.plugins.cbegin(), zip.plugins.cend()),
                 (QSet<QString>{QStringLiteral("kerfuffle_libzip"), QStringLiteral("kerfuffle_cli7z")}));
    }

    void resolvesMimeAliases()
    {
        const Ark::Backends backends = Ark::Backends::fromPlugins({plugin(cli7z)}, installed({QStringLiteral("7z")}));
        QVERIFY(backends.canWrite(QStringLiteral("application/x-zip-compressed")));
        QVERIFY(!backends.canRead(QStringLiteral("text/plain")));
        QVERIFY(!backends.canRead(QStringLiteral("not/a-type")));
    }

    void passesPathsAsSeparateArguments()
    {
        const QString archive = QStringLiteral("/tmp/-odd name $(x).rar");
        QCOMPARE(Ark::openArguments(archive), (QStringList{QStringLiteral("--"), archive}));
        QCOMPARE(Ark::extractArguments(archive, QStringLiteral("/tmp/out dir")),
                 (QStringList{QStringLiteral("--batch"), QStringLiteral("--destination"), QStringLiteral("/tmp/out dir"), QStringLiteral("--"), archive}));
        const QStringList sources{QStringLiteral("/tmp/-a b"), QStringLiteral("/tmp/c")};
        QCOMPARE(Ark::compressArguments(sources),
                 (QStringList{QStringLiteral("--changetofirstpath"),
                              QStringLiteral("--add"),
                              QStringLiteral("--dialog"),
                              QStringLiteral("--"),
                              QStringLiteral("/tmp/-a b"),
                              QStringLiteral("/tmp/c")}));
        QCOMPARE(Ark::addToArguments(archive, sources),
                 (QStringList{QStringLiteral("--changetofirstpath"),
                              QStringLiteral("--add-to"),
                              archive,
                              QStringLiteral("--"),
                              QStringLiteral("/tmp/-a b"),
                              QStringLiteral("/tmp/c")}));
    }
};

QTEST_GUILESS_MAIN(ArkTest)

#include "arktest.moc"
