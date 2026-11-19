// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "filecolorrules.h"
#include "ignorelist.h"
#include "viewsettings.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
const QList<QPair<QString, QString>> Columns{
    {QStringLiteral("type"), i18nc("@option:check", "Type")},
    {QStringLiteral("size"), i18nc("@option:check", "Size")},
    {QStringLiteral("date"), i18nc("@option:check", "Modified")},
    {QStringLiteral("mode"), i18nc("@option:check", "Permissions")},
    {QStringLiteral("owner"), i18nc("@option:check", "Owner")},
    {QStringLiteral("group"), i18nc("@option:check", "Group")},
    {QStringLiteral("rating"), i18nc("@option:check", "Rating")},
};
const QList<QPair<QString, QString>> IconInformation{
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
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : KPageDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Configure Opus File Manager"));
    setFaceType(KPageDialog::List);
    setMinimumSize(680, 520);

    const ViewSettings settings = ViewSettings::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("View")));
    auto *viewPage = new QWidget(this);
    auto *viewLayout = new QVBoxLayout(viewPage);

    auto *columnsGroup = new QGroupBox(i18nc("@title:group", "Default Visible Columns"), viewPage);
    auto *columnsLayout = new QGridLayout(columnsGroup);
    for (qsizetype i = 0; i < Columns.size(); ++i) {
        const auto &[key, label] = Columns.at(i);
        auto *check = new QCheckBox(label, columnsGroup);
        check->setObjectName(QStringLiteral("column_%1").arg(key));
        check->setChecked(settings.visibleColumns.contains(key));
        m_columnChecks.insert(key, check);
        columnsLayout->addWidget(check, i / 3, i % 3);
    }
    viewLayout->addWidget(columnsGroup);

    auto *generalGroup = new QGroupBox(i18nc("@title:group", "Listing Defaults"), viewPage);
    auto *generalLayout = new QFormLayout(generalGroup);
    m_groupBy = new QComboBox(generalGroup);
    m_groupBy->setObjectName(QStringLiteral("default_group_by"));
    const QList<QPair<QString, QString>> groups{
        {QStringLiteral("none"), i18nc("@item:inlistbox", "No Grouping")},
        {QStringLiteral("name"), i18nc("@item:inlistbox", "Name")},
        {QStringLiteral("type"), i18nc("@item:inlistbox", "Type")},
        {QStringLiteral("size"), i18nc("@item:inlistbox", "Size")},
        {QStringLiteral("date"), i18nc("@item:inlistbox", "Date")},
        {QStringLiteral("ext"), i18nc("@item:inlistbox", "Extension")},
    };
    for (const auto &[key, label] : groups) {
        m_groupBy->addItem(label, key);
    }
    m_groupBy->setCurrentIndex(m_groupBy->findData(settings.groupBy));
    generalLayout->addRow(i18nc("@label", "Group by:"), m_groupBy);

    m_sizeFormat = new QComboBox(generalGroup);
    m_sizeFormat->setObjectName(QStringLiteral("size_format"));
    m_sizeFormat->addItem(i18nc("@item:inlistbox", "Human readable"), QStringLiteral("human"));
    m_sizeFormat->addItem(i18nc("@item:inlistbox", "Bytes"), QStringLiteral("bytes"));
    m_sizeFormat->setCurrentIndex(m_sizeFormat->findData(settings.sizeFormat));
    generalLayout->addRow(i18nc("@label", "Size format:"), m_sizeFormat);

    m_dateFormat = new QComboBox(generalGroup);
    m_dateFormat->setObjectName(QStringLiteral("date_format"));
    m_dateFormat->addItem(i18nc("@item:inlistbox", "Locale"), QStringLiteral("locale"));
    m_dateFormat->addItem(i18nc("@item:inlistbox", "ISO 8601"), QStringLiteral("iso"));
    m_dateFormat->setCurrentIndex(m_dateFormat->findData(settings.dateFormat));
    generalLayout->addRow(i18nc("@label", "Date format:"), m_dateFormat);
    m_showSeconds = new QCheckBox(i18nc("@option:check", "Show seconds"), generalGroup);
    m_showSeconds->setObjectName(QStringLiteral("show_seconds"));
    m_showSeconds->setChecked(settings.showSeconds);
    generalLayout->addRow({}, m_showSeconds);
    viewLayout->addWidget(generalGroup);

    auto *iconsGroup = new QGroupBox(i18nc("@title:group", "Icon View"), viewPage);
    auto *iconsLayout = new QVBoxLayout(iconsGroup);
    auto *linesLayout = new QFormLayout;
    m_iconNameLines = new QSpinBox(iconsGroup);
    m_iconNameLines->setObjectName(QStringLiteral("icon_name_lines"));
    m_iconNameLines->setRange(1, 5);
    m_iconNameLines->setValue(settings.iconNameLines);
    linesLayout->addRow(i18nc("@label", "Name lines:"), m_iconNameLines);
    iconsLayout->addLayout(linesLayout);

    auto *informationGroup = new QGroupBox(i18nc("@title:group", "Metadata Shown"), iconsGroup);
    auto *informationLayout = new QGridLayout(informationGroup);
    for (qsizetype i = 0; i < IconInformation.size(); ++i) {
        const auto &[key, label] = IconInformation.at(i);
        auto *check = new QCheckBox(label, informationGroup);
        check->setObjectName(QStringLiteral("icon_info_%1").arg(key));
        check->setChecked(settings.iconInformation.contains(key));
        m_iconInformationChecks.insert(key, check);
        informationLayout->addWidget(check, i / 3, i % 3);
    }
    iconsLayout->addWidget(informationGroup);
    viewLayout->addWidget(iconsGroup);
    viewLayout->addStretch();
    addPage(viewPage, i18nc("@title:tab", "View"));

    auto *filesPage = new QWidget(this);
    auto *filesLayout = new QVBoxLayout(filesPage);
    auto *ignoreGroup = new QGroupBox(i18nc("@title:group", "Ignored Names"), filesPage);
    auto *ignoreLayout = new QVBoxLayout(ignoreGroup);
    m_ignoreEntries = new QPlainTextEdit(ignoreGroup);
    m_ignoreEntries->setObjectName(QStringLiteral("ignore_entries"));
    const IgnoreList ignoreList = IgnoreList::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files")));
    m_ignoreEntries->setPlainText(ignoreList.entries().join(QLatin1Char('\n')));
    auto *ignoreHint = new QLabel(i18nc("@info", "One file name or glob pattern per line. Matching is case-insensitive."), ignoreGroup);
    ignoreHint->setWordWrap(true);
    ignoreLayout->addWidget(m_ignoreEntries);
    ignoreLayout->addWidget(ignoreHint);
    filesLayout->addWidget(ignoreGroup);

    auto *colorsGroup = new QGroupBox(i18nc("@title:group", "File Colours by Extension"), filesPage);
    auto *colorsLayout = new QVBoxLayout(colorsGroup);
    m_colorRules = new QTableWidget(colorsGroup);
    m_colorRules->setObjectName(QStringLiteral("extension_color_rules"));
    m_colorRules->setColumnCount(2);
    m_colorRules->setHorizontalHeaderLabels({i18nc("@title:column", "Extension"), i18nc("@title:column", "Colour")});
    m_colorRules->horizontalHeader()->setStretchLastSection(true);
    m_colorRules->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_colorRules->setSelectionMode(QAbstractItemView::SingleSelection);
    m_colorRules->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const FileColorRules colors = FileColorRules::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files")));
    for (const QString &entry : colors.entries()) {
        const qsizetype separator = entry.indexOf(u'=');
        setColorRuleRow(m_colorRules->rowCount(), entry.first(separator), QColor(entry.sliced(separator + 1)));
    }
    colorsLayout->addWidget(m_colorRules);
    auto *colorButtons = new QHBoxLayout;
    auto *addColor = new QPushButton(i18nc("@action:button", "Add…"), colorsGroup);
    auto *editColor = new QPushButton(i18nc("@action:button", "Edit…"), colorsGroup);
    auto *removeColor = new QPushButton(i18nc("@action:button", "Remove"), colorsGroup);
    addColor->setObjectName(QStringLiteral("add_extension_color"));
    editColor->setObjectName(QStringLiteral("edit_extension_color"));
    removeColor->setObjectName(QStringLiteral("remove_extension_color"));
    colorButtons->addWidget(addColor);
    colorButtons->addWidget(editColor);
    colorButtons->addWidget(removeColor);
    colorButtons->addStretch();
    colorsLayout->addLayout(colorButtons);
    connect(addColor, &QPushButton::clicked, this, &SettingsDialog::addColorRule);
    connect(editColor, &QPushButton::clicked, this, &SettingsDialog::editColorRule);
    connect(removeColor, &QPushButton::clicked, this, &SettingsDialog::removeColorRule);
    filesLayout->addWidget(colorsGroup);
    addPage(filesPage, i18nc("@title:tab", "Files"));
}

