// SPDX-License-Identifier: GPL-3.0-or-later

#include "compressdialog.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

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
    case ArchiveWriter::Format::SevenZip:
        return i18nc("@item:inlistbox archive format", "7z (without a password)");
    }
    return {};
}
}

CompressDialog::CompressDialog(const QString &defaultName,
                               bool otherPaneAvailable,
                               QWidget *parent,
                               ArchiveWriter::Format defaultFormat,
                               bool otherPaneByDefault,
                               bool arkAvailable)
    : QDialog(parent)
    , m_name(new QLineEdit(defaultName, this))
    , m_password(new QLineEdit(this))
    , m_passwordConfirmation(new QLineEdit(this))
    , m_suffix(new QLabel(this))
    , m_format(new QComboBox(this))
    , m_split(new QCheckBox(i18nc("@option:check", "Split into volumes of"), this))
    , m_volumeSize(new QSpinBox(this))
    , m_location(new QComboBox(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
    , m_ark(new QPushButton(i18nc("@action:button", "More Options in Ark…"), this))
{
    setWindowTitle(i18nc("@title:window", "Compress"));
    m_name->setObjectName(QStringLiteral("archiveName"));
    m_password->setObjectName(QStringLiteral("archivePassword"));
    m_passwordConfirmation->setObjectName(QStringLiteral("archivePasswordConfirmation"));
    m_password->setEchoMode(QLineEdit::Password);
    m_passwordConfirmation->setEchoMode(QLineEdit::Password);
    m_format->setObjectName(QStringLiteral("archiveFormat"));
    m_location->setObjectName(QStringLiteral("archiveLocation"));
    m_split->setObjectName(QStringLiteral("archiveSplit"));
    m_volumeSize->setObjectName(QStringLiteral("archiveVolumeSize"));
    m_volumeSize->setRange(1, 1024 * 1024);
    m_volumeSize->setValue(100);
    m_volumeSize->setSuffix(i18nc("@item:valuesuffix mebibytes", " MiB"));
    m_volumeSize->setEnabled(false);
    m_split->setToolTip(i18nc("@info:tooltip",
                              "Writes the archive as numbered parts of at most this size, such as photos.7z.001. 7-Zip opens the first "
                              "part directly; other tools need the parts joined with Combine Files first."));
    m_ark->setObjectName(QStringLiteral("compressWithArk"));
    m_ark->setEnabled(arkAvailable);
    m_ark->setToolTip(arkAvailable ? i18nc("@info:tooltip",
                                           "Opens Ark's Compress dialog, which offers every format the installed tools write, encrypted 7z "
                                           "archives with encrypted file names, compression levels and split archives.")
                                   : i18nc("@info:tooltip", "Install Ark for more archive formats and options."));

    const QList<ArchiveWriter::Format> formats = ArchiveWriter::formats();
    for (ArchiveWriter::Format format : formats) {
        m_format->addItem(formatText(format));
    }
    m_format->setCurrentIndex(qMax(0, formats.indexOf(defaultFormat)));
    m_location->addItems({i18nc("@item:inlistbox archive location", "This folder"), i18nc("@item:inlistbox archive location", "The other pane's folder")});
    m_location->setEnabled(otherPaneAvailable);
    m_location->setCurrentIndex(otherPaneAvailable && otherPaneByDefault ? 1 : 0);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(m_name, 1);
    nameRow->addWidget(m_suffix);
    auto *form = new QFormLayout(this);
    m_form = form;
    form->addRow(i18nc("@label:textbox", "Archive name:"), nameRow);
    form->addRow(i18nc("@label:listbox", "Format:"), m_format);
    form->addRow(i18nc("@label:textbox", "Password (ZIP only):"), m_password);
    form->addRow(i18nc("@label:textbox", "Confirm password:"), m_passwordConfirmation);
    auto *splitRow = new QHBoxLayout;
    splitRow->addWidget(m_split);
    splitRow->addWidget(m_volumeSize, 1);
    form->addRow({}, splitRow);
    form->addRow(i18nc("@label:listbox", "Create in:"), m_location);
    form->addRow(m_buttons);

    m_buttons->button(QDialogButtonBox::Ok)->setText(i18nc("@action:button", "Compress"));
    m_buttons->addButton(m_ark, QDialogButtonBox::ActionRole);
    connect(m_ark, &QPushButton::clicked, this, [this] {
        done(UseArk);
    });
    connect(m_split, &QCheckBox::toggled, m_volumeSize, &QWidget::setEnabled);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_name, &QLineEdit::textChanged, this, &CompressDialog::updateState);
    connect(m_password, &QLineEdit::textChanged, this, &CompressDialog::updateState);
    connect(m_passwordConfirmation, &QLineEdit::textChanged, this, &CompressDialog::updateState);
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

QString CompressDialog::password() const
{
    return m_password->text();
}

qint64 CompressDialog::volumeSize() const
{
    return m_split->isChecked() ? qint64(m_volumeSize->value()) * 1024 * 1024 : 0;
}

void CompressDialog::setVolumeDefaults(bool split, int volumeSizeMiB)
{
    m_volumeSize->setValue(volumeSizeMiB);
    m_split->setChecked(split);
    m_volumeSize->setEnabled(split);
}

void CompressDialog::setPasswordOffered(bool offered)
{
    if (!offered) {
        m_password->clear();
        m_passwordConfirmation->clear();
    }
    m_form->setRowVisible(m_password, offered);
    m_form->setRowVisible(m_passwordConfirmation, offered);
}

void CompressDialog::updateState()
{
    m_suffix->setText(u'.' + ArchiveWriter::suffix(format()));
    const QString name = archiveName();
    const bool zip = format() == ArchiveWriter::Format::Zip || format() == ArchiveWriter::Format::ZipStored;
    m_password->setEnabled(zip);
    m_passwordConfirmation->setEnabled(zip);
    m_buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(!name.isEmpty() && !name.contains(u'/') && (zip ? m_password->text() == m_passwordConfirmation->text() : m_password->text().isEmpty()));
}
