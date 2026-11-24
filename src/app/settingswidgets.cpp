// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingswidgets.h"

#include "colorschemes.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "settings.h"
#include "workspacestartup.h"

#include <KColorButton>
#include <KIO/PreviewJob>
#include <KLanguageName>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KPluginMetaData>

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr qulonglong MiB = 1024 * 1024;
constexpr qulonglong NoLimit = std::numeric_limits<qulonglong>::max();
constexpr int MaximumMiB = 100000;

/// Lists contrast issues in @p message under @p intro, or hides it when there are none.
void showContrastIssues(KMessageWidget *message, const QString &intro, const QList<ColorSchemes::ContrastIssue> &issues)
{
    if (issues.isEmpty()) {
        message->hide();
        return;
    }
    QStringList lines;
    for (const ColorSchemes::ContrastIssue &issue : issues) {
        lines.append(i18nc("@info contrast issue: colour pair, ratio, minimum",
                           "%1: %2:1, at least %3:1 recommended",
                           issue.pair,
                           QString::number(std::floor(issue.ratio * 10) / 10, 'f', 1),
                           QString::number(issue.minimum, 'f', 1))
                         .toHtmlEscaped());
    }
    message->setText(intro.toHtmlEscaped() + QStringLiteral("<ul><li>") + lines.join(QStringLiteral("</li><li>")) + QStringLiteral("</li></ul>"));
    message->show();
}

QStringList sanitizedRules(RuleListEditor::Kind kind, const QStringList &rules)
{
    if (kind == RuleListEditor::Kind::Ignore) {
        return IgnoreList(rules).entries();
    }
    if (kind == RuleListEditor::Kind::Color) {
        return FileColorRules(rules).entries();
    }
    QStringList result;
    for (const QString &rule : rules) {
        const QString pattern = rule.trimmed();
        if (!pattern.isEmpty() && !result.contains(pattern)) {
            result.append(pattern);
        }
    }
    return result;
}
}

InterfaceLanguageCombo::InterfaceLanguageCombo(QWidget *parent)
    : QComboBox(parent)
{
    addItem(i18nc("@item:inlistbox interface language", "Follow system language"), QString());
    QStringList languages = KLocalizedString::availableDomainTranslations(QByteArrayLiteral("opus-file-manager")).values();
    if (!languages.contains(QStringLiteral("en_US"))) {
        languages.append(QStringLiteral("en_US"));
    }
    languages.sort();
    for (const QString &code : std::as_const(languages)) {
        const QString name = KLanguageName::nameForCode(code);
        addItem(name.isEmpty() ? code : i18nc("@item:inlistbox language and code", "%1 (%2)", name, code), code);
    }
    connect(this, &QComboBox::currentIndexChanged, this, &InterfaceLanguageCombo::languageChanged);
}

QString InterfaceLanguageCombo::language() const
{
    return currentData().toString();
}

void InterfaceLanguageCombo::setLanguage(const QString &language)
{
    int index = findData(language);
    if (index < 0) {
        addItem(i18nc("@item:inlistbox unavailable language", "%1 (translation not installed)", language), language);
        index = count() - 1;
    }
    setCurrentIndex(index);
}

RegionalLocaleCombo::RegionalLocaleCombo(QWidget *parent)
    : QComboBox(parent)
{
    addItem(i18nc("@item:inlistbox regional locale", "Follow system locale"), QString());
    static const QStringList names = [] {
        QSet<QString> unique;
        for (const QLocale &locale : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory)) {
            unique.insert(locale.name());
        }
        QStringList result = unique.values();
        result.sort();
        return result;
    }();
    for (const QString &name : std::as_const(names)) {
        const QLocale locale(name);
        const QString label = locale.nativeLanguageName();
        addItem(label.isEmpty() ? name : i18nc("@item:inlistbox locale and code", "%1 (%2)", label, name), name);
    }
    connect(this, &QComboBox::currentIndexChanged, this, &RegionalLocaleCombo::localeNameChanged);
}

QString RegionalLocaleCombo::localeName() const
{
    return currentData().toString();
}

void RegionalLocaleCombo::setLocaleName(const QString &name)
{
    int index = findData(name);
    if (index < 0) {
        addItem(i18nc("@item:inlistbox unavailable locale", "%1 (locale not available)", name), name);
        index = count() - 1;
    }
    setCurrentIndex(index);
}

StartupWorkspaceCombo::StartupWorkspaceCombo(QWidget *parent)
    : QComboBox(parent)
{
    setWorkspace(QString());
    connect(this, &QComboBox::currentIndexChanged, this, &StartupWorkspaceCombo::workspaceChanged);
}

QString StartupWorkspaceCombo::workspace() const
{
    return currentData().toString();
}

