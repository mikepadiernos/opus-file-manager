// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryrestoredialog.h"

#include "settings.h"

#include <KLocalizedString>
#include <KMessageWidget>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
LibraryArchive::BackupOptions backupOptions()
{
    return {Settings::libraryBackupFolder().toLocalFile(), Settings::libraryBackupLimit()};
}
}

LibraryRestoreDialog::LibraryRestoreDialog(const QString &root, QWidget *parent)
    : QDialog(parent)
    , m_root(QDir::cleanPath(root))
    , m_source(new QLineEdit(this))
    , m_preview(new QPushButton(QIcon::fromTheme(QStringLiteral("view-preview")), i18nc("@action:button", "&Preview"), this))
    , m_details(new QLabel(this))
    , m_changes(new QTreeWidget(this))
    , m_summary(new QLabel(this))
    , m_confirm(new QCheckBox(i18nc("@option:check", "&Replace the library's current tags, groups, fields and item metadata"), this))
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel, this))
    , m_restore(m_buttons->addButton(i18nc("@action:button", "&Restore"), QDialogButtonBox::AcceptRole))
{
    setObjectName(QStringLiteral("libraryRestoreDialog"));
    setWindowTitle(i18nc("@title:window", "Restore Library Metadata"));
    auto *intro = new QLabel(xi18nc("@info",
                                    "Restore the tags, tag groups, fields and item metadata of the library <filename>%1</filename> from an export. "
                                    "Preview the restore to see what changes. Files in the library aren't changed.",
                                    m_root),
                             this);
    intro->setWordWrap(true);
    m_source->setObjectName(QStringLiteral("libraryRestoreSource"));
    m_source->setClearButtonEnabled(true);
    auto *browse = new QPushButton(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:button", "&Browse…"), this);
    browse->setObjectName(QStringLiteral("libraryRestoreBrowse"));
    m_preview->setObjectName(QStringLiteral("libraryRestorePreview"));
    m_details->setObjectName(QStringLiteral("libraryRestoreDetails"));
    m_details->setWordWrap(true);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_changes->setObjectName(QStringLiteral("libraryRestoreChanges"));
    m_changes->setHeaderLabels({i18nc("@title:column", "Change"), i18nc("@title:column", "Name"), i18nc("@title:column", "Details")});
    m_changes->setRootIsDecorated(false);
    m_changes->setUniformRowHeights(true);
    m_changes->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_summary->setObjectName(QStringLiteral("libraryRestoreSummary"));
    m_summary->setWordWrap(true);
    m_confirm->setObjectName(QStringLiteral("libraryRestoreConfirm"));
    m_confirm->setToolTip(xi18nc("@info:tooltip",
                                 "A copy of the current library document is kept in <filename>%1</filename>, so it can be restored in turn.",
                                 LibraryArchive::backupDirectory(backupOptions().folder)));
    m_error->setObjectName(QStringLiteral("libraryRestoreError"));
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);
    m_error->hide();
    m_restore->setObjectName(QStringLiteral("libraryRestoreApply"));
    m_restore->setIcon(QIcon::fromTheme(QStringLiteral("document-revert")));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    auto *row = new QHBoxLayout;
    row->addWidget(m_source, 1);
    row->addWidget(browse);
    row->addWidget(m_preview);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "Restore from:"), row);
    layout->addLayout(form);
    layout->addWidget(m_details);
    layout->addWidget(m_changes, 1);
    layout->addWidget(m_summary);
    layout->addWidget(m_confirm);
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    const QString problem = TagLibrary::checkLocation(m_root, &m_readOnly);
    if (!problem.isEmpty() || m_readOnly) {
        m_error->setMessageType(KMessageWidget::Warning);
        m_error->setText(problem.isEmpty() ? i18nc("@info", "The library folder is read-only, so it can't be restored.") : problem);
        m_error->show();
        m_readOnly = true;
    }

    connect(m_source, &QLineEdit::textChanged, this, &LibraryRestoreDialog::invalidatePreview);
    connect(browse, &QPushButton::clicked, this, &LibraryRestoreDialog::browse);
    connect(m_preview, &QPushButton::clicked, this, &LibraryRestoreDialog::preview);
    connect(m_confirm, &QCheckBox::toggled, this, &LibraryRestoreDialog::updateButtons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &LibraryRestoreDialog::restore);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    invalidatePreview();
    resize(720, 560);
}

QString LibraryRestoreDialog::source() const
{
    return m_source->text().trimmed();
}

void LibraryRestoreDialog::setSource(const QString &path)
{
    m_source->setText(path);
}

const LibraryArchive::Plan &LibraryRestoreDialog::plan() const
{
    return m_plan;
}

QString LibraryRestoreDialog::backupPath() const
{
    return m_backup;
}

void LibraryRestoreDialog::browse()
{
    QString folder = source().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : QFileInfo(source()).absolutePath();
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        folder = QDir::homePath();
    }
    const QString path = QFileDialog::getOpenFileName(this,
                                                      i18nc("@title:window", "Restore Library Metadata"),
                                                      folder,
                                                      i18nc("@item:inlistbox file filter", "Library exports (*.json)"));
    if (!path.isEmpty()) {
        m_source->setText(path);
        preview();
    }
}

