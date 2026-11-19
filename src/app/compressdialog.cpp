// SPDX-License-Identifier: GPL-3.0-or-later

#include "compressdialog.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace
{
QString formatText(ArchiveWriter::Format format)
{
    switch (format) {
    case ArchiveWriter::Format::Zip:
        return i18nc("@item:inlistbox archive format", "Zip");
    case ArchiveWriter::Format::ZipStored:
        return i18nc("@item:inlistbox archive format", "Zip, without compression");
    case ArchiveWriter::Format::TarGz:
        return i18nc("@item:inlistbox archive format", "Tar, gzip compressed");
    case ArchiveWriter::Format::TarXz:
        return i18nc("@item:inlistbox archive format", "Tar, xz compressed (smaller, slower)");
    case ArchiveWriter::Format::TarZstd:
        return i18nc("@item:inlistbox archive format", "Tar, zstd compressed (fast)");
    }
    return {};
}
}

CompressDialog::CompressDialog(const QString &defaultName, bool otherPaneAvailable, QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(defaultName, this))
    , m_suffix(new QLabel(this))
    , m_format(new QComboBox(this))
    , m_location(new QComboBox(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle(i18nc("@title:window", "Compress"));
    m_name->setObjectName(QStringLiteral("archiveName"));
    m_format->setObjectName(QStringLiteral("archiveFormat"));
    m_location->setObjectName(QStringLiteral("archiveLocation"));

    const QList<ArchiveWriter::Format> formats = ArchiveWriter::formats();
    for (ArchiveWriter::Format format : formats) {
        m_format->addItem(formatText(format));
    }
    m_location->addItems({i18nc("@item:inlistbox archive location", "This folder"), i18nc("@item:inlistbox archive location", "The other pane's folder")});
    m_location->setEnabled(otherPaneAvailable);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(m_name, 1);
    nameRow->addWidget(m_suffix);
    auto *form = new QFormLayout(this);
    form->addRow(i18nc("@label:textbox", "Archive name:"), nameRow);
    form->addRow(i18nc("@label:listbox", "Format:"), m_format);
    form->addRow(i18nc("@label:listbox", "Create in:"), m_location);
    form->addRow(m_buttons);

    m_buttons->button(QDialogButtonBox::Ok)->setText(i18nc("@action:button", "Compress"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_name, &QLineEdit::textChanged, this, &CompressDialog::updateState);
    connect(m_format, &QComboBox::currentIndexChanged, this, &CompressDialog::updateState);
    updateState();
    m_name->selectAll();
}

QString CompressDialog::archiveName() const
{
    return m_name->text().trimmed();
}

ArchiveWriter::Format CompressDialog::format() const
{
    return ArchiveWriter::formats().at(m_format->currentIndex());
}

bool CompressDialog::toOtherPane() const
{
    return m_location->currentIndex() == 1;
}

void CompressDialog::updateState()
{
    m_suffix->setText(u'.' + ArchiveWriter::suffix(format()));
    const QString name = archiveName();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!name.isEmpty() && !name.contains(u'/'));
}
