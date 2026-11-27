// SPDX-License-Identifier: GPL-3.0-or-later

#include "remotemountdialog.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using RemoteMounts::Backend;

RemoteMountDialog::RemoteMountDialog(const RemoteProfiles::Profile &profile, const KConfigGroup &group, QWidget *parent, RemoteMounts::ExecutableLookup find)
    : QDialog(parent)
    , m_profile(profile)
    , m_group(group)
    , m_find(std::move(find))
    , m_backend(new QComboBox(this))
    , m_readOnly(new QCheckBox(i18nc("@option:check", "Read-only"), this))
    , m_location(new QLineEdit(this))
    , m_atStartup(new QCheckBox(i18nc("@option:check", "Mount when Plasma Opus starts"), this))
    , m_hint(new QLabel(this))
    , m_status(new QLabel(this))
{
    setWindowTitle(i18nc("@title:window %1 is a saved connection", "Mount Options for %1", profile.name.trimmed()));

    for (Backend backend : RemoteMounts::backends(profile.protocol)) {
        const QString unavailable = RemoteMounts::unavailableReason(backend, m_find);
        m_backend->addItem(unavailable.isEmpty() ? RemoteMounts::name(backend)
                                                 : i18nc("@item:inlistbox %1 is a mount backend", "%1 (not installed)", RemoteMounts::name(backend)),
                           int(backend));
        m_backend->setItemData(m_backend->count() - 1, unavailable, Qt::ToolTipRole);
    }

    auto *browse = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), QString(), this);
    browse->setToolTip(i18nc("@info:tooltip", "Choose an empty folder"));
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose an Empty Folder"), m_location->text());
        if (!folder.isEmpty()) {
            m_location->setText(folder);
        }
    });
    auto *locationRow = new QHBoxLayout;
    locationRow->addWidget(m_location, 1);
    locationRow->addWidget(browse);

    m_readOnly->setToolTip(i18nc("@info:tooltip", "Applications can open files on the mount but not change them"));
    m_atStartup->setToolTip(i18nc("@info:tooltip", "Mount this connection each time Plasma Opus starts, and unmount it when Plasma Opus quits"));
    m_hint->setWordWrap(true);
    m_hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setWordWrap(true);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Mount with:"), m_backend);
    form->addRow(QString(), m_readOnly);
    form->addRow(i18nc("@label:textbox", "Folder:"), locationRow);
    form->addRow(QString(), m_atStartup);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_save = buttons->button(QDialogButtonBox::Save);
    m_mount = buttons->addButton(i18nc("@action:button", "Save and Mount"), QDialogButtonBox::ActionRole);
    m_mount->setIcon(SymbolicIcon::fromTheme(QStringLiteral("drive-harddisk")));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (save()) {
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_mount, &QPushButton::clicked, this, [this] {
        if (save()) {
            Q_EMIT mountRequested();
            accept();
        }
    });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_hint);
    layout->addWidget(m_status);
    layout->addStretch();
    layout->addWidget(buttons);

    const RemoteMounts::Options saved = RemoteMounts::loadOptions(m_group, profile);
    m_backend->setCurrentIndex(qMax(0, m_backend->findData(int(saved.backend))));
    m_readOnly->setChecked(saved.readOnly);
    m_location->setText(saved.location);
    m_atStartup->setChecked(saved.atStartup);

    connect(m_backend, &QComboBox::currentIndexChanged, this, &RemoteMountDialog::updateState);
    connect(m_readOnly, &QCheckBox::toggled, this, &RemoteMountDialog::updateState);
    connect(m_location, &QLineEdit::textChanged, this, &RemoteMountDialog::updateState);
    updateState();
    resize(sizeHint().expandedTo(QSize(520, 0)));
}

RemoteMounts::Options RemoteMountDialog::options() const
{
    RemoteMounts::Options options;
    options.backend = Backend(m_backend->currentData().toInt());
    options.readOnly = m_readOnly->isEnabled() && m_readOnly->isChecked();
    options.location = m_location->isEnabled() ? m_location->text().trimmed() : QString();
    options.atStartup = m_atStartup->isChecked();
    return options;
}

bool RemoteMountDialog::save()
{
    const RemoteMounts::Options current = options();
    if (!RemoteMounts::optionsError(m_profile, current).isEmpty()) {
        return false;
    }
    RemoteMounts::saveOptions(m_group, m_profile.id, current);
    return true;
}

void RemoteMountDialog::updateState()
{
    const Backend backend = Backend(m_backend->currentData().toInt());
    const bool kioFuse = backend == Backend::KioFuse;
    m_readOnly->setEnabled(!kioFuse);
    m_location->setEnabled(!kioFuse);
    RemoteMounts::Options current = options();
    m_location->setPlaceholderText(
        kioFuse ? i18nc("@info:placeholder", "Chosen by KIO FUSE")
                : i18nc("@info:placeholder %1 is a folder", "Default: %1", RemoteMounts::mountPoint(m_profile, {backend, false, {}, false})));

    switch (backend) {
    case Backend::KioFuse:
        m_hint->setText(i18nc("@info",
                              "KIO FUSE mounts the connection through KDE, which asks for passwords and can keep them in KWallet. "
                              "It can't mount read-only, and the folder stays available until KIO FUSE stops."));
        break;
    case Backend::Sshfs:
        m_hint->setText(i18nc("@info",
                              "sshfs signs in with the SSH agent or a key, or asks for a password and new host keys through KDE's SSH password prompt. "
                              "It reads only the parts of files that applications request, so media can stream."));
        break;
    case Backend::Davfs2:
    case Backend::Cifs:
        m_hint->setText(i18nc("@info %1 is an fstab line",
                              "Mounting without administrator rights needs an fstab entry that an administrator adds, such as:\n%1\n"
                              "Plasma Opus never changes fstab or stores passwords for it.",
                              RemoteMounts::fstabExample(m_profile, backend, RemoteMounts::mountPoint(m_profile, current), current.readOnly)));
        break;
    }

    QString problem = RemoteMounts::optionsError(m_profile, current);
    if (problem.isEmpty()) {
        problem = RemoteMounts::unavailableReason(backend, m_find);
    }
    m_status->setText(problem);
    m_status->setVisible(!problem.isEmpty());
    const bool valid = RemoteMounts::optionsError(m_profile, current).isEmpty();
    m_save->setEnabled(valid);
    m_mount->setEnabled(valid && RemoteMounts::unavailableReason(backend, m_find).isEmpty());
}
