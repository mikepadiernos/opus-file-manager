// SPDX-License-Identifier: GPL-3.0-or-later

#include "tageditdialog.h"

#include "symbolicicon.h"

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
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int IdRole = Qt::UserRole;
}

QIcon tagColorIcon(const QString &color)
{
    const QColor fill(color);
    if (color.isEmpty() || !fill.isValid()) {
        return SymbolicIcon::fromTheme(QStringLiteral("tag"));
    }
    QIcon icon;
    for (const int size : {16, 22, 32}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(fill.darker(140), 1));
        painter.setBrush(fill);
        const qreal inset = size / 8.0;
        painter.drawEllipse(QRectF(inset, inset, size - 2 * inset, size - 2 * inset));
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

bool tagMatchesFilter(const TagLibrary::Library &library, const QString &id, const QString &shown, const QString &needle)
{
    if (needle.isEmpty() || shown.contains(needle, Qt::CaseInsensitive)) {
        return true;
    }
    const TagLibrary::Tag *tag = library.tag(id);
    return tag
        && (tag->shorthand.contains(needle, Qt::CaseInsensitive) || std::any_of(tag->aliases.cbegin(), tag->aliases.cend(), [&needle](const QString &alias) {
                return alias.contains(needle, Qt::CaseInsensitive);
            }));
}

TagEditDialog::TagEditDialog(const TagLibrary::Library &library, const TagLibrary::Tag &tag, QWidget *parent)
    : QDialog(parent)
    , m_library(library)
    , m_id(tag.id.isEmpty() ? TagLibrary::createId() : tag.id)
    , m_parents(tag.parents)
    , m_name(new QLineEdit(tag.name, this))
    , m_aliases(new QPlainTextEdit(tag.aliases.join(QLatin1Char('\n')), this))
    , m_shorthand(new QLineEdit(tag.shorthand, this))
    , m_useColor(new QCheckBox(i18nc("@option:check", "Use a colour:"), this))
    , m_color(new KColorButton(this))
    , m_parentFilter(new QLineEdit(this))
    , m_parentList(new QListWidget(this))
    , m_note(new KMessageWidget(this))
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("tagEditDialog"));
    setWindowTitle(tag.id.isEmpty() ? i18nc("@title:window", "New Tag") : i18nc("@title:window", "Edit Tag"));

    m_name->setObjectName(QStringLiteral("tagName"));
    m_name->setPlaceholderText(i18nc("@info:placeholder", "Required"));
    m_aliases->setObjectName(QStringLiteral("tagAliases"));
    m_aliases->setTabChangesFocus(true);
    m_aliases->setPlaceholderText(i18nc("@info:placeholder", "One alias per line"));
    m_aliases->setFixedHeight(m_aliases->fontMetrics().lineSpacing() * 4 + 2 * m_aliases->frameWidth() + 8);
    m_aliases->setToolTip(i18nc("@info:tooltip", "Other names that find this tag when searching or assigning"));
    m_shorthand->setObjectName(QStringLiteral("tagShorthand"));
    m_shorthand->setPlaceholderText(i18nc("@info:placeholder", "Optional, without spaces"));
    m_shorthand->setToolTip(i18nc("@info:tooltip", "A short name to type when searching or assigning"));
    m_useColor->setObjectName(QStringLiteral("tagUseColor"));
    m_useColor->setChecked(!tag.color.isEmpty());
    m_color->setObjectName(QStringLiteral("tagColor"));
    m_color->setAccessibleName(i18nc("@info:accessible", "Tag colour"));
    m_color->setColor(tag.color.isEmpty() ? QColor(QStringLiteral("#3daee9")) : QColor(tag.color));
    m_color->setEnabled(m_useColor->isChecked());
    m_parentFilter->setObjectName(QStringLiteral("tagParentFilter"));
    m_parentFilter->setPlaceholderText(i18nc("@info:placeholder", "Filter tags…"));
    m_parentFilter->setClearButtonEnabled(true);
    m_parentList->setObjectName(QStringLiteral("tagParents"));
    m_parentList->setAccessibleName(i18nc("@info:accessible", "Parent tags"));
    m_parentList->setUniformItemSizes(true);
    m_note->setObjectName(QStringLiteral("tagDuplicateNote"));
    m_note->setMessageType(KMessageWidget::Information);
    m_note->setWordWrap(true);
    m_note->setCloseButtonVisible(false);
    m_error->setObjectName(QStringLiteral("tagError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);

    auto *colorRow = new QHBoxLayout;
    colorRow->addWidget(m_useColor);
    colorRow->addWidget(m_color);
    colorRow->addStretch();

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "&Name:"), m_name);
    form->addRow(i18nc("@label:textbox", "&Aliases:"), m_aliases);
    form->addRow(i18nc("@label:textbox", "&Shorthand:"), m_shorthand);
    form->addRow(QString(), colorRow);

    auto *parentsLabel = new QLabel(i18nc("@label", "&Parent tags:"), this);
    parentsLabel->setBuddy(m_parentList);
    auto *parentsHint = new QLabel(i18nc("@info", "Items with this tag also match searches for its parent tags."), this);
    parentsHint->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(parentsLabel);
    layout->addWidget(parentsHint);
    layout->addWidget(m_parentFilter);
    layout->addWidget(m_parentList, 1);
    layout->addWidget(m_note);
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
        if (problem().isEmpty()) {
            accept();
        }
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_name, &QLineEdit::textChanged, this, &TagEditDialog::validate);
    connect(m_aliases, &QPlainTextEdit::textChanged, this, &TagEditDialog::validate);
    connect(m_shorthand, &QLineEdit::textChanged, this, &TagEditDialog::validate);
    connect(m_useColor, &QCheckBox::toggled, m_color, &QWidget::setEnabled);
    connect(m_useColor, &QCheckBox::toggled, this, &TagEditDialog::validate);
    connect(m_color, &KColorButton::changed, this, &TagEditDialog::validate);
    connect(m_parentFilter, &QLineEdit::textChanged, this, &TagEditDialog::filterParents);
    connect(m_parentList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const QString id = item->data(IdRole).toString();
        if (item->checkState() == Qt::Checked && !m_parents.contains(id)) {
            m_parents.append(id);
        } else if (item->checkState() != Qt::Checked) {
            m_parents.removeAll(id);
        }
        validate();
    });

    populateParents();
    validate();
    resize(460, 560);
}

