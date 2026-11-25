// SPDX-License-Identifier: GPL-3.0-or-later

#include "taggroupdialog.h"

#include "tageditdialog.h"

#include <KColorButton>
#include <KLocalizedString>
#include <KMessageWidget>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int IdRole = Qt::UserRole;
}

TagGroupDialog::TagGroupDialog(const TagLibrary::Library &library, const TagLibrary::Group &group, QWidget *parent)
    : QDialog(parent)
    , m_library(library)
    , m_id(group.id.isEmpty() ? TagLibrary::createId() : group.id)
    , m_tags(group.tags)
    , m_name(new QLineEdit(group.name, this))
    , m_useColor(new QCheckBox(i18nc("@option:check", "Use a colour:"), this))
    , m_color(new KColorButton(this))
    , m_tagFilter(new QLineEdit(this))
    , m_tagList(new QListWidget(this))
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("tagGroupDialog"));
    setWindowTitle(group.id.isEmpty() ? i18nc("@title:window", "New Tag Group") : i18nc("@title:window", "Edit Tag Group"));

    m_name->setObjectName(QStringLiteral("tagGroupName"));
    m_name->setPlaceholderText(i18nc("@info:placeholder", "Required"));
    m_useColor->setObjectName(QStringLiteral("tagGroupUseColor"));
    m_useColor->setChecked(!group.color.isEmpty());
    m_color->setObjectName(QStringLiteral("tagGroupColor"));
    m_color->setAccessibleName(i18nc("@info:accessible", "Group colour"));
    m_color->setColor(group.color.isEmpty() ? QColor(QStringLiteral("#3daee9")) : QColor(group.color));
    m_color->setEnabled(m_useColor->isChecked());
    m_tagFilter->setObjectName(QStringLiteral("tagGroupTagFilter"));
    m_tagFilter->setPlaceholderText(i18nc("@info:placeholder", "Filter tags…"));
    m_tagFilter->setClearButtonEnabled(true);
    m_tagList->setObjectName(QStringLiteral("tagGroupTags"));
    m_tagList->setAccessibleName(i18nc("@info:accessible", "Tags in the group"));
    m_tagList->setUniformItemSizes(true);
    m_error->setObjectName(QStringLiteral("tagGroupError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);

    auto *colorRow = new QHBoxLayout;
    colorRow->addWidget(m_useColor);
    colorRow->addWidget(m_color);
    colorRow->addStretch();

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "&Name:"), m_name);
    form->addRow(QString(), colorRow);

    auto *tagsLabel = new QLabel(i18nc("@label", "&Tags:"), this);
    tagsLabel->setBuddy(m_tagList);
    auto *tagsHint = new QLabel(i18nc("@info", "Groups only organize the tag list. A tag can be in several groups, and items keep their tags."), this);
    tagsHint->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(tagsLabel);
    layout->addWidget(tagsHint);
    layout->addWidget(m_tagFilter);
    layout->addWidget(m_tagList, 1);
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
        if (problem().isEmpty()) {
            accept();
        }
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_name, &QLineEdit::textChanged, this, &TagGroupDialog::validate);
    connect(m_useColor, &QCheckBox::toggled, m_color, &QWidget::setEnabled);
    connect(m_useColor, &QCheckBox::toggled, this, &TagGroupDialog::validate);
    connect(m_color, &KColorButton::changed, this, &TagGroupDialog::validate);
    connect(m_tagFilter, &QLineEdit::textChanged, this, &TagGroupDialog::filterTags);
    connect(m_tagList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const QString id = item->data(IdRole).toString();
        if (item->checkState() == Qt::Checked && !m_tags.contains(id)) {
            m_tags.append(id);
        } else if (item->checkState() != Qt::Checked) {
            m_tags.removeAll(id);
        }
    });

    populateTags();
    validate();
    resize(420, 520);
}

TagLibrary::Group TagGroupDialog::group() const
{
    TagLibrary::Group result;
    result.id = m_id;
    result.name = m_name->text().trimmed();
    result.color = m_useColor->isChecked() ? m_color->color().name(QColor::HexRgb) : QString();
    result.tags = m_tags;
    return result;
}

QString TagGroupDialog::problem() const
{
    return m_library.checkGroup(group());
}

void TagGroupDialog::populateTags()
{
    QList<const TagLibrary::Tag *> tags;
    for (const TagLibrary::Tag &tag : m_library.tags()) {
        tags.append(&tag);
    }
    std::sort(tags.begin(), tags.end(), [this](const TagLibrary::Tag *a, const TagLibrary::Tag *b) {
        return m_library.displayName(a->id).localeAwareCompare(m_library.displayName(b->id)) < 0;
    });
    const QSignalBlocker blocker(m_tagList);
    for (const TagLibrary::Tag *tag : std::as_const(tags)) {
        auto *item = new QListWidgetItem(tagColorIcon(tag->color), m_library.displayName(tag->id), m_tagList);
        item->setData(IdRole, tag->id);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(m_tags.contains(tag->id) ? Qt::Checked : Qt::Unchecked);
    }
    if (tags.isEmpty()) {
        m_tagFilter->setEnabled(false);
        auto *item = new QListWidgetItem(i18nc("@item", "No tags yet"), m_tagList);
        item->setFlags(Qt::NoItemFlags);
    }
}

void TagGroupDialog::filterTags(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_tagList->count(); ++i) {
        QListWidgetItem *item = m_tagList->item(i);
        item->setHidden(!tagMatchesFilter(m_library, item->data(IdRole).toString(), item->text(), needle));
    }
}

void TagGroupDialog::validate()
{
    const TagLibrary::Group edited = group();
    const QString error = m_library.checkGroup(edited);
    m_error->setText(error);
    m_error->setVisible(!error.isEmpty() && !edited.name.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
}
