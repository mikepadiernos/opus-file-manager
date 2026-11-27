// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldercolorrules.h"
#include "themeprofiles.h"

#include <QComboBox>
#include <QDialog>
#include <QHash>
#include <QListWidget>
#include <QPalette>
#include <QSpinBox>
#include <QWidget>

#include <functional>
#include <optional>

class QCheckBox;
class QLabel;
class QLineEdit;

/// Widgets that KConfigDialog binds to settings through their "kcfg_<Entry>" object names.
namespace SettingsWidgets
{
QComboBox *comboBox(const QString &entry, const QStringList &choices);
QCheckBox *checkBox(const QString &entry, const QString &text);
QSpinBox *pixelSpinBox(const QString &entry, int minimum, int maximum);

/// Binds @p editor to @p entry through its @p property.
template<typename Editor>
Editor *bound(Editor *editor, const QString &entry, const char *property)
{
    editor->setObjectName(QStringLiteral("kcfg_") + entry);
    editor->setProperty("kcfg_property", QByteArray(property));
    return editor;
}
}

class InterfaceLanguageCombo : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged USER true)

public:
    explicit InterfaceLanguageCombo(QWidget *parent = nullptr);
    QString language() const;
    void setLanguage(const QString &language);

Q_SIGNALS:
    void languageChanged();
};

class RegionalLocaleCombo : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString localeName READ localeName WRITE setLocaleName NOTIFY localeNameChanged USER true)

public:
    explicit RegionalLocaleCombo(QWidget *parent = nullptr);
    QString localeName() const;
    void setLocaleName(const QString &name);

Q_SIGNALS:
    void localeNameChanged();
};

/**
 * Chooses the startup workspace: the last used one, none, or a saved
 * workspace. A chosen workspace that no longer exists stays selected as
 * unavailable, so its reference is kept until the user changes it.
 */
class StartupWorkspaceCombo : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged USER true)

public:
    explicit StartupWorkspaceCombo(QWidget *parent = nullptr);
    QString workspace() const;
    void setWorkspace(const QString &workspace);
    /**
     * Lists the catalog's workspaces, keeping the current choice.
     * @param workspaces each workspace's UUID and display name
     */
    void setWorkspaces(const QList<std::pair<QString, QString>> &workspaces);

Q_SIGNALS:
    void workspaceChanged();

private:
    QList<std::pair<QString, QString>> m_workspaces;
};

/**
 * Chooses the shared saved path filter applied to new searches, or none. A
 * chosen filter that no longer exists stays selected as unavailable, so its
 * reference is kept until the user changes it.
 */
class SavedPathFilterCombo : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString filterName READ filterName WRITE setFilterName NOTIFY filterNameChanged USER true)

public:
    explicit SavedPathFilterCombo(QWidget *parent = nullptr);
    QString filterName() const;
    void setFilterName(const QString &name);
    /// Rereads the shared saved path filters, keeping the current choice.
    void reload();

Q_SIGNALS:
    void filterNameChanged();
};

/**
 * Chooses the app-local colour scheme: follow Plasma, a bundled preset or an
 * installed KDE colour scheme. A chosen scheme that is no longer installed
 * stays selected as unavailable, so its reference is kept until changed.
 */
class ColorSchemeCombo : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString scheme READ scheme WRITE setScheme NOTIFY schemeChanged USER true)

public:
    explicit ColorSchemeCombo(QWidget *parent = nullptr);
    QString scheme() const;
    void setScheme(const QString &scheme);

Q_SIGNALS:
    void schemeChanged();
};

class KColorButton;
class KMessageWidget;
class QCheckBox;
class QFrame;
class QPushButton;
class QHBoxLayout;
class QToolButton;

/**
 * Creates a hidden, word-wrapped message for contrast feedback below a colour
 * editor.
 * @param adjusted whether Plasma Opus adjusts the colour to stay readable, so
 *        the message informs rather than warns
 */
KMessageWidget *createContrastNotice(const QString &objectName, bool adjusted, QWidget *parent);

/**
 * Follows the colour scheme's accent or Plasma's accent, or chooses an
 * app-local accent colour, reporting when it lacks contrast against the
 * selected scheme. Bound to the AccentColor setting.
 */
class AccentColorChooser : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged USER true)

public:
    /// Items of the accent mode combo box.
    enum Mode {
        FollowScheme,
        FollowPlasma,
        Custom
    };

    explicit AccentColorChooser(QWidget *parent = nullptr);
    /// The chosen colour as #rrggbb, ColorSchemes::PlasmaAccent, or empty to follow the scheme.
    QString accent() const;
    void setAccent(const QString &accent);
    /// Validates the accent against the given ColorScheme setting value.
    void setScheme(const QString &id);

