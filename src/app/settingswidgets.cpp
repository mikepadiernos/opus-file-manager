// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingswidgets.h"

#include "appfonts.h"
#include "colorschemes.h"
#include "customstylesheet.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "settings.h"
#include "stylesheets.h"
#include "workspacestartup.h"

#include <KColorButton>
#include <KFontRequester>
#include <KIO/PreviewJob>
#include <KLanguageName>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
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

KMessageWidget *createContrastNotice(const QString &objectName, bool adjusted, QWidget *parent)
{
    auto *notice = new KMessageWidget(parent);
    notice->setObjectName(objectName);
    notice->setMessageType(adjusted ? KMessageWidget::Information : KMessageWidget::Warning);
    notice->setCloseButtonVisible(false);
    notice->setWordWrap(true);
    notice->hide();
    return notice;
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
    if (kind == Kind::Color) {
        m_contrast = createContrastNotice(QStringLiteral("fileColourContrastWarning"), false, this);
        layout->addWidget(m_contrast);
        connect(this, &RuleListEditor::rulesChanged, this, &RuleListEditor::updateContrast);
    }
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

void RuleListEditor::updateContrast()
{
    if (!m_contrast) {
        return;
    }
    // File colours are used as they are, so only warn about them.
    const QColor background = palette().color(QPalette::Base);
    QStringList unreadable;
    for (const QString &rule : rules()) {
        if (ColorSchemes::contrastRatio(QColor(rule.section(u'=', 1)), background) < 4.5) {
            unreadable.append(rule.section(u'=', 0, 0));
        }
    }
    if (unreadable.isEmpty()) {
        m_contrast->hide();
        return;
    }
    m_contrast->setText(i18ncp("@info %2 is a list of file extensions",
                               "Names of %2 files may be hard to read: their colour has less than 4.5:1 contrast against the view background.",
                               "Names of %2 files may be hard to read: their colours have less than 4.5:1 contrast against the view background.",
                               unreadable.size(),
                               QLocale().createSeparatedList(unreadable)));
    m_contrast->show();
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
    , m_contrast(createContrastNotice(QStringLiteral("folderColourContrastNotice"), true, this))
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
    form->addRow(QString(), m_contrast);
    form->addRow(QString(), m_subfolders);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_match, &QComboBox::currentIndexChanged, this, &FolderColorRuleDialog::validate);
    connect(m_pattern, &QLineEdit::textChanged, this, &FolderColorRuleDialog::validate);
    connect(m_color, &KColorButton::changed, this, &FolderColorRuleDialog::updateContrast);
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString start = m_pattern->text().startsWith(u'/') ? m_pattern->text() : QDir::homePath();
        const QString folder = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose Folder"), start);
        if (!folder.isEmpty()) {
            m_pattern->setText(folder);
        }
    });
    validate();
    updateContrast();
}

void FolderColorRuleDialog::updateContrast()
{
    const double ratio = ColorSchemes::contrastRatio(m_color->color(), palette().color(QPalette::Base));
    if (ratio >= 4.5) {
        m_contrast->hide();
        return;
    }
    m_contrast->setText(ratio < 3.0 ? i18nc("@info",
                                            "Folder icons and badges in this colour may be hard to see against the view background. Coloured "
                                            "names are darkened or lightened to stay readable.")
                                    : i18nc("@info", "Coloured names in this colour are darkened or lightened to stay readable against the view background."));
    m_contrast->setMessageType(ratio < 3.0 ? KMessageWidget::Warning : KMessageWidget::Information);
    m_contrast->show();
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
    updateContrast();
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

FontOverrideChooser::FontOverrideChooser(const QString &accessibleName, QWidget *parent)
    : QWidget(parent)
    , m_custom(new QCheckBox(i18nc("@option:check use a custom font", "Custom:"), this))
    , m_requester(new KFontRequester(this))
{
    m_custom->setToolTip(i18nc("@info:tooltip", "When unchecked, Plasma's General font is used."));
    m_custom->setAccessibleName(i18nc("@option:check %1 is an area such as File list font", "Custom %1", accessibleName));
    m_requester->setAccessibleName(accessibleName);
    m_requester->setFont(QApplication::font());
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_custom);
    layout->addWidget(m_requester, 1);
    m_requester->setEnabled(false);
    connect(m_custom, &QCheckBox::toggled, this, [this](bool custom) {
        m_requester->setEnabled(custom);
        Q_EMIT fontSettingChanged();
    });
    connect(m_requester, &KFontRequester::fontSelected, this, &FontOverrideChooser::fontSettingChanged);
}

