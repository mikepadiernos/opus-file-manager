// SPDX-License-Identifier: GPL-3.0-or-later

#include "informationpanel.h"

#include "libraryregistry.h"
#include "localizeddates.h"
#include "localizednumbers.h"
#include "settings.h"
#include "symbolicicon.h"
#include "tagassignment.h"

#include <KIO/Global>
#include <KLocalizedString>

#include <QDate>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QUrl>

#include <algorithm>

namespace
{
QList<QPair<QString, QString>> fieldDefinitions()
{
    return {
        {QStringLiteral("size"), i18nc("@label:file information", "Size")},
        {QStringLiteral("permissions"), i18nc("@label:file information", "Permissions")},
        {QStringLiteral("octalPermissions"), i18nc("@label:file information", "Permissions (octal)")},
        {QStringLiteral("owner"), i18nc("@label:file information", "Owner")},
        {QStringLiteral("ownerAndGroup"), i18nc("@label:file information", "Owner and group")},
        {QStringLiteral("creationTime"), i18nc("@label:file information", "Created")},
        {QStringLiteral("modificationTime"), i18nc("@label:file information", "Modified")},
        {QStringLiteral("accessTime"), i18nc("@label:file information", "Accessed")},
        {QStringLiteral("mimeType"), i18nc("@label:file information", "MIME type")},
        {QStringLiteral("friendlyMimeType"), i18nc("@label:file information", "File type")},
        {QStringLiteral("linkDest"), i18nc("@label:file information", "Link target")},
        {QStringLiteral("localPathOrUrl"), i18nc("@label:file information", "Path or URL")},
        {QStringLiteral("comment"), i18nc("@label:file information", "Comment")},
    };
}

QString valueFor(const KFileItem &item, const QString &field)
{
    if (item.isNull()) {
        return {};
    }
    if (field == QLatin1String("size")) {
        return item.isDir() ? QString() : LocalizedNumbers::shape(KIO::convertSize(item.size()));
    }
    if (field == QLatin1String("permissions")) {
        return item.permissionsString();
    }
    if (field == QLatin1String("octalPermissions")) {
        return QString::number(item.permissions() & 07777, 8);
    }
    if (field == QLatin1String("owner")) {
        return item.user();
    }
    if (field == QLatin1String("ownerAndGroup")) {
        const QString user = item.user();
        const QString group = item.group();
        if (user.isEmpty() || group.isEmpty()) {
            return {};
        }
        return i18nc("@info:file owner and group", "%1, %2", user, group);
    }
    if (field == QLatin1String("creationTime")) {
        return LocalizedDates::fileTime(item, KFileItem::CreationTime);
    }
    if (field == QLatin1String("modificationTime")) {
        return LocalizedDates::fileTime(item, KFileItem::ModificationTime);
    }
    if (field == QLatin1String("accessTime")) {
        return LocalizedDates::fileTime(item, KFileItem::AccessTime);
    }
    if (field == QLatin1String("mimeType")) {
        return item.mimetype();
    }
    if (field == QLatin1String("friendlyMimeType")) {
        return item.mimeComment();
    }
    if (field == QLatin1String("linkDest")) {
        return item.isLink() ? item.linkDest() : QString();
    }
    if (field == QLatin1String("localPathOrUrl")) {
        return item.url().toDisplayString(QUrl::PreferLocalFile);
    }
    if (field == QLatin1String("comment")) {
        return item.comment();
    }
    return {};
}
}

InformationPanel::InformationPanel(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QFormLayout(this))
    , m_panelOptions(new QToolButton(this))
{
    setObjectName(QStringLiteral("informationPanelContents"));
    m_panelOptions->setObjectName(QStringLiteral("informationPanelOptions"));
    m_panelOptions->setIcon(SymbolicIcon::fromTheme(QStringLiteral("application-menu")));
    m_panelOptions->setToolTip(i18nc("@info:tooltip", "Information panel options"));
    m_panelOptions->setAccessibleName(i18nc("@action", "Information panel options"));
    m_panelOptions->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_panelOptions);
    auto *left = menu->addAction(i18nc("@action", "Move Information Panel Left"));
    left->setObjectName(QStringLiteral("informationPlaceLeft"));
    auto *right = menu->addAction(i18nc("@action", "Move Information Panel Right"));
    right->setObjectName(QStringLiteral("informationPlaceRight"));
    auto *hide = menu->addAction(i18nc("@action", "Hide Information Panel"));
    hide->setObjectName(QStringLiteral("informationHide"));
    connect(left, &QAction::triggered, this, [this] {
        Q_EMIT placementRequested(Qt::LeftDockWidgetArea);
    });
    connect(right, &QAction::triggered, this, [this] {
        Q_EMIT placementRequested(Qt::RightDockWidgetArea);
    });
    connect(hide, &QAction::triggered, this, &InformationPanel::closeRequested);
    m_panelOptions->setMenu(menu);
    m_panelOptions->hide();
    m_layout->addRow(m_panelOptions);
    m_layout->setAlignment(m_panelOptions, Qt::AlignRight);
    m_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_layout->setContentsMargins(8, 8, 8, 8);
    rebuild();
}

