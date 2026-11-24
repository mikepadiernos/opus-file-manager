// SPDX-License-Identifier: GPL-3.0-or-later

#include "colorschemes.h"

#include <KColorScheme>
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace
{
/// Application property that KColorScheme and Plasma integration read.
constexpr auto SchemePathProperty = "KDE_COLOR_SCHEME_PATH";

struct ColorSet {
    const char *group;
    const char *backgroundNormal;
    const char *backgroundAlternate;
    const char *foregroundNormal;
    const char *foregroundInactive;
};

struct Preset {
    const char *id;
    const char *fileName;
    const char *schemeName;
    QList<ColorSet> sets;
    const char *link;
    const char *visited;
    const char *decorationHover;
    QList<std::pair<const char *, const char *>> windowManager;
};

// Breeze-derived palettes. Selections use a darker blue than Breeze so
// that white selected text keeps a contrast ratio above 4.5:1.
const QList<Preset> &presets()
{
    static const QList<Preset> presets{
        {ColorSchemes::LightPreset,
         "OpusLight.colors",
         "Plasma Opus Light",
         {{"Colors:View", "255,255,255", "247,247,247", "35,38,41", "112,125,138"},
          {"Colors:Window", "239,240,241", "227,229,231", "35,38,41", "112,125,138"},
          {"Colors:Button", "252,252,252", "163,212,250", "35,38,41", "112,125,138"},
          {"Colors:Selection", "38,113,165", "163,212,250", "255,255,255", "222,236,246"},
          {"Colors:Tooltip", "247,247,247", "239,240,241", "35,38,41", "112,125,138"},
          {"Colors:Complementary", "42,46,50", "27,30,32", "252,252,252", "161,169,177"},
          {"Colors:Header", "222,224,226", "239,240,241", "35,38,41", "112,125,138"}},
         "41,128,185",
         "155,89,182",
         "147,206,233",
         {{"activeBackground", "227,229,231"}, {"activeForeground", "35,38,41"}, {"inactiveBackground", "239,240,241"}, {"inactiveForeground", "112,125,138"}}},
        {ColorSchemes::DarkPreset,
         "OpusDark.colors",
         "Plasma Opus Dark",
         {{"Colors:View", "27,30,32", "35,38,41", "252,252,252", "161,169,177"},
          {"Colors:Window", "32,35,38", "41,44,48", "252,252,252", "161,169,177"},
          {"Colors:Button", "41,44,48", "30,87,116", "252,252,252", "161,169,177"},
          {"Colors:Selection", "38,113,165", "30,87,116", "252,252,252", "222,236,246"},
          {"Colors:Tooltip", "41,44,48", "32,35,38", "252,252,252", "161,169,177"},
          {"Colors:Complementary", "42,46,50", "27,30,32", "252,252,252", "161,169,177"},
          {"Colors:Header", "41,44,48", "32,35,38", "252,252,252", "161,169,177"}},
         "29,153,243",
         "155,89,182",
         "61,174,233",
         {{"activeBackground", "41,44,48"}, {"activeForeground", "252,252,252"}, {"inactiveBackground", "32,35,38"}, {"inactiveForeground", "161,169,177"}}},
    };
    return presets;
}

const Preset *presetFor(const QString &id)
{
    const auto &all = presets();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&id](const Preset &preset) {
        return id == QLatin1String(preset.id);
    });
    return found == all.cend() ? nullptr : &*found;
}

QString presetName(const Preset &preset)
{
    return QLatin1String(preset.id) == QLatin1String(ColorSchemes::DarkPreset) ? i18nc("@item:inlistbox colour scheme", "Plasma Opus Dark")
                                                                               : i18nc("@item:inlistbox colour scheme", "Plasma Opus Light");
}