QString FontOverrideChooser::fontSetting() const
{
    if (!m_custom->isChecked()) {
        return {};
    }
    // A custom font is stored in full, even when it matches Plasma's font.
    QFont font = m_requester->font();
    font.setResolveMask(QFont::AllPropertiesResolved);
    return AppFonts::toSetting(font);
}

void FontOverrideChooser::setFontSetting(const QString &setting)
{
    const QFont font = AppFonts::fromSetting(setting);
    const bool custom = AppFonts::isOverride(font);
    {
        const QSignalBlocker blocker(m_custom);
        m_custom->setChecked(custom);
    }
    m_requester->setEnabled(custom);
    m_requester->setFont(custom ? font : QApplication::font());
    Q_EMIT fontSettingChanged();
}

StyleSheetEditor::StyleSheetEditor(QWidget *parent)
    : QWidget(parent)
    , m_text(new QPlainTextEdit(this))
    , m_status(new KMessageWidget(this))
    , m_preview(new QFrame(this))
    , m_checkTimer(new QTimer(this))
{
    setProperty("kcfg_propertyNotify", QByteArray(SIGNAL(sheetChanged())));
    m_text->setObjectName(QStringLiteral("styleSheetText"));
    m_text->setAccessibleName(i18nc("@label", "Custom stylesheet"));
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setTabChangesFocus(true);
    m_text->setPlaceholderText(QStringLiteral("QStatusBar {\n    border-top: 1px solid palette(mid);\n}"));
    m_status->setObjectName(QStringLiteral("styleSheetStatus"));
    m_status->setWordWrap(true);
    m_status->setCloseButtonVisible(false);
    m_status->hide();
    m_checkTimer->setSingleShot(true);
    m_checkTimer->setInterval(300);

    auto *buttons = new QHBoxLayout;
    auto *import = new QPushButton(QIcon::fromTheme(QStringLiteral("document-import")), i18nc("@action:button", "Import…"), this);
    auto *exportButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-export")), i18nc("@action:button", "Export…"), this);
    auto *revertButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-revert")), i18nc("@action:button", "Revert"), this);
    import->setObjectName(QStringLiteral("importStyleSheet"));
    exportButton->setObjectName(QStringLiteral("exportStyleSheet"));
    revertButton->setObjectName(QStringLiteral("revertStyleSheet"));
    revertButton->setToolTip(i18nc("@info:tooltip", "Returns to the stylesheet that was last applied."));
    buttons->addWidget(import);
    buttons->addWidget(exportButton);
    buttons->addWidget(revertButton);
    buttons->addStretch();

    // Sample widgets show the stylesheet without applying it to any window.
    m_preview->setObjectName(QStringLiteral("styleSheetPreview"));
    m_preview->setAccessibleName(i18nc("@label", "Stylesheet preview"));
    m_preview->setFrameShape(QFrame::StyledPanel);
    auto *sample = new QGridLayout(m_preview);
    auto *tabs = new QTabBar(m_preview);
    tabs->addTab(i18nc("@title:tab sample tab", "Documents"));
    tabs->addTab(i18nc("@title:tab sample tab", "Pictures"));
    tabs->setFocusPolicy(Qt::NoFocus);
    auto *files = new QListWidget(m_preview);
    files->addItems({i18nc("@item sample file", "Report.odt"), i18nc("@item sample file", "Holiday.jpg"), i18nc("@item sample file", "Notes.txt")});
    files->setCurrentRow(1);
    files->setMaximumHeight(files->sizeHintForRow(0) * 4);
    files->setFocusPolicy(Qt::NoFocus);
    auto *filter = new QLineEdit(m_preview);
    filter->setPlaceholderText(i18nc("@info:placeholder sample", "Filter…"));
    filter->setFocusPolicy(Qt::NoFocus);
    auto *button = new QPushButton(i18nc("@action:button sample", "Open"), m_preview);
    button->setFocusPolicy(Qt::NoFocus);
    auto *hidden = new QCheckBox(i18nc("@option:check sample", "Show hidden files"), m_preview);
    hidden->setFocusPolicy(Qt::NoFocus);
    auto *status = new QStatusBar(m_preview);
    status->showMessage(i18nc("@info:status sample", "3 files"));
    status->setSizeGripEnabled(false);
    sample->addWidget(tabs, 0, 0, 1, 2);
    sample->addWidget(files, 1, 0, 1, 2);
    sample->addWidget(filter, 2, 0);
    sample->addWidget(button, 2, 1);
    sample->addWidget(hidden, 3, 0, 1, 2);
    sample->addWidget(status, 4, 0, 1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *editors = new QHBoxLayout;
    editors->addWidget(m_text, 3);
    auto *previewColumn = new QVBoxLayout;
    previewColumn->addWidget(new QLabel(i18nc("@title:group", "Preview:"), this));
    previewColumn->addWidget(m_preview);
    previewColumn->addStretch();
    editors->addLayout(previewColumn, 2);
    layout->addLayout(editors, 1);
    layout->addWidget(m_status);
    layout->addLayout(buttons);

    connect(m_text, &QPlainTextEdit::textChanged, this, [this] {
        m_checkTimer->start();
        Q_EMIT sheetChanged();
    });
    connect(m_checkTimer, &QTimer::timeout, this, &StyleSheetEditor::check);
    connect(import, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this,
                                                          i18nc("@title:window", "Import Stylesheet"),
                                                          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                          i18nc("@item:inlistbox", "Qt Style Sheets (*.qss *.css)"));
        if (!path.isEmpty()) {
            importFile(path);
        }
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        QString path =
            QFileDialog::getSaveFileName(this,
                                         i18nc("@title:window", "Export Stylesheet"),
                                         QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("plasma-opus.qss")),
                                         i18nc("@item:inlistbox", "Qt Style Sheets (*.qss)"));
        if (path.isEmpty()) {
            return;
        }
        if (!path.endsWith(QLatin1String(".qss"), Qt::CaseInsensitive) && !path.endsWith(QLatin1String(".css"), Qt::CaseInsensitive)) {
            path.append(QStringLiteral(".qss"));
        }
        exportFile(path);
    });
    connect(revertButton, &QPushButton::clicked, this, &StyleSheetEditor::revert);
}

