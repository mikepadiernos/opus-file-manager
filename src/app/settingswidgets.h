// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QComboBox>
#include <QHash>
#include <QListWidget>
#include <QPalette>
#include <QSpinBox>
#include <QWidget>

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

/**
 * Follows the colour scheme's accent or chooses an app-local accent colour,
 * reporting when it lacks contrast against the selected scheme. Bound to the
 * AccentColor setting.
 */
class AccentColorChooser : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged USER true)

public:
    explicit AccentColorChooser(QWidget *parent = nullptr);
    /// The chosen colour as #rrggbb, or empty to follow the scheme.
    QString accent() const;
    void setAccent(const QString &accent);
    /// Validates the accent against the given ColorScheme setting value.
    void setScheme(const QString &id);

Q_SIGNALS:
    void accentChanged();

private:
    void refresh();

    QCheckBox *m_custom;
    KColorButton *m_button;
    KMessageWidget *m_contrast;
    QPalette m_base;
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
    Kind m_kind;
    QListWidget *m_list;
};

class IconInformationList : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList information READ information WRITE setInformation NOTIFY informationChanged USER true)

public:
    explicit IconInformationList(QWidget *parent = nullptr);
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
