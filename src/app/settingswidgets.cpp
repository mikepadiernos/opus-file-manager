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
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr qulonglong MiB = 1024 * 1024;
constexpr qulonglong NoLimit = std::numeric_limits<qulonglong>::max();
constexpr int MaximumMiB = 100000;

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
    m_base = ColorSchemes::palette(id);
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
    const QList<ColorSchemes::ContrastIssue> issues = ColorSchemes::contrastIssues(palette, m_overrides.keys());
    if (issues.isEmpty()) {
        m_contrast->hide();
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
    m_contrast->setText(i18nc("@info", "Some colours may be hard to read:").toHtmlEscaped() + QStringLiteral("<ul><li>")
                        + lines.join(QStringLiteral("</li><li>")) + QStringLiteral("</li></ul>"));
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

IconInformationList::IconInformationList(QWidget *parent)
    : QListWidget(parent)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    const QList<QPair<QString, QString>> options{
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