QString StyleSheetEditor::sheet() const
{
    return m_text->toPlainText();
}

void StyleSheetEditor::setSheet(const QString &sheet)
{
    if (sheet == this->sheet()) {
        return;
    }
    m_text->setPlainText(sheet);
    check();
}

QString StyleSheetEditor::problem() const
{
    return m_problem;
}

void StyleSheetEditor::check()
{
    m_checkTimer->stop();
    const QString text = sheet();
    m_problem = CustomStyleSheet::error(text);
    if (!m_problem.isEmpty()) {
        m_preview->setStyleSheet(QString());
        showStatus(KMessageWidget::Error, m_problem);
    } else {
        m_preview->setStyleSheet(text);
        if (text.trimmed().isEmpty()) {
            m_status->hide();
        } else {
            showStatus(KMessageWidget::Positive, i18nc("@info", "No problems found. The preview shows the stylesheet."));
        }
    }
}

bool StyleSheetEditor::importFile(const QString &path)
{
    QString error;
    const std::optional<QString> imported = StyleSheets::readFile(path, &error);
    if (!imported) {
        showStatus(KMessageWidget::Error, i18nc("@info %1 is the reason", "The stylesheet was not imported. %1", error));
        return false;
    }
    m_text->setPlainText(*imported);
    check();
    return true;
}

bool StyleSheetEditor::exportFile(const QString &path)
{
    if (const QString error = StyleSheets::writeFile(path, sheet()); !error.isEmpty()) {
        showStatus(KMessageWidget::Error, error);
        return false;
    }
    showStatus(KMessageWidget::Positive, i18nc("@info", "Exported the stylesheet to %1.", QDir::toNativeSeparators(path)));
    return true;
}

void StyleSheetEditor::revert()
{
    m_text->setPlainText(Settings::customStyleSheet());
    check();
}

