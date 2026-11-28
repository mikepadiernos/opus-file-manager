// SPDX-License-Identifier: GPL-3.0-or-later

#include "keymapfile.h"

#include "appidentity.h"
#include "navigationshortcuts.h"

#include <KCoreConfigSkeleton>
#include <KLocalizedString>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace KeymapFile
{
namespace
{
const QString FormatKey = QStringLiteral("format");
const QString VersionKey = QStringLiteral("version");
const QString ApplicationKey = QStringLiteral("application");
const QString ExportedKey = QStringLiteral("exported");
const QString ActionsKey = QStringLiteral("actions");
const QString SettingsKey = QStringLiteral("settings");

/// The prefix of the names KXmlGui gives actions without one, which change from run to run.
const QString UnnamedPrefix = QStringLiteral("unnamed-");
/// The prefix of saved commands' action names, followed by a hash of the command's name.
const QString CommandPrefix = QStringLiteral("custom_command_preset_");

bool hasControlCharacter(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar ch) {
        return ch.category() == QChar::Other_Control || ch == QChar::LineSeparator || ch == QChar::ParagraphSeparator;
    });
}

bool isModifier(Qt::Key key)
{
    return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Meta || key == Qt::Key_Alt || key == Qt::Key_AltGr || key == Qt::Key_Super_L
        || key == Qt::Key_Super_R || key == Qt::Key_Hyper_L || key == Qt::Key_Hyper_R;
}

/// The key combinations of @p text, separated as QKeySequence separates them: a comma, optionally followed by a space, except in "Ctrl+,".
QStringList splitCombinations(const QString &text)
{
    QStringList parts;
    QString rest = text;
    while (!rest.isEmpty()) {
        qsizetype end = rest.indexOf(QLatin1Char(','));
        qsizetype skip = 1;
        if (end == rest.size() - 1) {
            // A trailing comma is the comma key itself, as in "Ctrl+,".
            end = -1;
        } else if (end >= 0) {
            if (rest.at(end + 1) == QLatin1Char(',')) {
                // "Ctrl+,, G": the first comma is the key.
                ++end;
            }
            if (end + 1 < rest.size() && rest.at(end + 1) == QLatin1Char(' ')) {
                skip = 2;
            }
        }
        if (end < 0) {
            parts.append(rest);
            break;
        }
        parts.append(rest.left(end));
        rest = rest.mid(end + skip);
        if (rest.isEmpty()) {
            // A separator with nothing after it.
            parts.append(QString());
        }
    }
    return parts;
}

const KConfigSkeletonItem *settingItem(const KCoreConfigSkeleton *settings, const QString &name)
{
    if (!settings || !settingNames().contains(name)) {
        return nullptr;
    }
    const KConfigSkeletonItem *item = settings->findItem(name);
    if (!item) {
        return nullptr;
    }
    const int type = item->property().typeId();
    if (dynamic_cast<const KCoreConfigSkeleton::ItemEnum *>(item) || type == QMetaType::Bool || type == QMetaType::Int) {
        return item;
    }
    return nullptr;
}

QJsonValue encodeSetting(const KConfigSkeletonItem *item, const QVariant &value)
{
    if (const auto *enumItem = dynamic_cast<const KCoreConfigSkeleton::ItemEnum *>(item)) {
        const auto choices = enumItem->choices();
        const int index = value.toInt();
        return index >= 0 && index < choices.size() ? QJsonValue(choices.at(index).name) : QJsonValue();
    }
    if (item->property().typeId() == QMetaType::Int) {
        return value.toInt();
    }
    return value.toBool();
}