void StartupWorkspaceCombo::setWorkspace(const QString &workspace)
{
    const QString previous = this->workspace();
    {
        const QSignalBlocker blocker(this);
        clear();
        addItem(i18nc("@item:inlistbox startup workspace", "Last used workspace"), QString());
        addItem(i18nc("@item:inlistbox startup workspace", "No workspace"), QLatin1String(WorkspaceStartup::NoWorkspace));
        for (const auto &[id, name] : std::as_const(m_workspaces)) {
            addItem(name, id);
        }
        int index = findData(workspace);
        if (index < 0) {
            addItem(i18nc("@item:inlistbox startup workspace that was deleted", "Unavailable workspace (%1)", workspace), workspace);
            index = count() - 1;
        }
        setCurrentIndex(index);
    }
    if (previous != this->workspace()) {
        Q_EMIT workspaceChanged();
    }
}

void StartupWorkspaceCombo::setWorkspaces(const QList<std::pair<QString, QString>> &workspaces)
{
    m_workspaces = workspaces;
    setWorkspace(workspace());
}

ColorSchemeCombo::ColorSchemeCombo(QWidget *parent)
    : QComboBox(parent)
{
    // KConfigDialogManager would otherwise watch QComboBox::activated, which
    // misses programmatic changes.
    setProperty("kcfg_propertyNotify", QByteArray(SIGNAL(schemeChanged())));
    setScheme(QString());
    connect(this, &QComboBox::currentIndexChanged, this, &ColorSchemeCombo::schemeChanged);
}

QString ColorSchemeCombo::scheme() const
{
    return currentData().toString();
}

void ColorSchemeCombo::setScheme(const QString &scheme)
{
    const QString previous = this->scheme();
    {
        const QSignalBlocker blocker(this);
        clear();
        addItem(i18nc("@item:inlistbox colour scheme", "Follow Plasma colour scheme"), QString());
        bool presets = true;
        for (const ColorSchemes::Scheme &available : ColorSchemes::available()) {
            if (presets && !available.preset) {
                insertSeparator(count());
                presets = false;
            }
            addItem(available.name, available.id);
        }
        int index = findData(scheme);
        if (index < 0) {
            addItem(i18nc("@item:inlistbox colour scheme that is no longer installed", "Unavailable colour scheme (%1)", scheme), scheme);
            index = count() - 1;
        }
        setCurrentIndex(index);
    }
    if (previous != this->scheme()) {
        Q_EMIT schemeChanged();
    }
}

AccentColorChooser::AccentColorChooser(QWidget *parent)
    : QWidget(parent)
    , m_custom(new QCheckBox(i18nc("@option:check", "Use an app-local accent colour"), this))
    , m_button(new KColorButton(this))
    , m_contrast(new KMessageWidget(this))
    , m_base(ColorSchemes::palette(QString()))
{
    setProperty("kcfg_propertyNotify", QByteArray(SIGNAL(accentChanged())));
    m_custom->setObjectName(QStringLiteral("customAccent"));
    m_button->setObjectName(QStringLiteral("accentColour"));
    m_button->setAccessibleName(i18nc("@label", "Accent colour"));
    m_contrast->setObjectName(QStringLiteral("accentContrastWarning"));
    m_contrast->setMessageType(KMessageWidget::Warning);
    m_contrast->setCloseButtonVisible(false);
    m_contrast->setWordWrap(true);
    m_contrast->hide();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *row = new QHBoxLayout;
    row->addWidget(m_custom);
    row->addWidget(m_button);
    row->addStretch();
    layout->addLayout(row);
    layout->addWidget(m_contrast);
    connect(m_custom, &QCheckBox::toggled, this, [this] {
        refresh();
        Q_EMIT accentChanged();
    });
    connect(m_button, &KColorButton::changed, this, [this] {
        if (m_custom->isChecked()) {
            refresh();
            Q_EMIT accentChanged();
        }
    });
    refresh();
}

QString AccentColorChooser::accent() const
{
    return m_custom->isChecked() ? QColor(m_button->color().rgb()).name(QColor::HexRgb) : QString();
}

void AccentColorChooser::setAccent(const QString &accent)
{
    const QColor color = QColor::fromString(accent);
    const QString previous = this->accent();
    {
        const QSignalBlocker checkBlocker(m_custom);
        const QSignalBlocker buttonBlocker(m_button);
        m_custom->setChecked(!accent.isEmpty() && color.isValid());
        if (m_custom->isChecked()) {
            m_button->setColor(color);
        }
    }
    refresh();
    if (previous != this->accent()) {
        Q_EMIT accentChanged();
    }
}

void AccentColorChooser::setScheme(const QString &id)
{
    m_base = ColorSchemes::palette(id);
    refresh();
}

