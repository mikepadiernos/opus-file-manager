// SPDX-License-Identifier: GPL-3.0-or-later

#include "keymapfile.h"

#include "settings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <algorithm>

using KeyBindings::Binding;
using KeyBindings::ConflictKind;

namespace
{
QKeySequence key(const QString &text)
{
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}

Binding binding(const QString &name, const QList<QKeySequence> &shortcuts, const QList<QKeySequence> &defaults)
{
    return {name, name, shortcuts, defaults};
}

QJsonObject keymapObject(const QJsonObject &actions = {})
{
    return {{QStringLiteral("format"), QStringLiteral("plasma-opus-keymap")},
            {QStringLiteral("version"), 1},
            {QStringLiteral("application"), QStringLiteral("opus-file-manager")},
            {QStringLiteral("actions"), actions}};
}

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
}

class KeymapFileTest : public QObject
{
    Q_OBJECT

private:
    const QKeySequence f5{Qt::Key_F5};
    const QKeySequence f9{Qt::Key_F9};
    const QKeySequence space{Qt::Key_Space};
    const QKeySequence ctrlKG{QKeyCombination(Qt::CTRL | Qt::Key_K), QKeyCombination(Qt::Key_G)};

    /// What fromJson() says about @p document, or empty when it reads it.
    QString errorOf(const QByteArray &document)
    {
        QString error;
        const auto read = KeymapFile::fromJson(document, Settings::self(), &error);
        if (read) {
            return {};
        }
        if (error.isEmpty()) {
            return QStringLiteral("rejected without a reason");
        }
        return error;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::self()->save();
    }

    void sequencesAreReadStrictly()
    {
        QCOMPARE(KeymapFile::parseSequence(QStringLiteral("F5")), std::optional(f5));
        QCOMPARE(KeymapFile::parseSequence(QStringLiteral("Ctrl+K, G")), std::optional(ctrlKG));
        QCOMPARE(KeymapFile::parseSequence(QStringLiteral(" Meta+Shift+Del ")), std::optional(QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Delete)));
        // The comma key, alone and in a sequence.
        QCOMPARE(KeymapFile::parseSequence(QStringLiteral("Ctrl+,")), std::optional(QKeySequence(Qt::CTRL | Qt::Key_Comma)));
        QCOMPARE(KeymapFile::parseSequence(QStringLiteral("Ctrl+,, G")),
                 std::optional(QKeySequence(QKeyCombination(Qt::CTRL | Qt::Key_Comma), QKeyCombination(Qt::Key_G))));
        QCOMPARE(KeymapFile::parseSequence(QStringLiteral("A, B, C, D"))->count(), 4);