std::optional<QVariant> decodeSetting(const KConfigSkeletonItem *item, const QJsonValue &json)
{
    if (const auto *enumItem = dynamic_cast<const KCoreConfigSkeleton::ItemEnum *>(item)) {
        const auto choices = enumItem->choices();
        for (int i = 0; json.isString() && i < choices.size(); ++i) {
            if (choices.at(i).name == json.toString()) {
                return QVariant(i);
            }
        }
        return std::nullopt;
    }
    if (item->property().typeId() == QMetaType::Int) {
        // A whole number within the entry's range, as Preferences would let it be set.
        if (!json.isDouble()) {
            return std::nullopt;
        }
        const double number = json.toDouble();
        const QVariant minimum = item->minValue();
        const QVariant maximum = item->maxValue();
        if (!std::isfinite(number) || std::trunc(number) != number || number < (minimum.isValid() ? minimum.toInt() : std::numeric_limits<int>::min())
            || number > (maximum.isValid() ? maximum.toInt() : std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
        return QVariant(int(number));
    }
    if (json.isBool()) {
        return QVariant(json.toBool());
    }
    return std::nullopt;
}

QJsonArray shortcutsJson(const QList<QKeySequence> &shortcuts)
{
    QJsonArray array;
    for (const QKeySequence &shortcut : KeyBindings::keys(shortcuts)) {
        array.append(shortcut.toString(QKeySequence::PortableText));
    }
    return array;
}
}

QString application()
{
    return AppIdentity::componentName();
}

QStringList settingNames()
{
    return {QStringLiteral("ExtraNavigationShortcuts"),
            QStringLiteral("YaziKeyPreset"),
            QStringLiteral("VimNavigation"),
            QStringLiteral("DoubleShiftSearch"),
            QStringLiteral("TabSwitchesPanes"),
            QStringLiteral("ShortcutHints"),
            QStringLiteral("ShortcutHintDelay"),
            QStringLiteral("KeySequenceTimeout"),
            QStringLiteral("KeyboardHintKeys"),
            QStringLiteral("KeyboardHintItemAction")};
}

QString settingText(const QString &name)
{
    if (name == QLatin1String("ExtraNavigationShortcuts")) {
        return i18nc("@item keyboard setting", "Extra navigation shortcuts");
    }
    if (name == QLatin1String("YaziKeyPreset")) {
        return i18nc("@item keyboard setting", "Yazi mode key preset");
    }
    if (name == QLatin1String("VimNavigation")) {
        return i18nc("@item keyboard setting", "Vim-style navigation in file views");
    }
    if (name == QLatin1String("DoubleShiftSearch")) {
        return i18nc("@item keyboard setting", "Pressing Shift twice opens search");
    }
    if (name == QLatin1String("TabSwitchesPanes")) {
        return i18nc("@item keyboard setting", "Tab key switches between the panes");
    }
    if (name == QLatin1String("ShortcutHints")) {
        return i18nc("@item keyboard setting", "Shortcut hints for pending key sequences");
    }
    if (name == QLatin1String("ShortcutHintDelay")) {
        return i18nc("@item keyboard setting", "Shortcut hint delay");
    }
    if (name == QLatin1String("KeySequenceTimeout")) {
        return i18nc("@item keyboard setting", "Key sequence timeout");
    }
    if (name == QLatin1String("KeyboardHintKeys")) {
        return i18nc("@item keyboard setting", "Keyboard hint label keys");
    }
    if (name == QLatin1String("KeyboardHintItemAction")) {
        return i18nc("@item keyboard setting", "Keyboard hint action for files and folders");
    }
    return name;
}

std::optional<QKeySequence> parseSequence(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > MaximumTextLength || hasControlCharacter(trimmed)) {
        return std::nullopt;
    }
    const QStringList parts = splitCombinations(trimmed);
    if (parts.isEmpty() || parts.size() > 4) {
        return std::nullopt;
    }
    QList<QKeyCombination> combinations;
    for (const QString &part : parts) {
        const QKeySequence single = QKeySequence::fromString(part.trimmed(), QKeySequence::PortableText);
        if (single.count() != 1 || single[0].toCombined() == 0 || single[0].key() == Qt::Key_unknown || isModifier(single[0].key())) {
            return std::nullopt;
        }
        combinations.append(single[0]);
    }
    while (combinations.size() < 4) {
        combinations.append(QKeyCombination::fromCombined(0));
    }
    const QKeySequence sequence(combinations.at(0), combinations.at(1), combinations.at(2), combinations.at(3));
    // Qt reads the text the same way; anything else is a misreading of ours.
    if (QKeySequence::fromString(trimmed, QKeySequence::PortableText) != sequence) {
        return std::nullopt;
    }
    return sequence;
}