void AccentColorChooser::refresh()
{
    m_button->setEnabled(m_custom->isChecked());
    if (!m_custom->isChecked()) {
        const QSignalBlocker blocker(m_button);
        m_button->setColor(m_base.color(QPalette::Active, QPalette::Accent));
        m_contrast->hide();
        return;
    }
    const QPalette palette = ColorSchemes::withOverrides(m_base, ColorSchemes::accentOverrides(m_base, m_button->color()));
    showContrastIssues(m_contrast,
                       i18nc("@info", "The accent colour may be hard to see with this colour scheme:"),
                       ColorSchemes::contrastIssues(palette, ColorSchemes::accentRoles()));
}

ColorPaletteEditor::ColorPaletteEditor(QWidget *parent)
    : QWidget(parent)
    , m_base(ColorSchemes::palette(QString()))
    , m_preview(new QFrame(this))
    , m_contrast(new KMessageWidget(this))
{
    setProperty("kcfg_propertyNotify", QByteArray(SIGNAL(colorsChanged())));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *roles = new QGridLayout;
    int row = 0;
    for (const ColorSchemes::Role &role : ColorSchemes::roles()) {
        auto *override = new QCheckBox(role.name, this);
        override->setObjectName(QStringLiteral("override") + role.key);
        auto *button = new KColorButton(this);
        button->setObjectName(QStringLiteral("colour") + role.key);
        button->setAccessibleName(role.name);
        roles->addWidget(override, row, 0);
        roles->addWidget(button, row, 1);
        ++row;
        const QPalette::ColorRole paletteRole = role.role;
        connect(override, &QCheckBox::toggled, this, [this, paletteRole, button](bool checked) {
            if (checked) {
                m_overrides.insert(paletteRole, button->color());
            } else {
                m_overrides.remove(paletteRole);
            }
            refresh();
            Q_EMIT colorsChanged();
        });
        connect(button, &KColorButton::changed, this, [this, paletteRole, override](const QColor &color) {
            if (!override->isChecked()) {
                return;
            }
            m_overrides.insert(paletteRole, QColor(color.rgb()));
            refresh();
            Q_EMIT colorsChanged();
        });
        m_rows.append({paletteRole, override, button});
    }
    auto *resetAll = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-clear-all")), i18nc("@action:button", "Use Scheme Colours"), this);
    resetAll->setObjectName(QStringLiteral("useSchemeColours"));
    resetAll->setToolTip(i18nc("@info:tooltip", "Stop overriding every palette role"));
    connect(resetAll, &QPushButton::clicked, this, [this] {
        setColors({});
    });
    m_revert = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-undo")), i18nc("@action:button", "Revert"), this);
    m_revert->setObjectName(QStringLiteral("revertColours"));
    m_revert->setToolTip(i18nc("@info:tooltip", "Discard colour changes that have not been applied"));
    connect(m_revert, &QPushButton::clicked, this, [this] {
        setColors(Settings::customColors());
    });
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(resetAll);
    buttons->addWidget(m_revert);
    buttons->addStretch();
    roles->addLayout(buttons, row, 0, 1, 2);
    roles->setRowStretch(row + 1, 1);
    layout->addLayout(roles);

    auto *side = new QVBoxLayout;
    m_preview->setObjectName(QStringLiteral("colourPreview"));
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setAutoFillBackground(true);
    auto *preview = new QVBoxLayout(m_preview);
    preview->addWidget(new QLabel(i18nc("@label colour preview", "Window text"), m_preview));
    auto *list = new QListWidget(m_preview);
    list->setAlternatingRowColors(true);
    list->addItems({i18nc("@item colour preview", "Documents"), i18nc("@item colour preview", "Selected item"), i18nc("@item colour preview", "Pictures")});
    list->setCurrentRow(1);
    list->setMaximumHeight(list->sizeHintForRow(0) * 4);
    list->setFocusPolicy(Qt::NoFocus);
    preview->addWidget(list);
    auto *text = new QLineEdit(i18nc("@info colour preview", "Text field"), m_preview);
    text->setReadOnly(true);
    preview->addWidget(text);
    auto *placeholder = new QLineEdit(m_preview);
    placeholder->setPlaceholderText(i18nc("@info colour preview", "Placeholder text"));
    placeholder->setReadOnly(true);
    preview->addWidget(placeholder);
    auto *link = new QLabel(QStringLiteral("<a href=\"#\">%1</a>").arg(i18nc("@info colour preview", "Link")), m_preview);
    link->setTextInteractionFlags(Qt::NoTextInteraction);
    preview->addWidget(link);
    preview->addWidget(new QPushButton(i18nc("@action:button colour preview", "Button"), m_preview));
    auto *tooltip = new QLabel(i18nc("@info colour preview", "Tooltip"), m_preview);
    tooltip->setAutoFillBackground(true);
    tooltip->setBackgroundRole(QPalette::ToolTipBase);
    tooltip->setForegroundRole(QPalette::ToolTipText);
    tooltip->setMargin(4);
    preview->addWidget(tooltip);
    preview->addStretch();
    side->addWidget(new QLabel(i18nc("@title:group", "Preview"), this));
    side->addWidget(m_preview);
    m_contrast->setObjectName(QStringLiteral("contrastWarning"));
    m_contrast->setMessageType(KMessageWidget::Warning);
    m_contrast->setCloseButtonVisible(false);
    m_contrast->setWordWrap(true);
    m_contrast->hide();
    side->addWidget(m_contrast);
    layout->addLayout(side, 1);
    connect(Settings::self(), &Settings::configChanged, this, &ColorPaletteEditor::refresh);
    refresh();
}