void SettingsDialog::accept()
{
    ViewSettings settings;
    settings.visibleColumns.clear();
    for (const auto &[key, label] : Columns) {
        Q_UNUSED(label)
        const QCheckBox *check = m_columnChecks.value(key);
        if (check && check->isChecked()) {
            settings.visibleColumns.append(key);
        }
    }
    for (const auto &[key, label] : IconInformation) {
        Q_UNUSED(label)
        const QCheckBox *check = m_iconInformationChecks.value(key);
        if (check && check->isChecked()) {
            settings.iconInformation.append(key);
        }
    }
    settings.groupBy = m_groupBy->currentData().toString();
    settings.sizeFormat = m_sizeFormat->currentData().toString();
    settings.dateFormat = m_dateFormat->currentData().toString();
    settings.showSeconds = m_showSeconds->isChecked();
    settings.iconNameLines = m_iconNameLines->value();

    const QStringList ignoreEntries = m_ignoreEntries->toPlainText().split(u'\n', Qt::SkipEmptyParts);
    const IgnoreList ignoreList(ignoreEntries);
    const qsizetype nonEmptyEntries = std::count_if(ignoreEntries.cbegin(), ignoreEntries.cend(), [](const QString &entry) {
        return !entry.trimmed().isEmpty();
    });
    if (ignoreList.entries().size() != nonEmptyEntries) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid Ignore Rules"),
                             i18nc("@info", "Each ignore rule must be at most 80 characters, contain no slash, and the list may have at most 100 entries."));
        return;
    }

    KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
    QStringList colorEntries;
    for (int row = 0; row < m_colorRules->rowCount(); ++row) {
        const QString extension = m_colorRules->item(row, 0)->text();
        const QColor color = m_colorRules->item(row, 1)->data(Qt::UserRole).value<QColor>();
        if (!color.isValid()) {
            QMessageBox::warning(this, i18nc("@title:window", "Invalid File Colour"), i18nc("@info", "Choose a valid colour for every file extension."));
            return;
        }
        colorEntries.append(extension + u'=' + color.name(QColor::HexRgb));
    }
    const QStringList sanitizedColors = FileColorRules(colorEntries).entries();
    if (sanitizedColors.size() != m_colorRules->rowCount()) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid File Colour Rules"),
                             i18nc("@info", "Use a unique file extension of up to 32 letters, numbers, dots, underscores, plus signs, or hyphens."));
        return;
    }

    KConfigGroup view = KSharedConfig::openConfig()->group(QStringLiteral("View"));
    settings.writeConfig(view);
    files.writeEntry("IgnoreList", ignoreList.entries());
    files.writeEntry("ExtensionColors", sanitizedColors);
    KSharedConfig::openConfig()->sync();
    KPageDialog::accept();
}