bool isExportable(const KeyBindings::Binding &binding)
{
    return !binding.global && binding.context == KeyBindings::Context::Application && !binding.name.isEmpty() && !binding.name.startsWith(UnnamedPrefix)
        && binding.name != QLatin1String("Version");
}

Keymap fromBindings(const QList<KeyBindings::Binding> &bindings)
{
    Keymap keymap;
    QSet<QString> listed;
    for (const KeyBindings::Binding &binding : bindings) {
        if (isExportable(binding) && KeyBindings::isCustomized(binding) && !listed.contains(binding.name)) {
            listed.insert(binding.name);
            keymap.actions.append({binding.name, KeyBindings::keys(binding.shortcuts)});
        }
    }
    return keymap;
}

QVariantMap settingValues(const KCoreConfigSkeleton *settings)
{
    QVariantMap values;
    for (const QString &name : settingNames()) {
        if (const KConfigSkeletonItem *item = settingItem(settings, name)) {
            values.insert(name, item->property());
        }
    }
    return values;
}

QByteArray toJson(const Keymap &keymap, const KCoreConfigSkeleton *settings, const QDateTime &exported)
{
    QJsonObject document;
    document.insert(FormatKey, QLatin1String(Format));
    document.insert(VersionKey, Version);
    document.insert(ApplicationKey, application());
    if (exported.isValid()) {
        document.insert(ExportedKey, exported.toUTC().toString(Qt::ISODate));
    }
    QJsonObject actions;
    for (const auto &[name, shortcuts] : keymap.actions) {
        actions.insert(name, shortcutsJson(shortcuts));
    }
    document.insert(ActionsKey, actions);
    QJsonObject values;
    for (auto it = keymap.settings.constBegin(); it != keymap.settings.constEnd(); ++it) {
        if (const KConfigSkeletonItem *item = settingItem(settings, it.key())) {
            if (const QJsonValue encoded = encodeSetting(item, it.value()); !encoded.isNull()) {
                values.insert(it.key(), encoded);
            }
        }
    }
    if (!values.isEmpty()) {
        document.insert(SettingsKey, values);
    }
    return QJsonDocument(document).toJson(QJsonDocument::Indented);
}