QStringList ColorPaletteEditor::colors() const
{
    return ColorSchemes::serializeOverrides(m_overrides);
}

void ColorPaletteEditor::setColors(const QStringList &colors)
{
    const ColorSchemes::Overrides overrides = ColorSchemes::parseOverrides(colors);
    if (overrides == m_overrides) {
        return;
    }
    m_overrides = overrides;
    refresh();
    Q_EMIT colorsChanged();
}

void ColorPaletteEditor::setScheme(const QString &id)
{
    m_scheme = id;
    updateBase();
}

void ColorPaletteEditor::setAccent(const QString &accent)
{
    m_accent = accent;
    updateBase();
}

void ColorPaletteEditor::updateBase()
{
    const QPalette scheme = ColorSchemes::palette(m_scheme);
    m_base = ColorSchemes::withOverrides(scheme, ColorSchemes::accentOverrides(scheme, QColor::fromString(m_accent)));
    refresh();
}

void ColorPaletteEditor::refresh()
{
    m_revert->setEnabled(colors() != ColorSchemes::serializeOverrides(ColorSchemes::parseOverrides(Settings::customColors())));
    for (const Row &row : std::as_const(m_rows)) {
        const bool overridden = m_overrides.contains(row.role);
        const QSignalBlocker overrideBlocker(row.override);
        const QSignalBlocker buttonBlocker(row.button);
        row.override->setChecked(overridden);
        row.button->setEnabled(overridden);
        row.button->setColor(overridden ? m_overrides.value(row.role) : m_base.color(QPalette::Active, row.role));
    }
    const QPalette palette = ColorSchemes::withOverrides(m_base, m_overrides);
    m_preview->setPalette(palette);
    showContrastIssues(m_contrast, i18nc("@info", "Some colours may be hard to read:"), ColorSchemes::contrastIssues(palette, m_overrides.keys()));
}

ColorListEditor::ColorListEditor(QWidget *parent)
    : QWidget(parent)
    , m_row(new QHBoxLayout)
    , m_add(new QToolButton(this))
    , m_remove(new QToolButton(this))
    , m_contrast(new KMessageWidget(this))
    , m_background(palette().color(QPalette::Base))
{
    setProperty("kcfg_propertyNotify", QByteArray(SIGNAL(colorsChanged())));
    m_add->setObjectName(QStringLiteral("addColour"));
    m_add->setIcon(QIcon::fromTheme(QStringLiteral("list-add-symbolic")));
    m_add->setToolTip(i18nc("@info:tooltip", "Add a colour"));
    m_add->setAccessibleName(m_add->toolTip());
    m_remove->setObjectName(QStringLiteral("removeColour"));
    m_remove->setIcon(QIcon::fromTheme(QStringLiteral("list-remove-symbolic")));
    m_remove->setToolTip(i18nc("@info:tooltip", "Remove the last colour"));
    m_remove->setAccessibleName(m_remove->toolTip());
    m_contrast->setObjectName(QStringLiteral("colourListContrastNotice"));
    m_contrast->setMessageType(KMessageWidget::Information);
    m_contrast->setCloseButtonVisible(false);
    m_contrast->setWordWrap(true);
    m_contrast->hide();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *row = new QHBoxLayout;
    m_row->setContentsMargins(0, 0, 0, 0);
    row->addLayout(m_row);
    row->addWidget(m_add);
    row->addWidget(m_remove);
    row->addStretch();
    layout->addLayout(row);
    layout->addWidget(m_contrast);
    connect(m_add, &QToolButton::clicked, this, [this] {
        addButton(m_buttons.isEmpty() ? palette().color(QPalette::Highlight) : m_buttons.constFirst()->color());
        refresh();
        Q_EMIT colorsChanged();
    });
    connect(m_remove, &QToolButton::clicked, this, [this] {
        if (m_buttons.size() > 1) {
            delete m_buttons.takeLast();
            refresh();
            Q_EMIT colorsChanged();
        }
    });
    addButton(palette().color(QPalette::Highlight));
    refresh();
}

QStringList ColorListEditor::colors() const
{
    QStringList names;
    for (const KColorButton *button : m_buttons) {
        names.append(QColor(button->color().rgb()).name(QColor::HexRgb));
    }
    return names;
}