void SettingsDialog::addColorRule()
{
    bool accepted = false;
    QString extension = QInputDialog::getText(this,
                                              i18nc("@title:window", "Add File Colour"),
                                              i18nc("@label:textbox", "Extension (for example: cpp or tar.gz):"),
                                              QLineEdit::Normal,
                                              QString(),
                                              &accepted)
                            .trimmed();
    if (!accepted || extension.isEmpty()) {
        return;
    }
    if (extension.startsWith(u'.')) {
        extension.remove(0, 1);
    }
    const QStringList normalized = FileColorRules({extension + QStringLiteral("=#010101")}).entries();
    if (normalized.isEmpty()) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid Extension"),
                             i18nc("@info", "Enter a file extension with up to 32 letters, numbers, dots, underscores, plus signs, or hyphens."));
        return;
    }
    extension = normalized.first().section(u'=', 0, 0);
    for (int row = 0; row < m_colorRules->rowCount(); ++row) {
        if (m_colorRules->item(row, 0)->text().compare(extension, Qt::CaseInsensitive) == 0) {
            m_colorRules->selectRow(row);
            editColorRule();
            return;
        }
    }
    const QColor color = QColorDialog::getColor(Qt::yellow, this, i18nc("@title:window", "Choose File Colour"));
    if (color.isValid()) {
        setColorRuleRow(m_colorRules->rowCount(), extension, color);
    }
}

