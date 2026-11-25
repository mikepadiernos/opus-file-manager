// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryexportdialog.h"

#include "libraryarchive.h"

#include <KLocalizedString>
#include <KMessageWidget>

#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

LibraryExportDialog::LibraryExportDialog(const QString &root, QWidget *parent)
    : QDialog(parent)
    , m_root(root)
    , m_summary(new QLabel(this))
    , m_destination(new QLineEdit(this))
    , m_destinationStatus(new QLabel(this))
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel, this))
    , m_export(m_buttons->addButton(i18nc("@action:button", "&Export"), QDialogButtonBox::AcceptRole))
{
    setObjectName(QStringLiteral("libraryExportDialog"));
    setWindowTitle(i18nc("@title:window", "Export Library Metadata"));
    auto *intro = new QLabel(xi18nc("@info",
                                    "Export the tags, tag groups, fields and item metadata of the library <filename>%1</filename> to a file. "
                                    "Files in the library aren't changed or copied.",
                                    root),
                             this);
    intro->setWordWrap(true);
    m_summary->setObjectName(QStringLiteral("libraryExportSummary"));
    m_summary->setWordWrap(true);
    m_destination->setObjectName(QStringLiteral("libraryExportDestination"));
    m_destination->setClearButtonEnabled(true);
    auto *browse = new QPushButton(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:button", "&Browse…"), this);
    browse->setObjectName(QStringLiteral("libraryExportBrowse"));
    m_destinationStatus->setObjectName(QStringLiteral("libraryExportDestinationStatus"));
    m_destinationStatus->setWordWrap(true);
    m_error->setObjectName(QStringLiteral("libraryExportError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);
    m_error->hide();
    m_export->setObjectName(QStringLiteral("libraryExportApply"));
    m_export->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_summary);
    auto *row = new QHBoxLayout;
    row->addWidget(m_destination, 1);
    row->addWidget(browse);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "Export to:"), row);
    form->addRow(QString(), m_destinationStatus);
    layout->addLayout(form);
    layout->addWidget(m_error);
    layout->addStretch();
    layout->addWidget(m_buttons);

    QString error;
    m_library = TagLibrary::Library::load(root, &error);
    if (m_library.isNull()) {
        m_error->setText(i18nc("@info", "Could not read the tag library: %1", error));
        m_error->show();
    }
    showSummary();

    QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        folder = QDir::homePath();
    }
    m_destination->setText(QDir(folder).filePath(LibraryArchive::suggestedFileName(root, QDate::currentDate())));

    connect(m_destination, &QLineEdit::textChanged, this, &LibraryExportDialog::updateDestination);
    connect(browse, &QPushButton::clicked, this, &LibraryExportDialog::browse);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &LibraryExportDialog::exportLibrary);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateDestination();
    resize(560, sizeHint().height());
}

QString LibraryExportDialog::destination() const
{
    return m_destination->text().trimmed();
}

void LibraryExportDialog::setDestination(const QString &path)
{
    m_destination->setText(path);
}

void LibraryExportDialog::browse()
{
    const QString path = QFileDialog::getSaveFileName(this,
                                                      i18nc("@title:window", "Export Library Metadata"),
                                                      destination(),
                                                      i18nc("@item:inlistbox file filter", "Library exports (*.json)"));
    if (!path.isEmpty()) {
        m_destination->setText(path);
    }
}

void LibraryExportDialog::showSummary()
{
    if (m_library.isNull()) {
        m_summary->clear();
        return;
    }
    QString text = LibraryArchive::describe(LibraryArchive::summarize(m_library));
    if (!m_library.warnings().isEmpty()) {
        text += QLatin1Char(' ')
            + i18nc("@info", "Problems repaired while reading the library are exported repaired: %1", m_library.warnings().join(QLatin1Char(' ')));
    }
    m_summary->setText(text);
}

void LibraryExportDialog::updateDestination()
{
    bool replaces = false;
    const QString problem = LibraryArchive::checkDestination(destination(), &replaces);
    if (!problem.isEmpty()) {
        m_destinationStatus->setText(problem);
    } else if (replaces) {
        m_destinationStatus->setText(i18nc("@info", "An earlier export at this location will be replaced."));
    } else {
        m_destinationStatus->clear();
    }
    m_export->setEnabled(!m_library.isNull() && problem.isEmpty());
}

bool LibraryExportDialog::exportLibrary()
{
    if (!m_export->isEnabled()) {
        return false;
    }
    QString error;
    const TagLibrary::Library current = TagLibrary::Library::load(m_root, &error);
    if (current.isNull()) {
        m_error->setText(i18nc("@info", "Could not read the tag library: %1", error));
        m_error->show();
        return false;
    }
    if (current.toJson() != m_library.toJson()) {
        // Export only what the summary showed.
        m_library = current;
        showSummary();
        m_error->setMessageType(KMessageWidget::Information);
        m_error->setText(i18nc("@info", "The library changed while this window was open. Review the summary and export again."));
        m_error->show();
        return false;
    }
    if (!LibraryArchive::write(m_library, destination(), &error)) {
        m_error->setMessageType(KMessageWidget::Error);
        m_error->setText(i18nc("@info", "Could not export the library: %1", error));
        m_error->show();
        updateDestination();
        return false;
    }
    Q_EMIT exported(QDir::cleanPath(destination()));
    accept();
    return true;
}
