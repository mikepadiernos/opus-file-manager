// SPDX-License-Identifier: GPL-3.0-or-later

#include "appidentity.h"

#include <QFile>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QStandardPaths>
#include <QTest>
#include <QXmlStreamReader>

#include <algorithm>

namespace
{
/// The [Desktop Entry] keys of a desktop file, including localized keys such as Name[fa].
QHash<QString, QString> desktopEntry(const QString &path)
{
    QHash<QString, QString> entries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entries;
    }
    bool inEntry = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = line == QLatin1String("[Desktop Entry]");
        } else if (inEntry && line.contains(QLatin1Char('=')) && !line.startsWith(QLatin1Char('#'))) {
            const qsizetype separator = line.indexOf(QLatin1Char('='));
            entries.insert(line.left(separator).trimmed(), line.mid(separator + 1).trimmed());
        }
    }
    return entries;
}

/// The AppStream values that identify the application, keyed by element and
/// attribute, such as "name", "name[fa]", "url[homepage]" and "release".
QHash<QString, QString> metainfo(const QString &path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return values;
    }
    QXmlStreamReader xml(&file);
    QStringList elements;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) {
            elements.removeLast();
            continue;
        }
        if (!xml.isStartElement()) {
            continue;
        }
        elements.append(xml.name().toString());
        const QString parent = elements.size() >= 2 ? elements.at(elements.size() - 2) : QString();
        const QString name = elements.constLast();
        const QString language = xml.attributes().value(QStringLiteral("xml:lang")).toString();
        QString key;
        if (parent == QLatin1String("component")) {
            if (name == QLatin1String("url") || name == QLatin1String("launchable")) {
                key = name + QLatin1Char('[') + xml.attributes().value(QStringLiteral("type")).toString() + QLatin1Char(']');
            } else if (name != QLatin1String("description") && name != QLatin1String("screenshots") && name != QLatin1String("releases")
                       && name != QLatin1String("provides") && name != QLatin1String("developer")) {
                key = language.isEmpty() ? name : name + QLatin1Char('[') + language + QLatin1Char(']');
            }
        } else if (parent == QLatin1String("provides") && name == QLatin1String("binary")) {
            key = name;
        } else if (parent == QLatin1String("releases") && name == QLatin1String("release") && !values.contains(QStringLiteral("release"))) {
            values.insert(QStringLiteral("release"), xml.attributes().value(QStringLiteral("version")).toString());
        }
        if (!key.isEmpty() && !values.contains(key)) {
            const QString text = xml.readElementText(QXmlStreamReader::SkipChildElements);
            elements.removeLast();
            values.insert(key, text);
        }
    }
    if (xml.hasError()) {
        values.insert(QStringLiteral("error"), xml.errorString());
    }
    return values;
}
}

class AppIdentityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void aboutData()
    {
        const KAboutData about = AppIdentity::aboutData(QStringLiteral(OPUSFM_PROJECT_VERSION));
        QCOMPARE(about.componentName(), QStringLiteral("opus-file-manager"));
        QCOMPARE(about.displayName(), QStringLiteral("Plasma Opus"));
        QCOMPARE(about.shortDescription(), QStringLiteral("Dual-pane file manager for KDE Plasma"));
        QCOMPARE(about.version(), QStringLiteral(OPUSFM_PROJECT_VERSION));
        QCOMPARE(about.desktopFileName(), AppIdentity::desktopFileName());
        QCOMPARE(about.homepage(), AppIdentity::homepage());
        QCOMPARE(about.bugAddress(), AppIdentity::bugAddress());
        QCOMPARE(int(about.licenses().constFirst().key()), int(KAboutLicense::GPL_V3));
        // KAboutApplicationDialog prefers a pixmap or image logo; an icon keeps every size sharp.
        QCOMPARE(about.programLogo().metaType(), QMetaType::fromType<QIcon>());
        QVERIFY(!about.programLogo().canConvert<QPixmap>());
        QVERIFY(!about.programLogo().canConvert<QImage>());

        QStringList domain = about.organizationDomain().split(QLatin1Char('.'));
        std::reverse(domain.begin(), domain.end());
        QCOMPARE(domain.join(QLatin1Char('.')) + QLatin1Char('.') + about.componentName(), AppIdentity::dbusServiceName());
    }

    void desktopEntryMatches()
    {
        const QHash<QString, QString> entry = desktopEntry(QStringLiteral(OPUSFM_DESKTOP_FILE));
        QVERIFY2(!entry.isEmpty(), OPUSFM_DESKTOP_FILE);
        QCOMPARE(entry.value(QStringLiteral("Name")), AppIdentity::displayName());
        QCOMPARE(entry.value(QStringLiteral("Comment")), AppIdentity::summary());
        QCOMPARE(entry.value(QStringLiteral("Icon")), AppIdentity::desktopFileName());
        QCOMPARE(entry.value(QStringLiteral("StartupWMClass")), AppIdentity::componentName());
        QCOMPARE(entry.value(QStringLiteral("X-DBUS-ServiceName")), AppIdentity::dbusServiceName());
        QVERIFY(entry.value(QStringLiteral("Exec")).endsWith(QLatin1Char('/') + AppIdentity::componentName() + QStringLiteral(" %U")));
    }

    void desktopEntryIsTranslated()
    {
        const QHash<QString, QString> entry = desktopEntry(QStringLiteral(OPUSFM_DESKTOP_FILE));
        QCOMPARE(entry.value(QStringLiteral("Name[fa]")), QString::fromUtf8("پلاسما اوپوس"));
        QCOMPARE(entry.value(QStringLiteral("GenericName[fa]")), QString::fromUtf8("مدیر پروندهٔ دو قسمتی"));
        QCOMPARE(entry.value(QStringLiteral("Comment[fa]")),
                 QString::fromUtf8("مدیر پروندهٔ دو قسمتی برای کی‌دی‌ای پلاسما"));
    }

    void metainfoMatches()
    {
        const QHash<QString, QString> values = metainfo(QStringLiteral(OPUSFM_METAINFO_FILE));
        QVERIFY2(!values.contains(QStringLiteral("error")), qPrintable(values.value(QStringLiteral("error"))));
        QCOMPARE(values.value(QStringLiteral("id")), AppIdentity::desktopFileName());
        QCOMPARE(values.value(QStringLiteral("name")), AppIdentity::displayName());
        QCOMPARE(values.value(QStringLiteral("summary")), AppIdentity::summary());
        QCOMPARE(values.value(QStringLiteral("launchable[desktop-id]")), AppIdentity::desktopFileName() + QStringLiteral(".desktop"));
        QCOMPARE(values.value(QStringLiteral("url[homepage]")), AppIdentity::homepage());
        QCOMPARE(values.value(QStringLiteral("url[bugtracker]")), AppIdentity::bugAddress());
        QCOMPARE(values.value(QStringLiteral("binary")), AppIdentity::componentName());
        QCOMPARE(values.value(QStringLiteral("project_license")), QStringLiteral("GPL-3.0-or-later"));
        // Add a release entry when the project version changes.
        QCOMPARE(values.value(QStringLiteral("release")), QStringLiteral(OPUSFM_PROJECT_VERSION));
    }

    void metainfoIsTranslated()
    {
        const QHash<QString, QString> values = metainfo(QStringLiteral(OPUSFM_METAINFO_FILE));
        QCOMPARE(values.value(QStringLiteral("name[fa]")), QString::fromUtf8("پلاسما اوپوس"));
        QCOMPARE(values.value(QStringLiteral("summary[fa]")),
                 QString::fromUtf8("مدیر پروندهٔ دو قسمتی برای کی‌دی‌ای پلاسما"));
    }
};

QTEST_MAIN(AppIdentityTest)

#include "appidentitytest.moc"