/// Writes a preset's colour file, rewriting only entries that changed.
QString writePreset(const Preset &preset)
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/color-schemes");
    if (!QDir().mkpath(folder)) {
        return {};
    }
    const QString file = folder + QLatin1Char('/') + QLatin1String(preset.fileName);
    KConfig config(file, KConfig::SimpleConfig);
    for (const ColorSet &set : preset.sets) {
        KConfigGroup group = config.group(QLatin1String(set.group));
        group.writeEntry("BackgroundNormal", set.backgroundNormal);
        group.writeEntry("BackgroundAlternate", set.backgroundAlternate);
        group.writeEntry("ForegroundNormal", set.foregroundNormal);
        group.writeEntry("ForegroundInactive", set.foregroundInactive);
        group.writeEntry("ForegroundActive", "61,174,233");
        group.writeEntry("ForegroundLink", preset.link);
        group.writeEntry("ForegroundVisited", preset.visited);
        group.writeEntry("ForegroundNegative", "218,68,83");
        group.writeEntry("ForegroundNeutral", "246,116,0");
        group.writeEntry("ForegroundPositive", "39,174,96");
        group.writeEntry("DecorationFocus", "61,174,233");
        group.writeEntry("DecorationHover", preset.decorationHover);
    }
    KConfigGroup windowManager = config.group(QStringLiteral("WM"));
    for (const auto &[key, value] : preset.windowManager) {
        windowManager.writeEntry(key, value);
    }
    KConfigGroup disabled = config.group(QStringLiteral("ColorEffects:Disabled"));
    disabled.writeEntry("Color", "56,56,56");
    disabled.writeEntry("ColorAmount", 0);
    disabled.writeEntry("ColorEffect", 0);
    disabled.writeEntry("ContrastAmount", 0.65);
    disabled.writeEntry("ContrastEffect", 1);
    disabled.writeEntry("IntensityAmount", 0.1);
    disabled.writeEntry("IntensityEffect", 2);
    KConfigGroup inactive = config.group(QStringLiteral("ColorEffects:Inactive"));
    inactive.writeEntry("ChangeSelectionColor", true);
    inactive.writeEntry("Enable", false);
    KConfigGroup general = config.group(QStringLiteral("General"));
    general.writeEntry("ColorScheme", QFileInfo(file).completeBaseName());
    general.writeEntry("Name", preset.schemeName);
    return config.sync() ? file : QString();
}

/// Installed scheme files by base name; earlier, user folders win.
QList<std::pair<QString, QString>> installedFiles()
{
    QList<std::pair<QString, QString>> files;
    QSet<QString> seen;
    const QStringList folders =
        QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes"), QStandardPaths::LocateDirectory);
    for (const QString &folder : folders) {
        const QFileInfoList entries = QDir(folder).entryInfoList({QStringLiteral("*.colors")}, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &entry : entries) {
            const QString id = entry.completeBaseName();
            if (!seen.contains(id)) {
                seen.insert(id);
                files.append({id, entry.absoluteFilePath()});
            }
        }
    }
    return files;
}
}

QList<ColorSchemes::Scheme> ColorSchemes::available()
{
    QList<Scheme> schemes;
    for (const Preset &preset : presets()) {
        schemes.append({QLatin1String(preset.id), presetName(preset), true});
    }
    QList<Scheme> installed;
    for (const auto &[id, file] : installedFiles()) {
        const KConfig config(file, KConfig::SimpleConfig);
        const QString name = config.group(QStringLiteral("General")).readEntry("Name", id);
        installed.append({id, name.isEmpty() ? id : name, false});
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(installed.begin(), installed.end(), [&collator](const Scheme &first, const Scheme &second) {
        return collator.compare(first.name, second.name) < 0;
    });
    return schemes + installed;
}

QString ColorSchemes::path(const QString &id)
{
    if (id.isEmpty()) {
        return {};
    }
    if (const Preset *preset = presetFor(id)) {
        return writePreset(*preset);
    }
    for (const auto &[installedId, file] : installedFiles()) {
        if (installedId == id) {
            return file;
        }
    }
    return {};
}

bool ColorSchemes::apply(const QString &id)
{
    const QString file = path(id);
    if (file.isEmpty()) {
        QCoreApplication::instance()->setProperty(SchemePathProperty, QVariant());
        // An empty palette resolves entirely against the platform theme, so
        // later Plasma scheme changes are followed again.
        QGuiApplication::setPalette(QPalette());
        return id.isEmpty();
    }
    QCoreApplication::instance()->setProperty(SchemePathProperty, file);
    const KSharedConfigPtr config = KSharedConfig::openConfig(file, KConfig::SimpleConfig);
    config->reparseConfiguration();
    QGuiApplication::setPalette(KColorScheme::createApplicationPalette(config));
    return true;
}