void SettingsDialog::editColorRule()
{
    const int row = m_colorRules->currentRow();
    if (row < 0) {
        return;
    }
    const QString currentExtension = m_colorRules->item(row, 0)->text();
    bool accepted = false;
    QString extension = QInputDialog::getText(this,
                                              i18nc("@title:window", "Edit File Colour"),
                                              i18nc("@label:textbox", "Extension:"),
                                              QLineEdit::Normal,
                                              currentExtension,
                                              &accepted)
                            .trimmed();
    if (!accepted || extension.isEmpty()) {
        return;
    }
    if (extension.startsWith(u'.')) {
        extension.remove(0, 1);
    }
    const QStringList normalized = FileColorRules({extension + QStringLiteral("=#010101")}).entries();
    if (normalized.isEmpty()) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid Extension"),
                             i18nc("@info", "Enter a file extension with up to 32 letters, numbers, dots, underscores, plus signs, or hyphens."));
        return;
    }
    extension = normalized.first().section(u'=', 0, 0);
    for (int other = 0; other < m_colorRules->rowCount(); ++other) {
        if (other != row && m_colorRules->item(other, 0)->text() == extension) {
            QMessageBox::warning(this, i18nc("@title:window", "Duplicate Extension"), i18nc("@info", "There is already a colour rule for this extension."));
            return;
        }
    }
    const QColor initial = m_colorRules->item(row, 1)->data(Qt::UserRole).value<QColor>();
    const QColor color = QColorDialog::getColor(initial, this, i18nc("@title:window", "Choose File Colour"));
    if (color.isValid()) {
        setColorRuleRow(row, extension, color);
    }
}

void SettingsDialog::removeColorRule()
{
    const int row = m_colorRules->currentRow();
    if (row >= 0) {
        m_colorRules->removeRow(row);
    }
}

void SettingsDialog::setColorRuleRow(int row, const QString &extension, const QColor &color)
{
    if (row == m_colorRules->rowCount()) {
        m_colorRules->insertRow(row);
    }
    auto *extensionItem = new QTableWidgetItem(extension);
    auto *colorItem = new QTableWidgetItem(color.name(QColor::HexRgb));
    colorItem->setData(Qt::UserRole, color);
    colorItem->setBackground(color);
    m_colorRules->setItem(row, 0, extensionItem);
    m_colorRules->setItem(row, 1, colorItem);
}