        for (const QString &invalid : {QString(),
                                       QStringLiteral("   "),
                                       QStringLiteral("Ctrl+Nonsense"),
                                       QStringLiteral("Ctrl+"),
                                       QStringLiteral("Ctrl"),
                                       QStringLiteral("Shift"),
                                       QStringLiteral("F5; F6"),
                                       QStringLiteral("F5, "),
                                       QStringLiteral("F5\nF6"),
                                       // Qt would silently drop the fifth key.
                                       QStringLiteral("A, B, C, D, E"),
                                       QString(300, QLatin1Char('A'))}) {
            QVERIFY2(!KeymapFile::parseSequence(invalid).has_value(), qPrintable(invalid));
        }
    }

    void exportListsOnlyCustomizedWindowActions()
    {
        Binding global = binding(QStringLiteral("toggle_window_visibility"), {key(QStringLiteral("Meta+Alt+O"))}, {});
        global.global = true;
        Binding gallery = binding(QStringLiteral("gallery_next"), {key(QStringLiteral("Right"))}, {});
        gallery.context = KeyBindings::Context::Viewer;
        const QList<Binding> bindings{
            binding(QStringLiteral("open_gallery"), {f9, ctrlKG}, {space}),
            binding(QStringLiteral("go_up"), {key(QStringLiteral("Alt+Up"))}, {key(QStringLiteral("Alt+Up"))}),
            binding(QStringLiteral("search_folder"), {}, {key(QStringLiteral("Ctrl+Alt+F"))}),
            // KXmlGui names actions without a name of their own differently each run.
            binding(QStringLiteral("unnamed-QAction-1"), {f5}, {}),
            binding(QString(), {f5}, {}),
            global,
            gallery,
        };
        const KeymapFile::Keymap keymap = KeymapFile::fromBindings(bindings);
        QCOMPARE(keymap.actions.size(), 2);
        QCOMPARE(keymap.actions.at(0).first, QStringLiteral("open_gallery"));
        QCOMPARE(keymap.actions.at(0).second, (QList<QKeySequence>{f9, ctrlKG}));
        QCOMPARE(keymap.actions.at(1).first, QStringLiteral("search_folder"));
        QVERIFY(keymap.actions.at(1).second.isEmpty());
    }

    void keymapsRoundTripWithTheKeyboardSettings()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        Settings::setExtraNavigationShortcuts(true);
        Settings::setDoubleShiftSearch(false);
        Settings::setVimNavigation(true);
        KeymapFile::Keymap keymap;
        keymap.actions = {{QStringLiteral("open_gallery"), {f9, ctrlKG}}, {QStringLiteral("search_folder"), {}}};
        keymap.settings = KeymapFile::settingValues(Settings::self());
        QCOMPARE(keymap.settings.value(QStringLiteral("YaziKeyPreset")), QVariant(1));
        QCOMPARE(keymap.settings.value(QStringLiteral("VimNavigation")), QVariant(true));
        QCOMPARE(keymap.settings.value(QStringLiteral("ExtraNavigationShortcuts")), QVariant(true));
        QCOMPARE(keymap.settings.value(QStringLiteral("DoubleShiftSearch")), QVariant(false));
        QCOMPARE(keymap.settings.value(QStringLiteral("TabSwitchesPanes")), QVariant(true));

        const QDateTime exported(QDate(2026, 11, 28), QTime(12, 0), QTimeZone::UTC);
        const QByteArray text = KeymapFile::toJson(keymap, Settings::self(), exported);
        const QJsonObject object = QJsonDocument::fromJson(text).object();
        QCOMPARE(object.value(QStringLiteral("format")).toString(), QStringLiteral("plasma-opus-keymap"));
        QCOMPARE(object.value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(object.value(QStringLiteral("application")).toString(), QStringLiteral("opus-file-manager"));
        QCOMPARE(object.value(QStringLiteral("exported")).toString(), QStringLiteral("2026-11-28T12:00:00Z"));
        // Shortcuts in Qt's portable text, primary first; enum settings by choice name.
        const QJsonObject actions = object.value(QStringLiteral("actions")).toObject();
        QCOMPARE(actions.value(QStringLiteral("open_gallery")).toArray(), (QJsonArray{QStringLiteral("F9"), QStringLiteral("Ctrl+K, G")}));
        QCOMPARE(actions.value(QStringLiteral("search_folder")).toArray(), QJsonArray());
        const QJsonObject settings = object.value(QStringLiteral("settings")).toObject();
        QCOMPARE(settings.value(QStringLiteral("YaziKeyPreset")).toString(), QStringLiteral("Vim"));
        QCOMPARE(settings.value(QStringLiteral("ExtraNavigationShortcuts")).toBool(false), true);
        QCOMPARE(settings.value(QStringLiteral("VimNavigation")).toBool(false), true);

        QString error;
        QStringList warnings;
        const auto read = KeymapFile::fromJson(text, Settings::self(), &error, &warnings);
        QVERIFY2(read, qPrintable(error));
        QVERIFY(warnings.isEmpty());
        QCOMPARE(read->settings, keymap.settings);
        QCOMPARE(read->actions.size(), 2);
        QVERIFY(read->actions.contains(keymap.actions.at(0)));
        QVERIFY(read->actions.contains(keymap.actions.at(1)));

        // Without settings to compare with, they are skipped and said to be.
        const auto withoutSettings = KeymapFile::fromJson(text, nullptr, &error, &warnings);
        QVERIFY(withoutSettings);
        QVERIFY(withoutSettings->settings.isEmpty());
        QCOMPARE(warnings.size(), 5);
    }

    void filesThatArentValidKeymapsAreRejected()
    {
        QVERIFY(errorOf(json(keymapObject())).isEmpty());
        QVERIFY(!errorOf(QByteArray(KeymapFile::MaximumFileSize + 1, ' ')).isEmpty());
        QVERIFY(!errorOf(QByteArray("{not json")).isEmpty());
        QVERIFY(!errorOf(QByteArray("[]")).isEmpty());

        const auto with = [](const QString &member, const QJsonValue &value) {
            QJsonObject object = keymapObject();
            if (value.isUndefined()) {
                object.remove(member);
            } else {
                object.insert(member, value);
            }
            return json(object);
        };
        QVERIFY(!errorOf(with(QStringLiteral("format"), QStringLiteral("plasma-opus-theme-profile"))).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("version"), QJsonValue::Undefined)).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("version"), QStringLiteral("1"))).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("version"), 0)).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("version"), 1.5)).isEmpty());
        QVERIFY(errorOf(with(QStringLiteral("version"), 2)).contains(QStringLiteral("newer version")));
        QVERIFY(!errorOf(with(QStringLiteral("application"), QJsonValue::Undefined)).isEmpty());
        QVERIFY(errorOf(with(QStringLiteral("application"), QStringLiteral("plasma-impresario"))).contains(QStringLiteral("plasma-impresario")));
        QVERIFY(!errorOf(with(QStringLiteral("exported"), 42)).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("actions"), QJsonValue::Undefined)).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("actions"), QJsonArray())).isEmpty());
        QVERIFY(!errorOf(with(QStringLiteral("settings"), QJsonArray())).isEmpty());

        const auto action = [](const QString &name, const QJsonValue &shortcuts) {
            return json(keymapObject({{name, shortcuts}}));
        };
        QVERIFY(errorOf(action(QStringLiteral("open_gallery"), QJsonArray{QStringLiteral("F9")})).isEmpty());
        QVERIFY(!errorOf(action(QStringLiteral("open_gallery"), QStringLiteral("F9"))).isEmpty());
        QVERIFY(!errorOf(action(QStringLiteral("open_gallery"), QJsonArray{9})).isEmpty());
        QVERIFY(!errorOf(action(QStringLiteral("open_gallery"), QJsonArray{QJsonObject()})).isEmpty());
        QVERIFY(errorOf(action(QStringLiteral("open_gallery"), QJsonArray{QStringLiteral("Ctrl+Nonsense")})).contains(QStringLiteral("Ctrl+Nonsense")));
        QVERIFY(!errorOf(action(QStringLiteral("open_gallery"), QJsonArray{QString()})).isEmpty());
        QVERIFY(!errorOf(action(QStringLiteral("open\ngallery"), QJsonArray())).isEmpty());
        QVERIFY(!errorOf(action(QStringLiteral(" open_gallery"), QJsonArray())).isEmpty());
        QVERIFY(!errorOf(action(QString(300, QLatin1Char('a')), QJsonArray())).isEmpty());
        QJsonArray tooMany;
        for (int i = 1; i <= KeymapFile::MaximumShortcuts + 1; ++i) {
            tooMany.append(QStringLiteral("Ctrl+Alt+%1").arg(QChar(QLatin1Char('A' + i))));
        }
        QVERIFY(!errorOf(action(QStringLiteral("open_gallery"), tooMany)).isEmpty());
        QJsonObject tooManyActions;
        for (int i = 0; i <= KeymapFile::MaximumActions; ++i) {
            tooManyActions.insert(QStringLiteral("a%1").arg(i), QJsonArray());
        }
        QVERIFY(!errorOf(json(keymapObject(tooManyActions))).isEmpty());

        // A known setting of the wrong type or with an unknown choice is an error.
        const auto setting = [](const QString &name, const QJsonValue &value) {
            QJsonObject object = keymapObject();
            object.insert(QStringLiteral("settings"), QJsonObject{{name, value}});
            return json(object);
        };
        QVERIFY(errorOf(setting(QStringLiteral("YaziKeyPreset"), QStringLiteral("Standard"))).isEmpty());
        QVERIFY(!errorOf(setting(QStringLiteral("YaziKeyPreset"), QStringLiteral("Emacs"))).isEmpty());
        QVERIFY(!errorOf(setting(QStringLiteral("YaziKeyPreset"), 1)).isEmpty());
        QVERIFY(!errorOf(setting(QStringLiteral("ExtraNavigationShortcuts"), QStringLiteral("true"))).isEmpty());
    }

    void unknownEntriesAreSkippedWithAReport()
    {
        QJsonObject object = keymapObject({{QStringLiteral("open_gallery"), QJsonArray{QStringLiteral("F9"), QStringLiteral("F9")}}});
        object.insert(QStringLiteral("comment"), QStringLiteral("from my laptop"));
        // Settings that aren't keyboard settings are never taken from a keymap.
        object.insert(
            QStringLiteral("settings"),
            QJsonObject{{QStringLiteral("UseCustomStyleSheet"), true}, {QStringLiteral("SomethingNew"), 1}, {QStringLiteral("TabSwitchesPanes"), false}});
        QString error;
        QStringList warnings;
        const auto read = KeymapFile::fromJson(json(object), Settings::self(), &error, &warnings);
        QVERIFY2(read, qPrintable(error));
        QCOMPARE(read->actions, (QList<std::pair<QString, QList<QKeySequence>>>{{QStringLiteral("open_gallery"), {f9}}}));
        QCOMPARE(read->settings, (QVariantMap{{QStringLiteral("TabSwitchesPanes"), false}}));
        const QString report = warnings.join(QLatin1Char('\n'));
        QCOMPARE(warnings.size(), 4);
        QVERIFY2(report.contains(QStringLiteral("“comment”")), qPrintable(report));
        QVERIFY2(report.contains(QStringLiteral("“UseCustomStyleSheet”")), qPrintable(report));
        QVERIFY2(report.contains(QStringLiteral("“SomethingNew”")), qPrintable(report));
        QVERIFY2(report.contains(QStringLiteral("more than once")), qPrintable(report));
    }

    void filesAreWrittenAndReadWithASizeLimit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("keymap.json"));
        KeymapFile::Keymap keymap;
        keymap.actions = {{QStringLiteral("open_gallery"), {f9}}};
        QCOMPARE(KeymapFile::exportFile(keymap, Settings::self(), path), QString());
        QString error;
        QCOMPARE(KeymapFile::importFile(path, Settings::self(), &error), std::optional(keymap));

        QVERIFY(!KeymapFile::importFile(dir.filePath(QStringLiteral("missing.json")), Settings::self(), &error));
        QVERIFY(!error.isEmpty());
        QFile large(dir.filePath(QStringLiteral("large.json")));
        QVERIFY(large.open(QIODevice::WriteOnly));
        large.write(QByteArray(KeymapFile::MaximumFileSize + 1, ' '));
        large.close();
        error.clear();
        QVERIFY(!KeymapFile::importFile(large.fileName(), Settings::self(), &error));
        QVERIFY2(error.contains(QStringLiteral("larger than")), qPrintable(error));
        QVERIFY(!KeymapFile::exportFile(keymap, Settings::self(), dir.filePath(QStringLiteral("missing/keymap.json"))).isEmpty());
    }

    void previewListsChangesUnknownActionsAndNewConflicts()
    {
        Binding gallery = binding(QStringLiteral("gallery_next"), {key(QStringLiteral("Right"))}, {key(QStringLiteral("Right"))});
        gallery.context = KeyBindings::Context::Viewer;
        const QList<Binding> bindings{
            binding(QStringLiteral("open_gallery"), {space}, {space}),
            binding(QStringLiteral("copy_to_other_pane"), {f5}, {f5}),
            binding(QStringLiteral("go_up"), {key(QStringLiteral("Alt+Up"))}, {key(QStringLiteral("Alt+Up"))}),
            binding(QStringLiteral("search_folder"), {key(QStringLiteral("Ctrl+K"))}, {key(QStringLiteral("Ctrl+Alt+F"))}),
            binding(QStringLiteral("rename"), {key(QStringLiteral("Ctrl+K, R"))}, {key(QStringLiteral("F2"))}),
            gallery,
        };
        // Search Current Folder begins Rename's sequence.
        QCOMPARE(KeyBindings::conflicts(bindings).size(), 1);

        KeymapFile::Keymap keymap;
        keymap.actions = {
            // Takes Copy to Other Pane's key.
            {QStringLiteral("open_gallery"), {f5}},
            {QStringLiteral("go_up"), {key(QStringLiteral("Alt+Up"))}},
            // Gives Tab, which text fields keep and which moves the focus.
            {QStringLiteral("copy_to_other_pane"), {f5, key(QStringLiteral("Tab"))}},
            {QStringLiteral("custom_command_preset_0123abcd"), {f9}},
            {QStringLiteral("gallery_next"), {f9}},
        };
        const KeymapFile::Preview listed = KeymapFile::preview(bindings, keymap, false);
        QCOMPARE(listed.changes, (QList<KeymapFile::Change>{{0, {f5}}, {1, {f5, key(QStringLiteral("Tab"))}}}));
        QCOMPARE(listed.unchanged, 1);
        QCOMPARE(listed.unknown, (QStringList{QStringLiteral("custom_command_preset_0123abcd"), QStringLiteral("gallery_next")}));
        QVERIFY(KeymapFile::unknownText(listed.unknown.at(0)).startsWith(QStringLiteral("A saved command")));
        QCOMPARE(listed.bindings.at(0).shortcuts, QList<QKeySequence>{f5});
        // F5 for both, and Tab where text fields and the focus keep it.
        const auto has = [&listed](ConflictKind kind, KeyBindings::Context context) {
            return std::any_of(listed.added.cbegin(), listed.added.cend(), [&](const KeyBindings::Conflict &conflict) {
                return conflict.kind == kind && (kind != ConflictKind::Reserved || conflict.context == context);
            });
        };
        QVERIFY(has(ConflictKind::Same, KeyBindings::Context::Application));
        QVERIFY(has(ConflictKind::Reserved, KeyBindings::Context::TextInput));
        QVERIFY(has(ConflictKind::Reserved, KeyBindings::Context::Application));
        QCOMPARE(listed.kept.size(), 1);
        QCOMPARE(listed.resolved, 0);

        // Resetting what the file doesn't list resolves the sequence conflict.
        const KeymapFile::Preview reset = KeymapFile::preview(bindings, keymap, true);
        QCOMPARE(reset.changes.size(), 4);
        QCOMPARE(reset.changes.at(2), (KeymapFile::Change{3, {key(QStringLiteral("Ctrl+Alt+F"))}}));
        QCOMPARE(reset.changes.at(3), (KeymapFile::Change{4, {key(QStringLiteral("F2"))}}));
        QVERIFY(reset.kept.isEmpty());
        QCOMPARE(reset.resolved, 1);

        // Nothing to change.
        const KeymapFile::Preview none = KeymapFile::preview(bindings, {}, false);
        QVERIFY(none.changes.isEmpty());
        QVERIFY(none.added.isEmpty());
        QCOMPARE(none.bindings, bindings);
    }

    void extraNavigationKeysGiveWayToImportedShortcuts()
    {
        const QKeySequence extra = key(QStringLiteral("Ctrl+Shift+A"));
        // Deselect All has its extra key as a default, as while the extra keys are on.
        const QList<Binding> bindings{binding(QStringLiteral("deselect_all"), {extra}, {extra}), binding(QStringLiteral("open_gallery"), {space}, {space})};
        KeymapFile::Keymap keymap;
        keymap.actions = {{QStringLiteral("open_gallery"), {extra}}};
        const KeymapFile::Preview preview = KeymapFile::preview(bindings, keymap, false);
        QCOMPARE(preview.changes, (QList<KeymapFile::Change>{{1, {extra}}}));
        QVERIFY(preview.bindings.at(0).shortcuts.isEmpty());
        QVERIFY(preview.bindings.at(0).defaults.isEmpty());
        QVERIFY(preview.added.isEmpty());

        // An extra key the user chose for the action stays, and conflicts.
        const QList<Binding> chosen{binding(QStringLiteral("deselect_all"), {extra, f9}, {extra}), binding(QStringLiteral("open_gallery"), {space}, {space})};
        const KeymapFile::Preview kept = KeymapFile::preview(chosen, keymap, false);
        QCOMPARE(kept.bindings.at(0).shortcuts, (QList<QKeySequence>{extra, f9}));
        QCOMPARE(kept.added.size(), 1);
        QCOMPARE(kept.added.at(0).kind, ConflictKind::Same);
    }
};

QTEST_MAIN(KeymapFileTest)

#include "keymapfiletest.moc"