Q_SIGNALS:
    void accentChanged();

private:
    void refresh();

    QComboBox *m_mode;
    KColorButton *m_button;
    KMessageWidget *m_contrast;
    QPalette m_base;
    bool m_schemeFollowsPlasma = true;
};

/**
 * Chooses the colour given to new tags, or no colour. Bound to the
 * DefaultTagColor setting; a value that isn't #rrggbb reads as no colour.
 */
class TagColorChooser : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged USER true)

public:
    explicit TagColorChooser(QWidget *parent = nullptr);
    /// The chosen colour as lowercase #rrggbb, or empty for no colour.
    QString color() const;
    void setColor(const QString &color);

Q_SIGNALS:
    void colorChanged();

private:
    QCheckBox *m_enabled;
    KColorButton *m_button;
};

/**
 * Edits app-local palette role colours layered over the selected colour
 * scheme, with a live preview and contrast feedback. Unchecked roles follow
 * the scheme. Bound to the CustomColors setting by KConfigDialog; Revert and
 * the dialog's Cancel discard unsaved edits.
 */
class ColorPaletteEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList colors READ colors WRITE setColors NOTIFY colorsChanged USER true)

public:
    explicit ColorPaletteEditor(QWidget *parent = nullptr);
    QStringList colors() const;
    void setColors(const QStringList &colors);
    /**
     * Shows the given scheme's colours for roles that are not overridden.
     * @param id a ColorScheme setting value
     */
    void setScheme(const QString &id);
    /**
     * Shows the given accent colour for the roles it sets.
     * @param accent an AccentColor setting value
     */
    void setAccent(const QString &accent);

Q_SIGNALS:
    void colorsChanged();

private:
    struct Row {
        QPalette::ColorRole role;
        QCheckBox *override;
        KColorButton *button;
    };
    void updateBase();
    void refresh();

    QList<Row> m_rows;
    QHash<QPalette::ColorRole, QColor> m_overrides;
    QString m_scheme;
    QString m_accent;
    QPalette m_base;
    QFrame *m_preview;
    KMessageWidget *m_contrast;
    QPushButton *m_revert = nullptr;
};

/**
 * Edits a short list of colours as #rrggbb, such as tree line colours by
 * nesting depth, and reports colours that are adjusted for contrast against
 * the view background. Bound to a StringList setting by KConfigDialog.
 */
class ColorListEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList colors READ colors WRITE setColors NOTIFY colorsChanged USER true)

public:
    static constexpr int MaximumColors = 8;

    explicit ColorListEditor(QWidget *parent = nullptr);
    QStringList colors() const;
    void setColors(const QStringList &colors);
    /// Sets the background that colours are validated against.
    void setBackground(const QColor &background);

Q_SIGNALS:
    void colorsChanged();

private:
    void addButton(const QColor &color);
    void refresh();

    QList<KColorButton *> m_buttons;
    QHBoxLayout *m_row;
    QToolButton *m_add;
    QToolButton *m_remove;
    KMessageWidget *m_contrast;
    QColor m_background;
};

class RuleListEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList rules READ rules WRITE setRules NOTIFY rulesChanged USER true)

public:
    enum class Kind {
        Ignore,
        Color,
        SelectionFilter
    };
    explicit RuleListEditor(Kind kind, QWidget *parent = nullptr);
    QStringList rules() const;
    void setRules(const QStringList &rules);

Q_SIGNALS:
    void rulesChanged();

private:
    void editRule(int row);
    void updateContrast();
    Kind m_kind;
    QListWidget *m_list;
    /// Reports file colours that are hard to read; only for Kind::Color.
    KMessageWidget *m_contrast = nullptr;
};

/**
 * Lists absolute local folders, with buttons to add folders from a folder
 * chooser and remove the selected one. Bound to a StringList setting.
 */
class FolderListEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList folders READ folders WRITE setFolders NOTIFY foldersChanged USER true)

public:
    /// @param chooserTitle the folder chooser's title
    explicit FolderListEditor(const QString &chooserTitle, QWidget *parent = nullptr);
    QStringList folders() const;
    void setFolders(const QStringList &folders);
    /// Adds a folder unless it is already listed or not an absolute path.
    bool addFolder(const QString &folder);

Q_SIGNALS:
    void foldersChanged();

private:
    QString m_chooserTitle;
    QListWidget *m_list;
    QPushButton *m_remove;
};

/**
 * Edits one folder colour rule: what it matches, the pattern, the colour and
 * whether subfolders inherit it.
 */
class FolderColorRuleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FolderColorRuleDialog(QWidget *parent = nullptr);
    /// The edited rule, or nothing while the input is invalid.
    std::optional<FolderColorRules::Rule> rule() const;
    void setRule(const FolderColorRules::Rule &rule);

private:
    void validate();
    void updateContrast();

    QComboBox *m_match;
    QLineEdit *m_pattern;
    QPushButton *m_browse;
    KColorButton *m_color;
    QCheckBox *m_subfolders;
    KMessageWidget *m_contrast;
    QPushButton *m_ok = nullptr;
};

/**
 * Lists folder colour rules in the order they are checked, with buttons to
 * add, edit, remove and reorder them. Bound to the FolderColorRules setting.
 */
class FolderColorRuleEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList rules READ rules WRITE setRules NOTIFY rulesChanged USER true)

public:
    explicit FolderColorRuleEditor(QWidget *parent = nullptr);
    QStringList rules() const;
    void setRules(const QStringList &rules);
    /// Moves the rule at @p row by @p offset rows, keeping it selected.
    void moveRule(int row, int offset);

Q_SIGNALS:
    void rulesChanged();

private:
    void editRule(int row);
    void updateButtons();

    QListWidget *m_list;
    QPushButton *m_add;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QPushButton *m_up;
    QPushButton *m_down;
};

class KFontRequester;

/**
 * Follows Plasma's font or chooses an app-local font for one area, such as
 * file lists. Bound to a font setting stored by AppFonts::toSetting().
 */
class FontOverrideChooser : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString fontSetting READ fontSetting WRITE setFontSetting NOTIFY fontSettingChanged USER true)

public:
    /// @param accessibleName names the area, such as "File list font"
    explicit FontOverrideChooser(const QString &accessibleName, QWidget *parent = nullptr);
    /// The chosen font, or empty to follow Plasma.
    QString fontSetting() const;
    void setFontSetting(const QString &setting);

Q_SIGNALS:
    void fontSettingChanged();

private:
    QCheckBox *m_custom;
    KFontRequester *m_requester;
};

class QPlainTextEdit;
class QTimer;

/**
 * Edits the app-local Qt Style Sheet, checks it as it is typed and previews
 * it on sample widgets. Bound to the CustomStyleSheet setting.
 */
class StyleSheetEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString sheet READ sheet WRITE setSheet NOTIFY sheetChanged USER true)

public:
    explicit StyleSheetEditor(QWidget *parent = nullptr);
    QString sheet() const;
    void setSheet(const QString &sheet);
    /// The problem found by the last check, or an empty string.
    QString problem() const;
    /// Checks the text and updates the status and the preview now.
    void check();
    /// Replaces the text with a stylesheet file.
    bool importFile(const QString &path);
    bool exportFile(const QString &path);
    /**
     * Exports to @p path, as chosen in a file dialog. Adds the .qss suffix
     * when the name has no .qss or .css suffix and asks before replacing a
     * file with the suffixed name, which the file dialog did not ask about.
     */
    bool exportChosen(QString path);
    /// Replaces the yes or no question, for tests.
    void setConfirmation(std::function<bool(const QString &)> confirm);
    /// Returns to the saved stylesheet.
    void revert();

Q_SIGNALS:
    void sheetChanged();

private:
    void showStatus(int type, const QString &text);

    QPlainTextEdit *m_text;
    KMessageWidget *m_status;
    QFrame *m_preview;
    QTimer *m_checkTimer;
    QString m_problem;
    std::function<bool(const QString &)> m_confirm;
};

/**
 * Saves, loads, imports, exports and removes theme profiles. Saved profiles
 * are written at once; loading a profile only fills the settings dialog, so
 * Apply, OK or Cancel decide whether it is used.
 */
class ThemeProfileManager : public QWidget
{
    Q_OBJECT

public:
    struct Callbacks {
        /// The appearance values currently shown in the dialog.
        std::function<QVariantMap()> current;
        /// Shows @p values in the dialog without applying them.
        std::function<void(const QVariantMap &values)> show;
        /// Shows Plasma's defaults in the dialog without applying them.
        std::function<void()> reset;
    };

    explicit ThemeProfileManager(Callbacks callbacks, QWidget *parent = nullptr);

    /// Replaces the question asked before replacing or removing a profile, for tests.
    void setConfirmation(std::function<bool(const QString &question)> confirm);

    /// The id of the selected profile, or an empty string.
    QString selectedId() const;

