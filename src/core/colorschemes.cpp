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
#include <QStyleHints>
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QAccessibilityHints>
#endif

#include <algorithm>
#include <cmath>

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

// Breeze-derived palettes. Selections, links and inactive text differ from
// Breeze where needed so that every pair ColorSchemes::contrastIssues()
// checks meets the WCAG 2 AA minimum.
const QList<Preset> &presets()
{
    static const QList<Preset> presets{
        {ColorSchemes::LightPreset,
         "OpusLight.colors",
         "Plasma Opus Light",
         {{"Colors:View", "255,255,255", "247,247,247", "35,38,41", "96,107,118"},
          {"Colors:Window", "239,240,241", "227,229,231", "35,38,41", "96,107,118"},
          {"Colors:Button", "252,252,252", "163,212,250", "35,38,41", "96,107,118"},
          {"Colors:Selection", "38,113,165", "163,212,250", "255,255,255", "222,236,246"},
          {"Colors:Tooltip", "247,247,247", "239,240,241", "35,38,41", "96,107,118"},
          {"Colors:Complementary", "42,46,50", "27,30,32", "252,252,252", "161,169,177"},
          {"Colors:Header", "222,224,226", "239,240,241", "35,38,41", "96,107,118"}},
         "31,111,166",
         "155,89,182",
         "147,206,233",
         {{"activeBackground", "227,229,231"}, {"activeForeground", "35,38,41"}, {"inactiveBackground", "239,240,241"}, {"inactiveForeground", "96,107,118"}}},
        {ColorSchemes::DarkPreset,
         "OpusDark.colors",
         "Plasma Opus Dark",
         {{"Colors:View", "27,30,32", "35,38,41", "252,252,252", "161,169,177"},
          {"Colors:Window", "32,35,38", "41,44,48", "252,252,252", "161,169,177"},
          {"Colors:Button", "41,44,48", "30,87,116", "252,252,252", "161,169,177"},
          {"Colors:Selection", "41,121,176", "30,87,116", "252,252,252", "222,236,246"},
          {"Colors:Tooltip", "41,44,48", "32,35,38", "252,252,252", "161,169,177"},
          {"Colors:Complementary", "42,46,50", "27,30,32", "252,252,252", "161,169,177"},
          {"Colors:Header", "41,44,48", "32,35,38", "252,252,252", "161,169,177"}},
         "29,153,243",
         "187,134,219",
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

bool ColorSchemes::highContrastPreferred()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    return QGuiApplication::styleHints()->accessibility()->contrastPreference() == Qt::ContrastPreference::HighContrast;
#else
    return false;
#endif
}

QList<ColorSchemes::Role> ColorSchemes::roles()
{
    return {
        {QPalette::Window, QStringLiteral("Window"), i18nc("@label palette role", "Window background")},
        {QPalette::WindowText, QStringLiteral("WindowText"), i18nc("@label palette role", "Window text")},
        {QPalette::Base, QStringLiteral("Base"), i18nc("@label palette role", "View background")},
        {QPalette::AlternateBase, QStringLiteral("AlternateBase"), i18nc("@label palette role", "Alternate view background")},
        {QPalette::Text, QStringLiteral("Text"), i18nc("@label palette role", "View text")},
        {QPalette::PlaceholderText, QStringLiteral("PlaceholderText"), i18nc("@label palette role", "Placeholder text")},
        {QPalette::Button, QStringLiteral("Button"), i18nc("@label palette role", "Button background")},
        {QPalette::ButtonText, QStringLiteral("ButtonText"), i18nc("@label palette role", "Button text")},
        {QPalette::Highlight, QStringLiteral("Highlight"), i18nc("@label palette role", "Selection background")},
        {QPalette::HighlightedText, QStringLiteral("HighlightedText"), i18nc("@label palette role", "Selection text")},
        {QPalette::Accent, QStringLiteral("Accent"), i18nc("@label palette role", "Accent and focus")},
        {QPalette::Link, QStringLiteral("Link"), i18nc("@label palette role", "Link")},
        {QPalette::LinkVisited, QStringLiteral("LinkVisited"), i18nc("@label palette role", "Visited link")},
        {QPalette::ToolTipBase, QStringLiteral("ToolTipBase"), i18nc("@label palette role", "Tooltip background")},
        {QPalette::ToolTipText, QStringLiteral("ToolTipText"), i18nc("@label palette role", "Tooltip text")},
    };
}

ColorSchemes::Overrides ColorSchemes::parseOverrides(const QStringList &entries)
{
    const QList<Role> known = roles();
    Overrides overrides;
    for (const QString &entry : entries) {
        const qsizetype separator = entry.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        const QString key = entry.left(separator).trimmed();
        const QColor color = QColor::fromString(entry.mid(separator + 1).trimmed());
        const auto role = std::find_if(known.cbegin(), known.cend(), [&key](const Role &candidate) {
            return candidate.key == key;
        });
        if (role != known.cend() && color.isValid()) {
            overrides.insert(role->role, QColor(color.rgb()));
        }
    }
    return overrides;
}

QStringList ColorSchemes::serializeOverrides(const Overrides &overrides)
{
    QStringList entries;
    for (const Role &role : roles()) {
        if (overrides.contains(role.role)) {
            entries.append(role.key + QLatin1Char('=') + overrides.value(role.role).name(QColor::HexRgb));
        }
    }
    return entries;
}

QPalette ColorSchemes::palette(const QString &id)
{
    const QString file = path(id);
    const KSharedConfigPtr config =
        file.isEmpty() ? KSharedConfig::openConfig(QStringLiteral("kdeglobals")) : KSharedConfig::openConfig(file, KConfig::SimpleConfig);
    config->reparseConfiguration();
    return KColorScheme::createApplicationPalette(config);
}

QPalette ColorSchemes::withOverrides(QPalette palette, const Overrides &overrides)
{
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        palette.setColor(QPalette::Active, it.key(), it.value());
        palette.setColor(QPalette::Inactive, it.key(), it.value());
    }
    return palette;
}

QList<QPalette::ColorRole> ColorSchemes::accentRoles()
{
    return {QPalette::Highlight, QPalette::Accent, QPalette::HighlightedText};
}

ColorSchemes::Overrides ColorSchemes::accentOverrides(const QPalette &palette, const QColor &accent)
{
    if (!accent.isValid()) {
        return {};
    }
    const QColor opaque(accent.rgb());
    QColor text = palette.color(QPalette::Active, QPalette::HighlightedText);
    if (contrastRatio(text, opaque) < 4.5) {
        text = contrastRatio(Qt::white, opaque) >= contrastRatio(Qt::black, opaque) ? QColor(Qt::white) : QColor(Qt::black);
    }
    return {{QPalette::Highlight, opaque}, {QPalette::Accent, opaque}, {QPalette::HighlightedText, text}};
}

QStringList ColorSchemes::withAccent(const QString &id, const QString &accent, const QStringList &customColors)
{
    const QColor color = QColor::fromString(accent);
    if (accent.isEmpty() || !color.isValid()) {
        return customColors;
    }
    return serializeOverrides(accentOverrides(palette(id), color)) + customColors;
}

ColorSchemes::Overrides ColorSchemes::paneTint(const QPalette &palette, const QColor &tint, int strengthPercent)
{
    if (!tint.isValid() || strengthPercent <= 0) {
        return {};
    }
    const auto mix = [&tint](const QColor &color, int percent) {
        const auto channel = [percent](int from, int to) {
            return (from * (100 - percent) + to * percent + 50) / 100;
        };
        return QColor(channel(color.red(), tint.red()), channel(color.green(), tint.green()), channel(color.blue(), tint.blue()));
    };
    const QColor base = palette.color(QPalette::Active, QPalette::Base);
    const QColor alternate = palette.color(QPalette::Active, QPalette::AlternateBase);
    const QColor text = palette.color(QPalette::Active, QPalette::Text);
    const double minimum = std::min(4.5, std::min(contrastRatio(text, base), contrastRatio(text, alternate)));
    for (int percent = std::min(strengthPercent, 100); percent > 0; --percent) {
        const QColor tintedBase = mix(base, percent);
        const QColor tintedAlternate = mix(alternate, percent);
        if (contrastRatio(text, tintedBase) >= minimum && contrastRatio(text, tintedAlternate) >= minimum) {
            return {{QPalette::Base, tintedBase}, {QPalette::AlternateBase, tintedAlternate}};
        }
    }
    return {};
}

ColorSchemes::Overrides ColorSchemes::translucent(const QPalette &palette, const QList<QPalette::ColorRole> &roles, int opacityPercent)
{
    if (opacityPercent >= 100) {
        return {};
    }
    const int alpha = qRound(255 * std::clamp(opacityPercent, 0, 100) / 100.0);
    Overrides overrides;
    for (const QPalette::ColorRole role : roles) {
        QColor color = palette.color(QPalette::Active, role);
        color.setAlpha(alpha);
        overrides.insert(role, color);
    }
    return overrides;
}

double ColorSchemes::contrastRatio(const QColor &first, const QColor &second)
{
    const auto luminance = [](const QColor &color) {
        const auto channel = [](double value) {
            return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        const QColor rgb = color.toRgb();
        return 0.2126 * channel(rgb.redF()) + 0.7152 * channel(rgb.greenF()) + 0.0722 * channel(rgb.blueF());
    };
    const auto [darker, lighter] = std::minmax(luminance(first), luminance(second));
    return (lighter + 0.05) / (darker + 0.05);
}

QColor ColorSchemes::readable(const QColor &color, const QColor &background, double minimum)
{
    if (!color.isValid()) {
        return color;
    }
    const QColor opaque(color.rgb());
    if (contrastRatio(opaque, background) >= minimum) {
        return opaque;
    }
    const QColor target = contrastRatio(Qt::black, background) >= contrastRatio(Qt::white, background) ? QColor(Qt::black) : QColor(Qt::white);
    for (int percent = 5; percent < 100; percent += 5) {
        const double amount = percent / 100.0;
        const QColor candidate = QColor::fromRgbF(opaque.redF() + (target.redF() - opaque.redF()) * amount,
                                                  opaque.greenF() + (target.greenF() - opaque.greenF()) * amount,
                                                  opaque.blueF() + (target.blueF() - opaque.blueF()) * amount);
        if (contrastRatio(candidate, background) >= minimum) {
            return candidate;
        }
    }
    return target;
}

QList<ColorSchemes::ContrastIssue> ColorSchemes::contrastIssues(const QPalette &palette, const QList<QPalette::ColorRole> &changed)
{
    struct Pair {
        QPalette::ColorRole foreground;
        QPalette::ColorRole background;
        double minimum;
        QString name;
    };
    const QList<Pair> pairs{
        {QPalette::WindowText, QPalette::Window, 4.5, i18nc("@item colour pair", "Window text on window background")},
        {QPalette::Text, QPalette::Base, 4.5, i18nc("@item colour pair", "View text on view background")},
        {QPalette::Text, QPalette::AlternateBase, 4.5, i18nc("@item colour pair", "View text on alternate view background")},
        {QPalette::PlaceholderText, QPalette::Base, 4.5, i18nc("@item colour pair", "Placeholder text on view background")},
        {QPalette::ButtonText, QPalette::Button, 4.5, i18nc("@item colour pair", "Button text on button background")},
        {QPalette::HighlightedText, QPalette::Highlight, 4.5, i18nc("@item colour pair", "Selection text on selection background")},
        {QPalette::Link, QPalette::Base, 4.5, i18nc("@item colour pair", "Link on view background")},
        {QPalette::LinkVisited, QPalette::Base, 4.5, i18nc("@item colour pair", "Visited link on view background")},
        {QPalette::ToolTipText, QPalette::ToolTipBase, 4.5, i18nc("@item colour pair", "Tooltip text on tooltip background")},
        {QPalette::Highlight, QPalette::Base, 3, i18nc("@item colour pair", "Selection against view background")},
        {QPalette::Accent, QPalette::Base, 3, i18nc("@item colour pair", "Focus indicator against view background")},
        {QPalette::Accent, QPalette::Window, 3, i18nc("@item colour pair", "Focus indicator against window background")},
    };
    QList<ContrastIssue> issues;
    for (const Pair &pair : pairs) {
        if (!changed.isEmpty() && !changed.contains(pair.foreground) && !changed.contains(pair.background)) {
            continue;
        }
        const double ratio = contrastRatio(palette.color(QPalette::Active, pair.foreground), palette.color(QPalette::Active, pair.background));
        // Compare at the displayed precision, so 4.49 is not reported as 4.5.
        if (std::floor(ratio * 10) / 10 < pair.minimum) {
            issues.append({pair.name, ratio, pair.minimum});
        }
    }
    return issues;
}

bool ColorSchemes::apply(const QString &id, const QStringList &customColors, bool highContrast)
{
    const QString file = highContrast ? QString() : path(id);
    const Overrides overrides = highContrast ? Overrides() : parseOverrides(customColors);
    if (file.isEmpty()) {
        QCoreApplication::instance()->setProperty(SchemePathProperty, QVariant());
        // A palette without resolved roles resolves entirely against the
        // platform theme, so later Plasma scheme changes are followed again;
        // custom colours resolve only their own roles.
        QGuiApplication::setPalette(withOverrides(QPalette(), overrides));
        return highContrast || id.isEmpty();
    }
    QCoreApplication::instance()->setProperty(SchemePathProperty, file);
    const KSharedConfigPtr config = KSharedConfig::openConfig(file, KConfig::SimpleConfig);
    config->reparseConfiguration();
    QGuiApplication::setPalette(withOverrides(KColorScheme::createApplicationPalette(config), overrides));
    return true;
}