void ColorListEditor::setColors(const QStringList &colors)
{
    const QStringList previous = this->colors();
    QList<QColor> valid;
    for (const QString &name : colors) {
        if (const QColor color = QColor::fromString(name); color.isValid() && valid.size() < MaximumColors) {
            valid.append(color);
        }
    }
    if (valid.isEmpty()) {
        valid.append(palette().color(QPalette::Highlight));
    }
    qDeleteAll(m_buttons);
    m_buttons.clear();
    for (const QColor &color : std::as_const(valid)) {
        addButton(color);
    }
    refresh();
    if (previous != this->colors()) {
        Q_EMIT colorsChanged();
    }
}

void ColorListEditor::setBackground(const QColor &background)
{
    m_background = background;
    refresh();
}

void ColorListEditor::addButton(const QColor &color)
{
    auto *button = new KColorButton(color, this);
    button->setAccessibleName(i18nc("@label colour by nesting depth", "Depth %1 colour", m_buttons.size() + 1));
    m_row->addWidget(button);
    m_buttons.append(button);
    connect(button, &KColorButton::changed, this, [this] {
        refresh();
        Q_EMIT colorsChanged();
    });
}

void ColorListEditor::refresh()
{
    m_add->setEnabled(m_buttons.size() < MaximumColors);
    m_remove->setEnabled(m_buttons.size() > 1);
    QStringList adjusted;
    for (int i = 0; i < m_buttons.size(); ++i) {
        if (ColorSchemes::contrastRatio(m_buttons.at(i)->color(), m_background) < 3.0) {
            adjusted.append(QString::number(i + 1));
        }
    }
    if (adjusted.isEmpty()) {
        m_contrast->hide();
        return;
    }
    m_contrast->setText(i18ncp("@info %2 is a list of depths",
                               "The colour for depth %2 is darkened or lightened to stay visible against the view background.",
                               "The colours for depths %2 are darkened or lightened to stay visible against the view background.",
                               adjusted.size(),
                               QLocale().createSeparatedList(adjusted)));
    m_contrast->show();
}

RuleListEditor::RuleListEditor(Kind kind, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
    , m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(i18nc("@action:button", "Add..."), this);
    auto *edit = new QPushButton(i18nc("@action:button", "Edit..."), this);
    auto *remove = new QPushButton(i18nc("@action:button", "Remove"), this);
    buttons->addWidget(add);
    buttons->addWidget(edit);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this] {
        editRule(-1);
    });
    connect(edit, &QPushButton::clicked, this, [this] {
        if (m_list->currentRow() >= 0) {
            editRule(m_list->currentRow());
        }
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        editRule(m_list->row(item));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0) {
            delete m_list->takeItem(row);
            Q_EMIT rulesChanged();
        }
    });
}

QStringList RuleListEditor::rules() const
{
    QStringList result;
    for (int row = 0; row < m_list->count(); ++row) {
        result.append(m_list->item(row)->data(Qt::UserRole).toString());
    }
    return result;
}

void RuleListEditor::setRules(const QStringList &rules)
{
    m_list->clear();
    const QStringList sanitized = sanitizedRules(m_kind, rules);
    for (const QString &rule : sanitized) {
        auto *item = new QListWidgetItem(rule, m_list);
        item->setData(Qt::UserRole, rule);
        if (m_kind == Kind::Color) {
            item->setForeground(QColor(rule.sliced(rule.indexOf(u'=') + 1)));
        }
    }
    Q_EMIT rulesChanged();
}

void RuleListEditor::editRule(int row)
{
    if (row < -1 || row >= m_list->count()) {
        return;
    }
    QStringList current = rules();
    const QString old = row >= 0 ? current.takeAt(row) : QString();
    bool accepted = false;
    const QString label = m_kind == Kind::Color ? i18nc("@label:textbox", "File extension:") : i18nc("@label:textbox", "File name or glob pattern:");
    const QString initial = m_kind == Kind::Color ? old.section(u'=', 0, 0) : old;
    QString value = QInputDialog::getText(this, i18nc("@title:window", "Edit Rule"), label, QLineEdit::Normal, initial, &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (m_kind == Kind::Color) {
        const QColor previous = row < 0 ? QColor(Qt::black) : QColor(old.section(u'=', 1));
        const QColor color = QColorDialog::getColor(previous, this, i18nc("@title:window", "File Colour"));
        if (!color.isValid()) {
            return;
        }
        value += u'=' + color.name(QColor::HexRgb);
    }
    current.append(value);
    const QStringList sanitized = sanitizedRules(m_kind, current);
    if (sanitized.size() != current.size()) {
        QMessageBox::warning(this, i18nc("@title:window", "Invalid Rule"), i18nc("@info", "The rule is invalid, duplicated, or the list is full."));
        return;
    }
    setRules(sanitized);
}

namespace
{
QString ruleText(const FolderColorRules::Rule &rule)
{
    QString text;
    switch (rule.match) {
    case FolderColorRules::Match::Path:
        text = i18nc("@item folder colour rule", "Folder %1", rule.pattern);
        break;
    case FolderColorRules::Match::Name:
        text = i18nc("@item folder colour rule", "Name matches %1", rule.pattern);
        break;
    case FolderColorRules::Match::Tag:
        text = i18nc("@item folder colour rule", "Tagged %1", rule.pattern);
        break;
    }
    return rule.subfolders ? i18nc("@item folder colour rule that subfolders inherit", "%1, and subfolders", text) : text;
}

QIcon colorSwatch(const QColor &color)
{
    QPixmap swatch(16, 16);
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(127, 127, 127), 1));
    painter.setBrush(color);
    painter.drawEllipse(QRectF(1.5, 1.5, 13, 13));
    return QIcon(swatch);
}
}