void StyleSheetEditor::showStatus(int type, const QString &text)
{
    m_status->setMessageType(static_cast<KMessageWidget::MessageType>(type));
    m_status->setText(text);
    m_status->show();
}

ThemeProfileManager::ThemeProfileManager(Callbacks callbacks, QWidget *parent)
    : QWidget(parent)
    , m_callbacks(std::move(callbacks))
    , m_list(new QListWidget(this))
    , m_message(new KMessageWidget(this))
    , m_load(new QPushButton(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:button", "Load"), this))
    , m_export(new QPushButton(QIcon::fromTheme(QStringLiteral("document-export")), i18nc("@action:button", "Export…"), this))
    , m_remove(new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove"), this))
{
    m_confirm = [this](const QString &question) {
        return QMessageBox::question(this, i18nc("@title:window", "Theme Profiles"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes;
    };
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18nc("@info",
                                   "A theme profile stores every Themes option except folder colour rules and the high contrast safeguard. "
                                   "Loading a profile shows it in this dialog: Apply or OK uses it, and Cancel discards it. Profiles hold "
                                   "only setting values, so importing one never runs code."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list->setObjectName(QStringLiteral("themeProfiles"));
    m_list->setAccessibleName(i18nc("@label", "Theme profiles"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), i18nc("@action:button", "Save Current…"), this);
    auto *import = new QPushButton(QIcon::fromTheme(QStringLiteral("document-import")), i18nc("@action:button", "Import…"), this);
    save->setObjectName(QStringLiteral("saveThemeProfile"));
    save->setToolTip(i18nc("@info:tooltip", "Saves the options shown in this dialog, including changes that are not applied yet."));
    m_load->setObjectName(QStringLiteral("loadThemeProfile"));
    import->setObjectName(QStringLiteral("importThemeProfile"));
    m_export->setObjectName(QStringLiteral("exportThemeProfile"));
    m_remove->setObjectName(QStringLiteral("removeThemeProfile"));
    for (QPushButton *button : {save, m_load, import, m_export, m_remove}) {
        buttons->addWidget(button);
    }
    buttons->addStretch();
    layout->addLayout(buttons);

    m_message->setObjectName(QStringLiteral("themeProfileMessage"));
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(true);
    m_message->hide();
    layout->addWidget(m_message);

    auto *resetRow = new QHBoxLayout;
    auto *reset = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-reset")), i18nc("@action:button", "Reset to Plasma Defaults"), this);
    reset->setObjectName(QStringLiteral("resetThemeProfileAppearance"));
    reset->setToolTip(i18nc("@info:tooltip", "Shows Plasma's defaults for every Themes option except folder colour rules. Saved profiles are kept."));
    resetRow->addWidget(reset);
    resetRow->addStretch();
    layout->addLayout(resetRow);

    connect(save, &QPushButton::clicked, this, [this] {
        const QListWidgetItem *selected = m_list->currentItem();
        bool accepted = false;
        const QString name = QInputDialog::getText(this,
                                                   i18nc("@title:window", "Save Theme Profile"),
                                                   i18nc("@label:textbox", "Profile name:"),
                                                   QLineEdit::Normal,
                                                   selected ? selected->text() : QString(),
                                                   &accepted);
        if (accepted) {
            saveCurrent(name);
        }
    });
    connect(m_load, &QPushButton::clicked, this, [this] {
        loadProfile(selectedId());
    });
    connect(m_list, &QListWidget::itemActivated, this, [this] {
        loadProfile(selectedId());
    });
    connect(import, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this,
                                                          i18nc("@title:window", "Import Theme Profile"),
                                                          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                          i18nc("@item:inlistbox", "Plasma Opus theme profile (*.json)"));
        if (!path.isEmpty()) {
            importProfile(path);
        }
    });
    connect(m_export, &QPushButton::clicked, this, [this] {
        const QString id = selectedId();
        const QListWidgetItem *selected = m_list->currentItem();
        if (id.isEmpty() || !selected) {
            return;
        }
        QString fileName = selected->text();
        fileName.replace(QRegularExpression(QStringLiteral(R"([/\\:*?"<>|]+)")), QStringLiteral("-"));
        const QString defaultPath =
            QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(fileName + QStringLiteral(".opus-theme.json"));
        QString path = QFileDialog::getSaveFileName(this,
                                                    i18nc("@title:window", "Export Theme Profile"),
                                                    defaultPath,
                                                    i18nc("@item:inlistbox", "Plasma Opus theme profile (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        if (!path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
            path.append(QStringLiteral(".opus-theme.json"));
        }
        bool includeStyleSheet = true;
        const auto profile = std::find_if(m_profiles.cbegin(), m_profiles.cend(), [&id](const ThemeProfiles::Profile &candidate) {
            return candidate.id == id;
        });
        if (profile != m_profiles.cend() && ThemeProfiles::hasStyleSheet(profile->values)) {
            const auto answer = QMessageBox::question(this,
                                                      i18nc("@title:window", "Export Theme Profile"),
                                                      i18nc("@info", "Include the profile's custom stylesheet in the exported file?"),
                                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                                      QMessageBox::Yes);
            if (answer == QMessageBox::Cancel) {
                return;
            }
            includeStyleSheet = answer == QMessageBox::Yes;
        }
        exportProfile(id, path, includeStyleSheet);
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        removeProfile(selectedId());
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_callbacks.reset();
        showMessage(KMessageWidget::Information,
                    i18nc("@info", "Plasma's defaults are shown in this dialog. Apply or OK uses them; Cancel keeps the current appearance."));
    });
    connect(m_list, &QListWidget::currentItemChanged, this, &ThemeProfileManager::updateButtons);
    reload();
}

void ThemeProfileManager::setConfirmation(std::function<bool(const QString &)> confirm)
{
    m_confirm = std::move(confirm);
}

QString ThemeProfileManager::selectedId() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ThemeProfileManager::reload(const QString &selectId)
{
    QString error;
    m_profiles = ThemeProfiles::load(Settings::self(), KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group)), &error);
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const ThemeProfiles::Profile &profile : std::as_const(m_profiles)) {
        auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")), profile.name, m_list);
        item->setData(Qt::UserRole, profile.id);
        if (profile.id == selectId) {
            m_list->setCurrentItem(item);
        }
    }
    if (!error.isEmpty()) {
        showMessage(KMessageWidget::Warning, error);
    }
    updateButtons();
}

void ThemeProfileManager::updateButtons()
{
    const bool selected = m_list->currentItem() != nullptr;
    m_load->setEnabled(selected);
    m_export->setEnabled(selected);
    m_remove->setEnabled(selected);
}

void ThemeProfileManager::showMessage(int type, const QString &text)
{
    m_message->setMessageType(static_cast<KMessageWidget::MessageType>(type));
    m_message->setText(text);
    m_message->animatedShow();
}

QString ThemeProfileManager::unavailableText(const QVariantMap &values) const
{
    const QStringList missing = ThemeProfiles::unavailable(values);
    if (missing.isEmpty()) {
        return {};
    }
    return i18nc("@info %1 is a list of colour schemes and fonts",
                 "Not installed: %1. Plasma's own is used instead, and the profile keeps them, so they apply once they are installed.",
                 QLocale().createSeparatedList(missing));
}

bool ThemeProfileManager::store(const QString &name, const QVariantMap &values, QString *id)
{
    if (const QString error = ThemeProfiles::nameError(name); !error.isEmpty()) {
        showMessage(KMessageWidget::Error, error);
        return false;
    }
    QList<ThemeProfiles::Profile> profiles = m_profiles;
    const qsizetype existing = ThemeProfiles::indexOfName(profiles, name);
    if (existing >= 0) {
        if (!m_confirm(i18nc("@info", "Replace the theme profile “%1”?", profiles.at(existing).name))) {
            return false;
        }
        profiles[existing].name = name.trimmed();
        profiles[existing].values = values;
    } else {
        if (profiles.size() >= ThemeProfiles::MaximumProfiles) {
            showMessage(KMessageWidget::Error,
                        i18ncp("@info",
                               "You can save at most %1 theme profile. Remove one first.",
                               "You can save at most %1 theme profiles. Remove one first.",
                               ThemeProfiles::MaximumProfiles));
            return false;
        }
        profiles.append(ThemeProfiles::Profile{QString(), name.trimmed(), values});
    }
    QString error;
    if (!ThemeProfiles::save(Settings::self(), KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group)), profiles, &error)) {
        showMessage(KMessageWidget::Error, error);
        return false;
    }
    *id = profiles.at(existing >= 0 ? existing : profiles.size() - 1).id;
    reload(*id);
    return true;
}

bool ThemeProfileManager::saveCurrent(const QString &name)
{
    QString id;
    if (!store(name, m_callbacks.current(), &id)) {
        return false;
    }
    showMessage(KMessageWidget::Positive, i18nc("@info", "Saved the theme profile “%1”.", name.trimmed()));
    return true;
}

bool ThemeProfileManager::loadProfile(const QString &id)
{
    const auto profile = std::find_if(m_profiles.cbegin(), m_profiles.cend(), [&id](const ThemeProfiles::Profile &candidate) {
        return candidate.id == id;
    });
    if (id.isEmpty() || profile == m_profiles.cend()) {
        return false;
    }
    m_callbacks.show(profile->values);
    const QString loaded = i18nc("@info", "“%1” is shown in this dialog. Apply or OK uses it; Cancel keeps the current appearance.", profile->name);
    const QString missing = unavailableText(profile->values);
    showMessage(missing.isEmpty() ? KMessageWidget::Positive : KMessageWidget::Warning, missing.isEmpty() ? loaded : loaded + QLatin1Char(' ') + missing);
    return true;
}

bool ThemeProfileManager::importProfile(const QString &path)
{
    QString error;
    QStringList warnings;
    const auto profile = ThemeProfiles::importFile(Settings::self(), path, &error, &warnings);
    if (!profile) {
        showMessage(KMessageWidget::Error, i18nc("@info %1 is the reason", "The theme profile was not imported. %1", error));
        return false;
    }
    QString id;
    if (!store(profile->name, profile->values, &id)) {
        return false;
    }
    QStringList notes = warnings;
    if (const QString missing = unavailableText(profile->values); !missing.isEmpty()) {
        notes.append(missing);
    }
    const QString imported = i18nc("@info", "Imported the theme profile “%1”. Load it to see it in this dialog.", profile->name);
    showMessage(notes.isEmpty() ? KMessageWidget::Positive : KMessageWidget::Warning,
                notes.isEmpty() ? imported : imported + QLatin1Char(' ') + notes.join(QLatin1Char(' ')));
    return true;
}

bool ThemeProfileManager::exportProfile(const QString &id, const QString &path, bool includeStyleSheet)
{
    const auto profile = std::find_if(m_profiles.cbegin(), m_profiles.cend(), [&id](const ThemeProfiles::Profile &candidate) {
        return candidate.id == id;
    });
    if (id.isEmpty() || profile == m_profiles.cend()) {
        return false;
    }
    ThemeProfiles::Profile exported = *profile;
    if (!includeStyleSheet) {
        for (const QString &entry : ThemeProfiles::styleSheetEntries()) {
            exported.values.remove(entry);
        }
    }
    if (const QString error = ThemeProfiles::exportFile(Settings::self(), exported, path); !error.isEmpty()) {
        showMessage(KMessageWidget::Error, error);
        return false;
    }
    showMessage(KMessageWidget::Positive, i18nc("@info", "Exported “%1” to %2.", profile->name, QDir::toNativeSeparators(path)));
    return true;
}

bool ThemeProfileManager::removeProfile(const QString &id)
{
    QList<ThemeProfiles::Profile> profiles = m_profiles;
    const auto profile = std::find_if(profiles.begin(), profiles.end(), [&id](const ThemeProfiles::Profile &candidate) {
        return candidate.id == id;
    });
    if (id.isEmpty() || profile == profiles.end()) {
        return false;
    }
    const QString name = profile->name;
    if (!m_confirm(i18nc("@info", "Remove the theme profile “%1”? The current appearance is not changed.", name))) {
        return false;
    }
    profiles.erase(profile);
    QString error;
    if (!ThemeProfiles::save(Settings::self(), KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group)), profiles, &error)) {
        showMessage(KMessageWidget::Error, error);
        return false;
    }
    reload();
    showMessage(KMessageWidget::Information, i18nc("@info", "Removed the theme profile “%1”.", name));
    return true;
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
