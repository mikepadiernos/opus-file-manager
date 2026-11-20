// SPDX-License-Identifier: GPL-3.0-or-later

#include "informationpanel.h"

#include <KIO/Global>
#include <KLocalizedString>

#include <QFormLayout>
#include <QLabel>
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
        return item.isDir() ? QString() : KIO::convertSize(item.size());
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
        return item.timeString(KFileItem::CreationTime);
    }
    if (field == QLatin1String("modificationTime")) {
        return item.timeString(KFileItem::ModificationTime);
    }
    if (field == QLatin1String("accessTime")) {
        return item.timeString(KFileItem::AccessTime);
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
{
    setObjectName(QStringLiteral("informationPanelContents"));
    m_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_layout->setContentsMargins(8, 8, 8, 8);
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
}

void InformationPanel::rebuild()
{
    while (m_layout->rowCount() > 0) {
        m_layout->removeRow(0);
    }
    m_values.clear();
    if (m_fields.isEmpty()) {
        auto *emptyMessage = new QLabel(i18nc("@info", "No metadata fields are enabled."), this);
        emptyMessage->setWordWrap(true);
        m_layout->addRow(emptyMessage);
        return;
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
    refresh();
}