FolderColorRuleDialog::FolderColorRuleDialog(QWidget *parent)
    : QDialog(parent)
    , m_match(new QComboBox(this))
    , m_pattern(new QLineEdit(this))
    , m_browse(new QPushButton(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:button", "Browse…"), this))
    , m_color(new KColorButton(QColor(0x3d, 0xae, 0xe9), this))
    , m_subfolders(new QCheckBox(i18nc("@option:check", "Also colour subfolders"), this))
{
    setWindowTitle(i18nc("@title:window", "Folder Colour Rule"));
    m_match->addItem(i18nc("@item:inlistbox folder colour rule", "Folder path"), int(FolderColorRules::Match::Path));
    m_match->addItem(i18nc("@item:inlistbox folder colour rule", "Folder name pattern"), int(FolderColorRules::Match::Name));
    m_match->addItem(i18nc("@item:inlistbox folder colour rule", "Folder tag"), int(FolderColorRules::Match::Tag));
    m_match->setCurrentIndex(1);
    m_color->setAccessibleName(i18nc("@label", "Rule colour"));
    auto *form = new QFormLayout(this);
    form->addRow(i18nc("@label:listbox", "Match:"), m_match);
    auto *patternRow = new QHBoxLayout;
    patternRow->addWidget(m_pattern, 1);
    patternRow->addWidget(m_browse);
    form->addRow(i18nc("@label:textbox", "Pattern:"), patternRow);
    form->addRow(i18nc("@label", "Colour:"), m_color);
    form->addRow(QString(), m_subfolders);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_match, &QComboBox::currentIndexChanged, this, &FolderColorRuleDialog::validate);
    connect(m_pattern, &QLineEdit::textChanged, this, &FolderColorRuleDialog::validate);
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString start = m_pattern->text().startsWith(u'/') ? m_pattern->text() : QDir::homePath();
        const QString folder = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose Folder"), start);
        if (!folder.isEmpty()) {
            m_pattern->setText(folder);
        }
    });
    validate();
}

std::optional<FolderColorRules::Rule> FolderColorRuleDialog::rule() const
{
    FolderColorRules::Rule rule;
    rule.match = static_cast<FolderColorRules::Match>(m_match->currentData().toInt());
    rule.pattern = m_pattern->text();
    rule.color = m_color->color();
    rule.subfolders = m_subfolders->isChecked();
    return FolderColorRules::parse(FolderColorRules::entry(rule));
}

void FolderColorRuleDialog::setRule(const FolderColorRules::Rule &rule)
{
    m_match->setCurrentIndex(std::max(0, m_match->findData(int(rule.match))));
    m_pattern->setText(rule.pattern);
    m_color->setColor(rule.color);
    m_subfolders->setChecked(rule.subfolders);
    validate();
}

void FolderColorRuleDialog::validate()
{
    const auto match = static_cast<FolderColorRules::Match>(m_match->currentData().toInt());
    m_browse->setVisible(match == FolderColorRules::Match::Path);
    switch (match) {
    case FolderColorRules::Match::Path:
        m_pattern->setPlaceholderText(i18nc("@info:placeholder", "/path/to/folder or ~/folder"));
        break;
    case FolderColorRules::Match::Name:
        m_pattern->setPlaceholderText(i18nc("@info:placeholder", "Name or wildcard pattern, such as *-archive"));
        break;
    case FolderColorRules::Match::Tag:
        m_pattern->setPlaceholderText(i18nc("@info:placeholder", "Tag name"));
        break;
    }
    m_ok->setEnabled(rule().has_value());
}