void InformationPanel::setTitleVisible(bool visible)
{
    m_panelOptions->setVisible(!visible);
}

void InformationPanel::setFields(const QStringList &fields)
{
    const auto definitions = fieldDefinitions();
    QStringList valid;
    for (const QString &field : fields) {
        if (std::any_of(definitions.cbegin(),
                        definitions.cend(),
                        [&field](const auto &candidate) {
                            return candidate.first == field;
                        })
            && !valid.contains(field)) {
            valid.append(field);
        }
    }
    if (m_fields == valid) {
        return;
    }
    m_fields = std::move(valid);
    rebuild();
}

void InformationPanel::setItem(const KFileItem &item)
{
    m_item = item;
    refresh();
}

void InformationPanel::refresh()
{
    for (const QString &field : m_fields) {
        const QString text = valueFor(m_item, field);
        m_values.value(field)->setText(text.isEmpty() ? i18nc("@info:file metadata", "Unavailable") : text);
    }
    refreshTags();
}

void InformationPanel::refreshTags()
{
    if (!m_tags) {
        return;
    }
    const QString path = m_item.isNull() || !m_item.url().isLocalFile() ? QString() : QDir::cleanPath(m_item.url().toLocalFile());
    const QString root = path.isEmpty() ? QString() : LibraryRegistry::rootFor(path);
    const TagLibrary::Library library = root.isEmpty() || root == path ? TagLibrary::Library() : TagAssignment::cachedLibrary(root);
    m_layout->setRowVisible(m_tags, !library.isNull() && Settings::showTagsInInformationPanel());
    m_layout->setRowVisible(m_libraryFields, !library.isNull() && !library.fields().isEmpty());
    if (library.isNull()) {
        m_tags->clear();
        m_tags->setToolTip({});
        m_libraryFields->clear();
        return;
    }
    const TagLibrary::Entry *entry = library.entry(library.relativePath(path));
    const QStringList direct = entry ? entry->tags : QStringList();
    const QString separator = i18nc("@item list separator", ", ");
    QStringList names;
    QStringList inherited;
    for (const QString &id : direct) {
        names.append(library.displayName(id));
        for (const QString &ancestor : library.ancestors(id)) {
            const QString name = library.displayName(ancestor);
            if (!direct.contains(ancestor) && !inherited.contains(name)) {
                inherited.append(name);
            }
        }
    }
    if (Settings::showInheritedTags()) {
        for (const QString &name : std::as_const(inherited)) {
            names.append(i18nc("@item:intext a tag inherited from a parent tag", "%1 (inherited)", name));
        }
    }
    m_tags->setText(names.isEmpty() ? i18nc("@info:file metadata", "No tags") : names.join(separator));
    m_tags->setToolTip(inherited.isEmpty() ? QString() : i18nc("@info:tooltip", "Inherited tags: %1", inherited.join(separator)));

    QStringList lines;
    for (const TagLibrary::Field &field : library.fields()) {
        QString value = entry ? entry->fields.value(field.id) : QString();
        if (value.isEmpty()) {
            continue;
        }
        if (field.type == TagLibrary::FieldType::Date) {
            if (const QDate date = QDate::fromString(value, Qt::ISODate); date.isValid()) {
                value = LocalizedDates::date(date);
            }
        }
        lines.append(i18nc("@info:file metadata field name and value", "%1: %2", field.name, value));
    }
    m_libraryFields->setText(lines.isEmpty() ? i18nc("@info:file metadata no field values", "None") : lines.join(QLatin1Char('\n')));
}

void InformationPanel::rebuild()
{
    while (m_layout->rowCount() > 1) {
        m_layout->removeRow(1);
    }
    m_values.clear();
    m_tags = nullptr;
    m_libraryFields = nullptr;
    if (m_fields.isEmpty()) {
        auto *emptyMessage = new QLabel(i18nc("@info", "No metadata fields are enabled."), this);
        emptyMessage->setWordWrap(true);
        m_layout->addRow(emptyMessage);
    }
    const auto definitions = fieldDefinitions();
    for (const QString &field : m_fields) {
        const auto definition = std::find_if(definitions.cbegin(), definitions.cend(), [&field](const auto &candidate) {
            return candidate.first == field;
        });
        if (definition == definitions.cend()) {
            continue;
        }
        auto *value = new QLabel(this);
        value->setObjectName(QStringLiteral("informationValue_") + field);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setWordWrap(true);
        m_layout->addRow(definition->second, value);
        m_values.insert(field, value);
    }
    m_tags = new QLabel(this);
    m_tags->setObjectName(QStringLiteral("informationTags"));
    m_tags->setTextFormat(Qt::PlainText);
    m_tags->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_tags->setWordWrap(true);
    m_layout->addRow(i18nc("@label:file information", "Tags"), m_tags);
    m_libraryFields = new QLabel(this);
    m_libraryFields->setObjectName(QStringLiteral("informationLibraryFields"));
    m_libraryFields->setTextFormat(Qt::PlainText);
    m_libraryFields->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_libraryFields->setWordWrap(true);
    m_layout->addRow(i18nc("@label:file information tag library fields", "Fields"), m_libraryFields);
    refresh();
}