std::optional<Keymap> fromJson(const QByteArray &json, const KCoreConfigSkeleton *settings, QString *error, QStringList *warnings)
{
    QString reason;
    const auto reject = [&reason, error] {
        if (error) {
            *error = reason;
        }
        return std::nullopt;
    };
    QStringList notes;
    if (json.size() > MaximumFileSize) {
        reason = i18nc("@info %1 file size", "The keymap is larger than %1 and was not read.", QLocale().formattedDataSize(MaximumFileSize));
        return reject();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || object.value(FormatKey).toString() != QLatin1String(Format)) {
        reason = i18nc("@info", "The file is not a Plasma Opus keymap.");
        return reject();
    }
    const QJsonValue version = object.value(VersionKey);
    if (!version.isDouble() || version.toDouble() != std::trunc(version.toDouble()) || version.toDouble() < 1) {
        reason = i18nc("@info", "The keymap has no valid format version.");
        return reject();
    }
    if (version.toDouble() > Version) {
        reason = i18nc("@info %1 version number",
                       "The keymap uses format version %1, which needs a newer version of Plasma Opus.",
                       QString::number(version.toDouble(), 'f', 0));
        return reject();
    }
    const QJsonValue app = object.value(ApplicationKey);
    if (!app.isString()) {
        reason = i18nc("@info", "The keymap doesn't say which application it is for.");
        return reject();
    }
    if (app.toString() != application()) {
        const QString name = app.toString().left(MaximumTextLength);
        reason = i18nc("@info %1 application name", "The keymap is for another application, “%1”.", hasControlCharacter(name) ? QString() : name);
        return reject();
    }
    if (object.contains(ExportedKey) && !object.value(ExportedKey).isString()) {
        reason = i18nc("@info", "The keymap's export date is not text.");
        return reject();
    }
    const QJsonValue actionsValue = object.value(ActionsKey);
    if (!actionsValue.isObject()) {
        reason = i18nc("@info", "The keymap has no list of actions.");
        return reject();
    }
    const QJsonObject actions = actionsValue.toObject();
    if (actions.size() > MaximumActions) {
        reason = i18ncp("@info", "The keymap lists more than %1 action.", "The keymap lists more than %1 actions.", MaximumActions);
        return reject();
    }

    Keymap keymap;
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const QString name = it.key();
        if (name.isEmpty() || name.size() > MaximumTextLength || hasControlCharacter(name) || name != name.trimmed()) {
            reason = i18nc("@info", "The keymap has an action whose name is not valid.");
            return reject();
        }
        if (!it.value().isArray()) {
            reason = i18nc("@info %1 action name", "The shortcuts of “%1” are not a list.", name);
            return reject();
        }
        const QJsonArray array = it.value().toArray();
        if (array.size() > MaximumShortcuts) {
            reason = i18ncp("@info %2 action name", "“%2” has more than %1 shortcut.", "“%2” has more than %1 shortcuts.", MaximumShortcuts, name);
            return reject();
        }
        QList<QKeySequence> shortcuts;
        bool repeated = false;
        for (const QJsonValue &value : array) {
            const std::optional<QKeySequence> sequence = value.isString() ? parseSequence(value.toString()) : std::nullopt;
            if (!sequence) {
                const QString text = value.isString() && !hasControlCharacter(value.toString()) ? value.toString().left(MaximumTextLength) : QString();
                reason = text.isEmpty() ? i18nc("@info %1 action name", "A shortcut of “%1” is not a valid key sequence.", name)
                                        : i18nc("@info %1 action name, %2 shortcut", "The shortcut “%2” of “%1” is not a valid key sequence.", name, text);
                return reject();
            }
            if (shortcuts.contains(*sequence)) {
                repeated = true;
                continue;
            }
            shortcuts.append(*sequence);
        }
        if (repeated) {
            notes.append(i18nc("@info %1 action name", "“%1” lists a shortcut more than once; it is used once.", name));
        }
        keymap.actions.append({name, shortcuts});
    }

    if (object.contains(SettingsKey)) {
        const QJsonValue settingsValue = object.value(SettingsKey);
        if (!settingsValue.isObject()) {
            reason = i18nc("@info", "The keymap's settings are not a list of settings.");
            return reject();
        }
        const QJsonObject values = settingsValue.toObject();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            const KConfigSkeletonItem *item = settingItem(settings, it.key());
            if (!item) {
                const QString name = hasControlCharacter(it.key()) ? QString() : it.key().left(MaximumTextLength);
                notes.append(i18nc("@info %1 setting name", "Skipped the unknown setting “%1”.", name));
                continue;
            }
            const std::optional<QVariant> value = decodeSetting(item, it.value());
            if (!value) {
                reason = i18nc("@info %1 setting name", "The keymap's setting %1 has an invalid value.", it.key());
                return reject();
            }
            keymap.settings.insert(it.key(), *value);
        }
    }

    static const QSet<QString> known{FormatKey, VersionKey, ApplicationKey, ExportedKey, ActionsKey, SettingsKey};
    for (const QString &key : object.keys()) {
        if (!known.contains(key)) {
            notes.append(
                i18nc("@info %1 member of a file", "Skipped the unknown entry “%1”.", hasControlCharacter(key) ? QString() : key.left(MaximumTextLength)));
        }
    }
    if (error) {
        error->clear();
    }
    if (warnings) {
        *warnings = notes;
    }
    return keymap;
}