FolderColorRuleEditor::FolderColorRuleEditor(QWidget *parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
    , m_add(new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add…"), this))
    , m_edit(new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "Edit…"), this))
    , m_remove(new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove"), this))
    , m_up(new QPushButton(QIcon::fromTheme(QStringLiteral("go-up")), i18nc("@action:button", "Move Up"), this))
    , m_down(new QPushButton(QIcon::fromTheme(QStringLiteral("go-down")), i18nc("@action:button", "Move Down"), this))
{
    m_list->setAccessibleName(i18nc("@label", "Folder colour rules"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);
    auto *buttons = new QHBoxLayout;
    for (QPushButton *button : {m_add, m_edit, m_remove, m_up, m_down}) {
        buttons->addWidget(button);
    }
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(m_add, &QPushButton::clicked, this, [this] {
        editRule(-1);
    });
    connect(m_edit, &QPushButton::clicked, this, [this] {
        editRule(m_list->currentRow());
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        editRule(m_list->row(item));
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        if (const int row = m_list->currentRow(); row >= 0) {
            delete m_list->takeItem(row);
            updateButtons();
            Q_EMIT rulesChanged();
        }
    });
    connect(m_up, &QPushButton::clicked, this, [this] {
        moveRule(m_list->currentRow(), -1);
    });
    connect(m_down, &QPushButton::clicked, this, [this] {
        moveRule(m_list->currentRow(), 1);
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &FolderColorRuleEditor::updateButtons);
    updateButtons();
}

QStringList FolderColorRuleEditor::rules() const
{
    QStringList result;
    for (int row = 0; row < m_list->count(); ++row) {
        result.append(m_list->item(row)->data(Qt::UserRole).toString());
    }
    return result;
}

void FolderColorRuleEditor::setRules(const QStringList &rules)
{
    const int current = m_list->currentRow();
    m_list->clear();
    const QList<FolderColorRules::Rule> parsed = FolderColorRules(rules).rules();
    for (const FolderColorRules::Rule &rule : parsed) {
        auto *item = new QListWidgetItem(colorSwatch(rule.color), ruleText(rule), m_list);
        item->setData(Qt::UserRole, FolderColorRules::entry(rule));
    }
    m_list->setCurrentRow(std::min(current, int(m_list->count()) - 1));
    updateButtons();
    Q_EMIT rulesChanged();
}

void FolderColorRuleEditor::moveRule(int row, int offset)
{
    const int target = row + offset;
    if (row < 0 || row >= m_list->count() || target < 0 || target >= m_list->count()) {
        return;
    }
    QStringList current = rules();
    current.move(row, target);
    setRules(current);
    m_list->setCurrentRow(target);
}

void FolderColorRuleEditor::editRule(int row)
{
    if (row < -1 || row >= m_list->count()) {
        return;
    }
    QStringList current = rules();
    if (row < 0 && current.size() >= FolderColorRules::MaximumRules) {
        QMessageBox::warning(this, i18nc("@title:window", "Invalid Rule"), i18nc("@info", "The list of folder colour rules is full."));
        return;
    }
    FolderColorRuleDialog dialog(this);
    if (row >= 0) {
        if (const std::optional<FolderColorRules::Rule> rule = FolderColorRules::parse(current.at(row))) {
            dialog.setRule(*rule);
        }
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const std::optional<FolderColorRules::Rule> rule = dialog.rule();
    if (!rule) {
        return;
    }
    const QString entry = FolderColorRules::entry(*rule);
    if (current.contains(entry) && (row < 0 || current.at(row) != entry)) {
        QMessageBox::warning(this, i18nc("@title:window", "Invalid Rule"), i18nc("@info", "The same rule is already in the list."));
        return;
    }
    if (row >= 0) {
        current[row] = entry;
    } else {
        current.append(entry);
    }
    setRules(current);
    m_list->setCurrentRow(row >= 0 ? row : m_list->count() - 1);
}

void FolderColorRuleEditor::updateButtons()
{
    const int row = m_list->currentRow();
    m_edit->setEnabled(row >= 0);
    m_remove->setEnabled(row >= 0);
    m_up->setEnabled(row > 0);
    m_down->setEnabled(row >= 0 && row + 1 < m_list->count());
}

IconInformationList::IconInformationList(QWidget *parent, bool cardFields)
    : QListWidget(parent)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    QList<QPair<QString, QString>> options{
        {QStringLiteral("size"), i18nc("@option:check", "Size")},
        {QStringLiteral("permissions"), i18nc("@option:check", "Permissions")},
        {QStringLiteral("octalPermissions"), i18nc("@option:check", "Permissions (octal)")},
        {QStringLiteral("owner"), i18nc("@option:check", "Owner")},
        {QStringLiteral("ownerAndGroup"), i18nc("@option:check", "Owner and group")},
        {QStringLiteral("creationTime"), i18nc("@option:check", "Created")},
        {QStringLiteral("modificationTime"), i18nc("@option:check", "Modified")},
        {QStringLiteral("accessTime"), i18nc("@option:check", "Accessed")},
        {QStringLiteral("mimeType"), i18nc("@option:check", "MIME type")},
        {QStringLiteral("friendlyMimeType"), i18nc("@option:check", "File type")},
        {QStringLiteral("linkDest"), i18nc("@option:check", "Link target")},
        {QStringLiteral("localPathOrUrl"), i18nc("@option:check", "Path or URL")},
        {QStringLiteral("comment"), i18nc("@option:check", "Comment")},
    };
    if (cardFields) {
        options.append({QStringLiteral("tags"), i18nc("@option:check", "Tags")});
        options.append({QStringLiteral("rating"), i18nc("@option:check", "Rating")});
        options.append({QStringLiteral("label"), i18nc("@option:check", "Colour label")});
    }
    for (const auto &[key, label] : options) {
        auto *item = new QListWidgetItem(label, this);
        item->setData(Qt::UserRole, key);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setCheckState(Qt::Unchecked);
    }
    connect(this, &QListWidget::itemChanged, this, &IconInformationList::informationChanged);
    connect(model(), &QAbstractItemModel::rowsMoved, this, &IconInformationList::informationChanged);
}

QStringList IconInformationList::information() const
{
    QStringList result;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Checked) {
            result.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return result;
}

void IconInformationList::setInformation(const QStringList &information)
{
    const QSignalBlocker blocker(this);
    QList<QListWidgetItem *> ordered;
    QSet<QListWidgetItem *> seen;
    for (const QString &field : information) {
        for (int row = 0; row < count(); ++row) {
            QListWidgetItem *candidate = item(row);
            if (candidate->data(Qt::UserRole).toString() == field && !seen.contains(candidate)) {
                ordered.append(candidate);
                seen.insert(candidate);
                break;
            }
        }
    }
    for (int row = 0; row < count(); ++row) {
        QListWidgetItem *candidate = item(row);
        if (!seen.contains(candidate)) {
            ordered.append(candidate);
        }
    }
    while (count() > 0) {
        takeItem(0);
    }
    for (int row = 0; row < ordered.size(); ++row) {
        QListWidgetItem *candidate = ordered.at(row);
        candidate->setCheckState(information.contains(candidate->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
        insertItem(row, candidate);
    }
}

HiddenItemsList::HiddenItemsList(QWidget *parent)
    : QListWidget(parent)
{
    connect(this, &QListWidget::itemChanged, this, &HiddenItemsList::hiddenItemsChanged);
}

void HiddenItemsList::addItem(const QString &id, const QString &label)
{
    auto *entry = new QListWidgetItem(label, this);
    entry->setData(Qt::UserRole, id);
    entry->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    entry->setCheckState(Qt::Checked);
}

QStringList HiddenItemsList::hiddenItems() const
{
    QStringList hidden = m_missingItems;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Unchecked) {
            hidden.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return hidden;
}

void HiddenItemsList::setHiddenItems(const QStringList &hidden)
{
    const QSignalBlocker blocker(this);
    m_missingItems = hidden;
    for (int row = 0; row < count(); ++row) {
        const QString id = item(row)->data(Qt::UserRole).toString();
        item(row)->setCheckState(hidden.contains(id) ? Qt::Unchecked : Qt::Checked);
        m_missingItems.removeAll(id);
    }
}

PreviewPluginList::PreviewPluginList(QWidget *parent)
    : QListWidget(parent)
{
    QList<KPluginMetaData> available = KIO::PreviewJob::availableThumbnailerPlugins();
    std::sort(available.begin(), available.end(), [](const KPluginMetaData &a, const KPluginMetaData &b) {
        return QString::localeAwareCompare(a.name(), b.name()) < 0;
    });
    for (const KPluginMetaData &plugin : std::as_const(available)) {
        auto *entry = new QListWidgetItem(plugin.name(), this);
        entry->setData(Qt::UserRole, plugin.pluginId());
        entry->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        entry->setCheckState(Qt::Unchecked);
    }
    connect(this, &QListWidget::itemChanged, this, &PreviewPluginList::pluginsChanged);
}

QStringList PreviewPluginList::plugins() const
{
    QStringList ids;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Checked) {
            ids.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return ids;
}

void PreviewPluginList::setPlugins(const QStringList &plugins)
{
    for (int row = 0; row < count(); ++row) {
        const bool enabled = plugins.contains(item(row)->data(Qt::UserRole).toString());
        item(row)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
}

PreviewSizeSpinBox::PreviewSizeSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    setRange(0, MaximumMiB);
    setSuffix(i18nc("@item:valuesuffix mebibytes", " MiB"));
    setSpecialValueText(i18nc("@item:inlistbox preview size limit", "No limit"));
    connect(this, &QSpinBox::valueChanged, this, &PreviewSizeSpinBox::bytesChanged);
}

qulonglong PreviewSizeSpinBox::bytes() const
{
    return value() == 0 ? NoLimit : qulonglong(value()) * MiB;
}

void PreviewSizeSpinBox::setBytes(qulonglong bytes)
{
    if (bytes == NoLimit) {
        setValue(0);
        return;
    }
    const qulonglong mib = std::max<qulonglong>(1, (bytes + MiB - 1) / MiB);
    setValue(int(std::min<qulonglong>(mib, MaximumMiB)));
}