void LibraryRestoreDialog::invalidatePreview()
{
    m_plan = {};
    m_changes->clear();
    m_details->clear();
    m_confirm->setChecked(false);
    if (!m_readOnly) {
        m_error->hide();
    }
    m_summary->setText(source().isEmpty() ? i18nc("@info", "Choose an export to restore from.")
                                          : i18nc("@info", "Preview the restore to see what will change."));
    updateButtons();
}

void LibraryRestoreDialog::updateButtons()
{
    m_preview->setEnabled(!source().isEmpty());
    m_confirm->setEnabled(m_plan.hasChanges() && !m_readOnly);
    m_restore->setEnabled(m_plan.hasChanges() && !m_readOnly && m_confirm->isChecked());
}

QString LibraryRestoreDialog::describe(const LibraryArchive::Change &change)
{
    using LibraryArchive::Action;
    using LibraryArchive::Kind;
    switch (change.kind) {
    case Kind::Tag:
        return change.action == Action::Add   ? i18nc("@item:intable", "Restore tag")
            : change.action == Action::Remove ? i18nc("@item:intable", "Remove tag")
                                              : i18nc("@item:intable", "Change tag");
    case Kind::Group:
        return change.action == Action::Add   ? i18nc("@item:intable", "Restore group")
            : change.action == Action::Remove ? i18nc("@item:intable", "Remove group")
                                              : i18nc("@item:intable", "Change group");
    case Kind::Field:
        return change.action == Action::Add   ? i18nc("@item:intable", "Restore field")
            : change.action == Action::Remove ? i18nc("@item:intable", "Remove field")
                                              : i18nc("@item:intable", "Change field");
    case Kind::Item:
        return change.action == Action::Add   ? i18nc("@item:intable", "Restore item metadata")
            : change.action == Action::Remove ? i18nc("@item:intable", "Remove item metadata")
                                              : i18nc("@item:intable", "Change item metadata");
    }
    return {};
}

bool LibraryRestoreDialog::preview()
{
    invalidatePreview();
    if (source().isEmpty()) {
        return false;
    }
    QString error;
    m_plan = LibraryArchive::preview(m_root, source(), &error);
    if (m_plan.isNull()) {
        m_error->setMessageType(KMessageWidget::Error);
        m_error->setText(i18nc("@info", "Could not read the export: %1", error));
        m_error->show();
        m_summary->clear();
        updateButtons();
        return false;
    }

    QStringList details;
    const QString date = QLocale().toString(m_plan.exported.toLocalTime(), QLocale::ShortFormat);
    if (m_plan.exportedRoot.isEmpty()) {
        details.append(i18nc("@info", "A copy of a library document, saved on %1.", date));
    } else if (QDir::cleanPath(m_plan.exportedRoot) == m_root) {
        details.append(i18nc("@info", "Exported from this library on %1.", date));
    } else {
        details.append(
            xi18nc("@info", "Exported from <filename>%1</filename> on %2. Items are matched by their path inside the library.", m_plan.exportedRoot, date));
    }
    details.append(m_plan.currentProblem.isEmpty() ? i18nc("@info", "Now: %1", LibraryArchive::describe(m_plan.before))
                                                   : i18nc("@info", "Now: the library document can't be read and will be replaced. %1", m_plan.currentProblem));
    details.append(i18nc("@info", "After restoring: %1", LibraryArchive::describe(m_plan.after)));
    m_details->setText(details.join(QLatin1Char('\n')));

    for (const LibraryArchive::Change &change : std::as_const(m_plan.changes)) {
        auto *item = new QTreeWidgetItem(m_changes, {describe(change), change.name, change.detail});
        item->setToolTip(1, change.name);
        item->setToolTip(2, change.detail);
        item->setData(0, Qt::UserRole, static_cast<int>(change.kind));
        item->setData(1, Qt::UserRole, static_cast<int>(change.action));
    }
    for (int column = 0; column < m_changes->columnCount() - 1; ++column) {
        m_changes->resizeColumnToContents(column);
    }

    QStringList summary;
    if (!m_plan.hasChanges()) {
        summary.append(i18nc("@info", "The library already matches the export, so there is nothing to restore."));
    } else if (m_plan.changes.isEmpty()) {
        summary.append(i18nc("@info", "Only identifiers and index details differ; the tags, fields and item metadata stay the same."));
    }
    if (m_plan.notFound > 0) {
        summary.append(i18ncp("@info",
                              "One item isn't in the library folder and is restored as missing, so you can review it.",
                              "%1 items aren't in the library folder and are restored as missing, so you can review them.",
                              m_plan.notFound));
    }
    if (!m_plan.warnings.isEmpty()) {
        summary.append(i18nc("@info", "Repaired while reading the export: %1", m_plan.warnings.join(QLatin1Char(' '))));
    }
    if (m_readOnly && m_plan.hasChanges()) {
        summary.append(i18nc("@info", "The library folder is read-only, so it can't be restored."));
    }
    m_summary->setText(summary.join(QLatin1Char(' ')));
    updateButtons();
    return true;
}

bool LibraryRestoreDialog::restore()
{
    if (!m_restore->isEnabled()) {
        return false;
    }
    QString error;
    if (!LibraryArchive::restore(m_plan, backupOptions(), &m_backup, &error)) {
        m_error->setMessageType(KMessageWidget::Error);
        m_error->setText(i18nc("@info", "Could not restore the library: %1", error));
        m_error->show();
        m_confirm->setChecked(false);
        return false;
    }
    Q_EMIT libraryChanged(m_root);
    accept();
    return true;
}