    /// Saves the dialog's values as @p name, asking before replacing a profile.
    bool saveCurrent(const QString &name);
    /// Shows a saved profile's values in the dialog.
    bool loadProfile(const QString &id);
    /// Imports a profile file, asking before replacing a profile with its name.
    bool importProfile(const QString &path);
    bool exportProfile(const QString &id, const QString &path, bool includeStyleSheet = true);
    /**
     * Exports the selected profile to @p path, as chosen in a file dialog.
     * Adds the .opus-theme.json suffix when the name has no .json suffix and
     * asks before replacing a file with the suffixed name, which the file
     * dialog did not ask about, and whether to include a custom stylesheet.
     */
    bool exportSelected(QString path);
    bool removeProfile(const QString &id);

private:
    void reload(const QString &selectId = QString());
    void updateButtons();
    void showMessage(int type, const QString &text);
    bool store(const QString &name, const QVariantMap &values, QString *id);
    QString unavailableText(const QVariantMap &values) const;

    Callbacks m_callbacks;
    std::function<bool(const QString &)> m_confirm;
    QList<ThemeProfiles::Profile> m_profiles;
    QListWidget *m_list;
    KMessageWidget *m_message;
    QPushButton *m_load;
    QPushButton *m_export;
    QPushButton *m_remove;
};

class IconInformationList : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList information READ information WRITE setInformation NOTIFY informationChanged USER true)

public:
    /// @param cardFields also offer the tags, rating and colour label fields that cards show
    explicit IconInformationList(QWidget *parent = nullptr, bool cardFields = false);
    QStringList information() const;
    void setInformation(const QStringList &information);

Q_SIGNALS:
    void informationChanged();
};

class HiddenItemsList : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList hiddenItems READ hiddenItems WRITE setHiddenItems NOTIFY hiddenItemsChanged USER true)

public:
    explicit HiddenItemsList(QWidget *parent = nullptr);
    void addItem(const QString &id, const QString &label);
    /**
     * Replaces the listed items with @p items, given as id and label pairs,
     * keeping the shown or hidden state of every id, including unsaved
     * changes and ids that are no longer listed.
     */
    void setItems(const QList<std::pair<QString, QString>> &items);
    QStringList hiddenItems() const;
    void setHiddenItems(const QStringList &hidden);

Q_SIGNALS:
    void hiddenItemsChanged();

private:
    QStringList m_missingItems;
};

/**
 * Checkable list of the installed KIO thumbnailer plugins, bound to a
 * StringList setting of plugin ids by KConfigDialog.
 */
class PreviewPluginList : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList plugins READ plugins WRITE setPlugins NOTIFY pluginsChanged USER true)

public:
    explicit PreviewPluginList(QWidget *parent = nullptr);

    /// Ids of the checked plugins, in list order.
    QStringList plugins() const;
    /**
     * Checks exactly the given plugins.
     * @param plugins plugin ids; ids that are not installed are ignored
     */
    void setPlugins(const QStringList &plugins);

Q_SIGNALS:
    void pluginsChanged();
};

/**
 * Edits the relative widths of the parent, current and preview columns of
 * Yazi mode, within YaziMode::clampedRatios() limits. Bound to the
 * YaziColumnRatios setting. A stored value that YaziMode::ratiosFromList()
 * rejects shows the defaults, but is returned unchanged until edited.
 */
class YaziColumnRatiosEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<int> ratios READ ratios WRITE setRatios NOTIFY ratiosChanged USER true)

public:
    explicit YaziColumnRatiosEditor(QWidget *parent = nullptr);
    /// The parent, current and preview widths.
    QList<int> ratios() const;
    void setRatios(const QList<int> &ratios);
    /// Shows the default widths, 1, 3 and 2.
    void restoreDefaultWidths();

Q_SIGNALS:
    void ratiosChanged();

private:
    void edited();
    void updateShares();

    QSpinBox *m_parent;
    QSpinBox *m_current;
    QSpinBox *m_preview;
    QLabel *m_shares;
    QList<int> m_stored;
    bool m_edited = false;
};

/**
 * File size limit edited in MiB and stored in bytes. Zero in the spin box
 * stands for no limit, stored as the largest qulonglong.
 */
class PreviewSizeSpinBox : public QSpinBox
{
    Q_OBJECT
    Q_PROPERTY(qulonglong bytes READ bytes WRITE setBytes NOTIFY bytesChanged USER true)

public:
    explicit PreviewSizeSpinBox(QWidget *parent = nullptr);

    qulonglong bytes() const;
    /**
     * Shows a byte count, rounded up to whole MiB.
     * @param bytes the limit; the largest qulonglong means no limit
     */
    void setBytes(qulonglong bytes);

Q_SIGNALS:
    void bytesChanged();
};