std::optional<Keymap> importFile(const QString &path, const KCoreConfigSkeleton *settings, QString *error, QStringList *warnings)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = i18nc("@info %1 file, %2 reason", "Could not open %1: %2", path, file.errorString());
        }
        return std::nullopt;
    }
    if (file.size() > MaximumFileSize) {
        if (error) {
            *error = i18nc("@info %1 file size", "The keymap is larger than %1 and was not read.", QLocale().formattedDataSize(MaximumFileSize));
        }
        return std::nullopt;
    }
    return fromJson(file.read(MaximumFileSize + 1), settings, error, warnings);
}

QString exportFile(const Keymap &keymap, const KCoreConfigSkeleton *settings, const QString &path, const QDateTime &exported)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson(keymap, settings, exported)) < 0 || !file.commit()) {
        return i18nc("@info %1 file, %2 reason", "Could not write %1: %2", path, file.errorString());
    }
    return {};
}

Preview preview(const QList<KeyBindings::Binding> &bindings, const Keymap &keymap, bool resetUnlisted)
{
    Preview result;
    result.bindings = bindings;
    QList<bool> changed(bindings.size(), false);
    QSet<QString> listed;
    for (const auto &[name, shortcuts] : keymap.actions) {
        listed.insert(name);
        const auto found = std::find_if(bindings.cbegin(), bindings.cend(), [&name](const KeyBindings::Binding &binding) {
            return binding.name == name && isExportable(binding);
        });
        if (found == bindings.cend()) {
            if (!result.unknown.contains(name)) {
                result.unknown.append(name);
            }
            continue;
        }
        const int index = static_cast<int>(found - bindings.cbegin());
        if (changed.at(index)) {
            continue;
        }
        if (KeyBindings::keys(shortcuts) == KeyBindings::keys(found->shortcuts)) {
            ++result.unchanged;
            continue;
        }
        changed[index] = true;
        result.bindings[index].shortcuts = KeyBindings::keys(shortcuts);
    }
    if (resetUnlisted) {
        for (int i = 0; i < bindings.size(); ++i) {
            const KeyBindings::Binding &binding = bindings.at(i);
            if (!changed.at(i) && isExportable(binding) && !listed.contains(binding.name) && KeyBindings::isCustomized(binding)) {
                changed[i] = true;
                result.bindings[i].shortcuts = KeyBindings::keys(binding.defaults);
            }
        }
    }
    for (int i = 0; i < bindings.size(); ++i) {
        if (changed.at(i)) {
            result.changes.append({i, result.bindings.at(i).shortcuts});
        }
    }

    // The window leaves out an extra navigation key that another action takes; an action that has its defaults loses it too.
    for (int i = 0; i < result.bindings.size(); ++i) {
        KeyBindings::Binding &binding = result.bindings[i];
        const QKeySequence extra = NavigationShortcuts::extraKey(binding.name);
        if (extra.isEmpty() || binding.global || !binding.defaults.contains(extra) || NavigationShortcuts::user(result.bindings, i, extra) < 0) {
            continue;
        }
        const bool ownDefaults = !KeyBindings::isCustomized(binding);
        binding.defaults.removeAll(extra);
        if (ownDefaults) {
            binding.shortcuts = binding.defaults;
        }
    }

    const QList<KeyBindings::Conflict> before = KeyBindings::conflicts(bindings);
    const QList<KeyBindings::Conflict> after = KeyBindings::conflicts(result.bindings);
    for (const KeyBindings::Conflict &conflict : after) {
        (before.contains(conflict) ? result.kept : result.added).append(conflict);
    }
    result.resolved = static_cast<int>(std::count_if(before.cbegin(), before.cend(), [&after](const KeyBindings::Conflict &conflict) {
        return !after.contains(conflict);
    }));
    return result;
}

QString unknownText(const QString &name)
{
    if (name.startsWith(CommandPrefix)) {
        return i18nc("@item %1 action name", "A saved command that isn't saved here (%1)", name);
    }
    return i18nc("@item %1 action name", "%1: no action of this name", name);
}
}
