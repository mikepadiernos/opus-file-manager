// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcutstorage.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDomDocument>
#include <QStandardPaths>
#include <QTest>

class ShortcutStorageTest : public QObject
{
    Q_OBJECT

private:
    const QKeySequence f5{Qt::Key_F5};
    const QKeySequence space{Qt::Key_Space};
    const QKeySequence ctrlKG{QKeyCombination(Qt::CTRL | Qt::Key_K), QKeyCombination(Qt::Key_G)};

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void textRoundTripsAlternatesAndKeySequences()
    {
        const QList<QKeySequence> shortcuts{f5, ctrlKG, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Delete)};
        const QString text = ShortcutStorage::toText(shortcuts);
        // The format of KActionCollection::writeSettings: portable text, primary first.
        QCOMPARE(text, QStringLiteral("F5; Ctrl+K, G; Meta+Shift+Del"));
        QCOMPARE(ShortcutStorage::fromText(text), std::optional(shortcuts));
        // Empty sequences are left out.
        QCOMPARE(ShortcutStorage::toText({QKeySequence(), f5, QKeySequence()}), QStringLiteral("F5"));
    }

    void noneMeansNoShortcut()
    {
        QCOMPARE(ShortcutStorage::toText({}), QStringLiteral("none"));
        QCOMPARE(ShortcutStorage::toText({QKeySequence()}), QStringLiteral("none"));
        QCOMPARE(ShortcutStorage::fromText(QStringLiteral("none")), std::optional(QList<QKeySequence>()));
    }

    void unreadableTextGivesTheDefaults()
    {
        QVERIFY(!ShortcutStorage::fromText(QString()).has_value());
        QVERIFY(!ShortcutStorage::fromText(QStringLiteral("   ")).has_value());
        QVERIFY(!ShortcutStorage::fromText(QStringLiteral("Ctrl+Nonsense")).has_value());
        // One unreadable shortcut makes the whole entry unreadable, rather than dropping it.
        QVERIFY(!ShortcutStorage::fromText(QStringLiteral("F5; Ctrl+Nonsense")).has_value());
    }

    void entriesExistOnlyForShortcutsOtherThanTheDefaults()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QLatin1String(ShortcutStorage::Group));

        ShortcutStorage::write(group, QStringLiteral("open_gallery"), {f5, ctrlKG}, {space});
        QCOMPARE(group.readEntry("open_gallery", QString()), QStringLiteral("F5; Ctrl+K, G"));
        QCOMPARE(ShortcutStorage::read(group, QStringLiteral("open_gallery")), std::optional(QList<QKeySequence>{f5, ctrlKG}));

        ShortcutStorage::write(group, QStringLiteral("search_folder"), {}, {space});
        QCOMPARE(group.readEntry("search_folder", QString()), QStringLiteral("none"));
        QCOMPARE(ShortcutStorage::read(group, QStringLiteral("search_folder")), std::optional(QList<QKeySequence>()));

        // Going back to the defaults removes the entry; an empty alternate is the same as none.
        ShortcutStorage::write(group, QStringLiteral("open_gallery"), {space, QKeySequence()}, {space});
        QVERIFY(!group.hasKey("open_gallery"));
        QVERIFY(!ShortcutStorage::read(group, QStringLiteral("open_gallery")).has_value());
        ShortcutStorage::write(group, QStringLiteral("go_up"), {}, {});
        QVERIFY(!group.hasKey("go_up"));

        QCOMPARE(ShortcutStorage::actionNames(group), QStringList{QStringLiteral("search_folder")});
    }

    void theVersionEntryIsNotAnAction()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QLatin1String(ShortcutStorage::Group));
        QVERIFY(!ShortcutStorage::isCurrent(group));
        ShortcutStorage::markCurrent(group);
        QVERIFY(ShortcutStorage::isCurrent(group));
        QCOMPARE(group.readEntry(ShortcutStorage::VersionKey, 0), ShortcutStorage::Version);

        // An action can't be named Version, so its value is never read as shortcuts or overwritten.
        QVERIFY(!ShortcutStorage::read(group, QLatin1String(ShortcutStorage::VersionKey)).has_value());
        ShortcutStorage::write(group, QLatin1String(ShortcutStorage::VersionKey), {f5}, {});
        QCOMPARE(group.readEntry(ShortcutStorage::VersionKey, 0), ShortcutStorage::Version);
        QVERIFY(ShortcutStorage::actionNames(group).isEmpty());
    }

    void shortcutsAreTakenOutOfTheUiRcFile()
    {
        const QString uiRc = QStringLiteral(
            "<!DOCTYPE gui SYSTEM \"kpartgui.dtd\">\n"
            "<gui name=\"opus-file-manager\" version=\"3\">\n"
            " <ToolBar name=\"mainToolBar\" noMerge=\"1\"><Action name=\"go_up\"/></ToolBar>\n"
            " <ActionProperties scheme=\"Default\">\n"
            "  <Action name=\"open_gallery\" shortcut=\"F5; Ctrl+K, G\"/>\n"
            "  <Action name=\"search_folder\" shortcut=\"\"/>\n"
            "  <Action name=\"go_up\" icon=\"go-top\" shortcut=\"Alt+Up\"/>\n"
            "  <Action name=\"bogus\" shortcut=\"Ctrl+Nonsense\"/>\n"
            "  <Action name=\"go_back\" iconText=\"Back\"/>\n"
            " </ActionProperties>\n"
            " <ActionProperties scheme=\"Other\">\n"
            "  <Action name=\"open_gallery\" shortcut=\"F6\"/>\n"
            " </ActionProperties>\n"
            "</gui>\n");
        const ShortcutStorage::XmlGuiShortcuts found = ShortcutStorage::takeXmlGuiShortcuts(uiRc);
        // KDE's editor writes an empty attribute for no shortcut; an unreadable one gives the defaults back.
        const QList<std::pair<QString, QList<QKeySequence>>> expected{
            {QStringLiteral("open_gallery"), {f5, ctrlKG}},
            {QStringLiteral("search_folder"), {}},
            {QStringLiteral("go_up"), {QKeySequence(Qt::ALT | Qt::Key_Up)}},
        };
        QVERIFY(found.shortcuts == expected);

        QDomDocument stripped;
        QVERIFY(stripped.setContent(found.stripped));
        QVERIFY2(!found.stripped.contains(QStringLiteral("Alt+Up")) && !found.stripped.contains(QStringLiteral("bogus")), qPrintable(found.stripped));
        const QDomElement properties = stripped.documentElement().firstChildElement(QStringLiteral("ActionProperties"));
        // Elements left with only a name are removed; other properties and the toolbars stay.
        QDomElement action = properties.firstChildElement();
        QCOMPARE(action.attribute(QStringLiteral("name")), QStringLiteral("go_up"));
        QCOMPARE(action.attribute(QStringLiteral("icon")), QStringLiteral("go-top"));
        QVERIFY(!action.hasAttribute(QStringLiteral("shortcut")));
        action = action.nextSiblingElement();
        QCOMPARE(action.attribute(QStringLiteral("name")), QStringLiteral("go_back"));
        QVERIFY(action.nextSiblingElement().isNull());
        QCOMPARE(stripped.documentElement().firstChildElement(QStringLiteral("ToolBar")).firstChildElement().attribute(QStringLiteral("name")),
                 QStringLiteral("go_up"));
        // Another shortcut scheme's properties are left as they are.
        const QDomElement other = properties.nextSiblingElement(QStringLiteral("ActionProperties"));
        QCOMPARE(other.attribute(QStringLiteral("scheme")), QStringLiteral("Other"));
        QCOMPARE(other.firstChildElement().attribute(QStringLiteral("shortcut")), QStringLiteral("F6"));

        // The other scheme's shortcuts are taken when it is the current one.
        const ShortcutStorage::XmlGuiShortcuts otherScheme = ShortcutStorage::takeXmlGuiShortcuts(uiRc, QStringLiteral("Other"));
        QCOMPARE(otherScheme.shortcuts.size(), 1);
        QCOMPARE(otherScheme.shortcuts.at(0).second, QList<QKeySequence>{QKeySequence(Qt::Key_F6)});
    }

    void uiRcFilesWithoutShortcutsAreLeftAlone()
    {
        const QString uiRc =
            QStringLiteral("<gui name=\"opus-file-manager\"><ActionProperties><Action name=\"go_up\" icon=\"go-top\"/></ActionProperties></gui>");
        const ShortcutStorage::XmlGuiShortcuts found = ShortcutStorage::takeXmlGuiShortcuts(uiRc);
        QVERIFY(found.shortcuts.isEmpty());
        QVERIFY(found.stripped.isEmpty());
        QVERIFY(ShortcutStorage::takeXmlGuiShortcuts(QString()).stripped.isEmpty());
        QVERIFY(ShortcutStorage::takeXmlGuiShortcuts(QStringLiteral("<gui")).shortcuts.isEmpty());

        // KXmlGui reads the tags without case, and properties without a scheme are the default scheme's.
        const ShortcutStorage::XmlGuiShortcuts lower =
            ShortcutStorage::takeXmlGuiShortcuts(QStringLiteral("<gui><actionproperties><action name=\"go_up\" shortcut=\"F2\"/></actionproperties></gui>"));
        QCOMPARE(lower.shortcuts.size(), 1);
        QCOMPARE(lower.shortcuts.at(0).first, QStringLiteral("go_up"));
        QVERIFY2(!lower.stripped.contains(QStringLiteral("F2")), qPrintable(lower.stripped));
    }

    void migrationReplacesTheEntriesOfTheSameActions()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QLatin1String(ShortcutStorage::Group));
        group.writeEntry("open_gallery", QStringLiteral("F9"));
        group.writeEntry("go_home", QStringLiteral("F10"));

        ShortcutStorage::XmlGuiShortcuts found;
        found.shortcuts = {{QStringLiteral("open_gallery"), {f5}}, {QStringLiteral("search_folder"), {}}};
        ShortcutStorage::migrate(group, found);
        QCOMPARE(group.readEntry("open_gallery", QString()), QStringLiteral("F5"));
        QCOMPARE(group.readEntry("search_folder", QString()), QStringLiteral("none"));
        QCOMPARE(group.readEntry("go_home", QString()), QStringLiteral("F10"));
        QVERIFY(ShortcutStorage::isCurrent(group));

        // Nothing to migrate still marks the format.
        KConfigGroup empty = config.group(QStringLiteral("Empty"));
        ShortcutStorage::migrate(empty, {});
        QVERIFY(ShortcutStorage::isCurrent(empty));
        QVERIFY(ShortcutStorage::actionNames(empty).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ShortcutStorageTest)

#include "shortcutstoragetest.moc"