TagLibrary::Tag TagEditDialog::tag() const
{
    TagLibrary::Tag result;
    result.id = m_id;
    result.name = m_name->text().trimmed();
    for (const QString &line : m_aliases->toPlainText().split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty()) {
            result.aliases.append(line.trimmed());
        }
    }
    result.shorthand = m_shorthand->text().trimmed();
    result.color = m_useColor->isChecked() ? m_color->color().name(QColor::HexRgb) : QString();
    result.parents = m_parents;
    return result;
}

QString TagEditDialog::problem() const
{
    return m_library.checkTag(tag());
}

QString TagEditDialog::duplicateNote() const
{
    return m_note->text();
}

void TagEditDialog::populateParents()
{
    const QStringList below = m_library.descendants(m_id);
    QList<const TagLibrary::Tag *> tags;
    for (const TagLibrary::Tag &candidate : m_library.tags()) {
        if (candidate.id != m_id) {
            tags.append(&candidate);
        }
    }
    std::sort(tags.begin(), tags.end(), [this](const TagLibrary::Tag *a, const TagLibrary::Tag *b) {
        return m_library.displayName(a->id).localeAwareCompare(m_library.displayName(b->id)) < 0;
    });
    const QSignalBlocker blocker(m_parentList);
    for (const TagLibrary::Tag *candidate : std::as_const(tags)) {
        auto *item = new QListWidgetItem(tagColorIcon(candidate->color), m_library.displayName(candidate->id), m_parentList);
        item->setData(IdRole, candidate->id);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(m_parents.contains(candidate->id) ? Qt::Checked : Qt::Unchecked);
        if (below.contains(candidate->id)) {
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
            item->setToolTip(i18nc("@info:tooltip", "This tag is below the edited tag, so it can't also be its parent."));
        }
    }
    if (tags.isEmpty()) {
        m_parentFilter->setEnabled(false);
        auto *item = new QListWidgetItem(i18nc("@item", "No other tags"), m_parentList);
        item->setFlags(Qt::NoItemFlags);
    }
}

void TagEditDialog::filterParents(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_parentList->count(); ++i) {
        QListWidgetItem *item = m_parentList->item(i);
        item->setHidden(!tagMatchesFilter(m_library, item->data(IdRole).toString(), item->text(), needle));
    }
}

void TagEditDialog::validate()
{
    const TagLibrary::Tag edited = tag();
    const QString error = m_library.checkTag(edited);
    m_error->setText(error);
    m_error->setVisible(!error.isEmpty() && !edited.name.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());

    QString note;
    if (error.isEmpty() && !m_library.findTags(edited.name).isEmpty()) {
        TagLibrary::Library preview = m_library;
        preview.setTag(edited);
        const QString shown = preview.displayName(edited.id);
        if (shown != edited.name) {
            note = i18nc("@info", "Another tag is also named “%1”, so this tag is shown as “%2”.", edited.name, shown);
        }
    }
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
}
