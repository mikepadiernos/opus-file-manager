// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataeditdialog.h"

#include "settings.h"

#include <KLocalizedString>

#include <QActionGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace
{
QString label(const QString &tag)
{
    if (tag == QLatin1String("Title")) {
        return i18nc("@label", "Title:");
    }
    if (tag == QLatin1String("Artist")) {
        return i18nc("@label", "Artist:");
    }
    if (tag == QLatin1String("Album")) {
        return i18nc("@label", "Album:");
    }
    if (tag == QLatin1String("ImageDescription")) {
        return i18nc("@label", "Image description:");
    }
    return i18nc("@label", "Copyright:");
}
}

MetadataEditDialog::MetadataEditDialog(const QString &path, QWidget *parent)
    : QDialog(parent)
    , m_path(path)
    , m_options(new QWidget(this))
    , m_fieldsLayout(new QVBoxLayout(m_options))
    , m_status(new QLabel(this))
    , m_changes(new QTreeWidget(this))
    , m_preview(new QPushButton(i18nc("@action:button", "Preview"), this))
    , m_apply(new QPushButton(i18nc("@action:button", "Apply"), this))
    , m_placementActions(new QActionGroup(this))
{
    setObjectName(QStringLiteral("metadataEditDialog"));
    setWindowTitle(i18nc("@title:window", "Edit Metadata"));
    resize(750, 450);
    auto *layout = new QVBoxLayout(this);
    auto *placement = new QToolButton(this);
    placement->setObjectName(QStringLiteral("metadataPlacement"));
    placement->setText(i18nc("@action:button", "Placement"));
    placement->setToolTip(i18nc("@info:tooltip", "Metadata editor placement"));
    placement->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(placement);
    m_placementActions->setExclusive(true);
    const auto addPlacement = [this, menu](const QString &name, const QString &text, Placement value) {
        QAction *action = menu->addAction(text);
        action->setObjectName(name);
        action->setCheckable(true);
        action->setData(static_cast<int>(value));
        m_placementActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, value] {
            Q_EMIT placementRequested(value);
        });
    };
    addPlacement(QStringLiteral("metadataPlaceWindow"), i18nc("@action", "Separate Window"), Placement::Window);
    addPlacement(QStringLiteral("metadataPlaceBelow"), i18nc("@action", "Below Panes"), Placement::Below);
    addPlacement(QStringLiteral("metadataPlaceRight"), i18nc("@action", "Right of Panes"), Placement::Right);
    placement->setMenu(menu);
    layout->addWidget(placement, 0, Qt::AlignRight);
    auto *pathLabel = new QLabel(path, this);
    pathLabel->setTextFormat(Qt::PlainText);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);
    m_fieldsLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_options);
    auto *notice = new QLabel(i18nc("@info", "Empty fields remove metadata. Back up the file before editing; changes cannot be undone."), this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    m_status->setObjectName(QStringLiteral("metadataStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_changes->setObjectName(QStringLiteral("metadataChanges"));
    m_changes->setRootIsDecorated(false);
    m_changes->setHeaderLabels({i18nc("@title:column", "Field"), i18nc("@title:column", "Current"), i18nc("@title:column", "New")});
    for (int i = 0; i < 3; ++i) {
        m_changes->header()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
    layout->addWidget(m_changes);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_preview->setObjectName(QStringLiteral("metadataPreview"));
    m_apply->setObjectName(QStringLiteral("metadataApply"));
    m_preview->setEnabled(false);
    m_apply->setEnabled(false);
    buttons->addButton(m_preview, QDialogButtonBox::ActionRole);
    buttons->addButton(m_apply, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &MetadataEditDialog::reject);
    connect(m_preview, &QPushButton::clicked, this, &MetadataEditDialog::preview);
    connect(m_apply, &QPushButton::clicked, this, &MetadataEditDialog::apply);
    connect(&m_reader, &QFutureWatcher<MetadataEditor::Snapshot>::finished, this, [this] {
        m_busy = false;
        m_snapshot = m_reader.result();
        if (!m_snapshot.error.isEmpty()) {
            m_status->setText(m_snapshot.error);
            return;
        }
        auto *form = new QFormLayout;
        const QMap<QString, bool> initiallyVisible = m_snapshot.mimeType == QLatin1String("audio/mpeg")
            ? QMap<QString, bool>{{QStringLiteral("Title"), Settings::metadataAudioTitle()},
                                  {QStringLiteral("Artist"), Settings::metadataAudioArtist()},
                                  {QStringLiteral("Album"), Settings::metadataAudioAlbum()}}
            : QMap<QString, bool>{{QStringLiteral("ImageDescription"), Settings::metadataImageDescription()},
                                  {QStringLiteral("Artist"), Settings::metadataImageArtist()},
                                  {QStringLiteral("Copyright"), Settings::metadataImageCopyright()}};
        for (auto it = m_snapshot.values.cbegin(); it != m_snapshot.values.cend(); ++it) {
            auto *field = new QLineEdit(it.value(), m_options);
            field->setObjectName(QStringLiteral("metadataField_") + it.key());
            m_fields.insert(it.key(), field);
            form->addRow(label(it.key()), field);
            form->setRowVisible(field, initiallyVisible.value(it.key(), true));
            connect(field, &QLineEdit::textChanged, this, &MetadataEditDialog::invalidate);
        }
        m_fieldsLayout->addLayout(form);
        auto *showAll = new QCheckBox(i18nc("@option:check", "Show all editable fields"), m_options);
        showAll->setObjectName(QStringLiteral("metadataShowAllFields"));
        m_fieldsLayout->addWidget(showAll);
        connect(showAll, &QCheckBox::toggled, this, [this, form, initiallyVisible](bool checked) {
            for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) {
                form->setRowVisible(it.value(), checked || initiallyVisible.value(it.key(), true));
            }
        });
        m_options->setEnabled(true);
        m_preview->setEnabled(true);
        m_status->setText(i18nc("@info", "Edit fields and preview changes before applying."));
    });
    connect(&m_writer, &QFutureWatcher<QString>::finished, this, [this] {
        m_busy = false;
        const QString error = m_writer.result();
        m_status->setText(error.isEmpty() ? i18nc("@info", "Metadata updated and verified. Close and reopen to edit again.")
                                          : error + QLatin1Char(' ') + i18nc("@info", "Close and reopen to reload metadata."));
        m_apply->setEnabled(false);
    });
    load();
}

void MetadataEditDialog::setPlacement(Placement placement)
{
    for (QAction *action : m_placementActions->actions()) {
        action->setChecked(action->data().toInt() == static_cast<int>(placement));
    }
}

MetadataEditDialog::~MetadataEditDialog()
{
    m_reader.waitForFinished();
    m_writer.waitForFinished();
}

void MetadataEditDialog::reject()
{
    if (m_busy) {
        m_status->setText(i18nc("@info", "Wait for the metadata operation to finish before closing."));
        return;
    }
    QDialog::reject();
}

void MetadataEditDialog::load()
{
    m_busy = true;
    m_options->setEnabled(false);
    m_status->setText(i18nc("@info", "Reading metadata…"));
    const QString path = m_path;
    m_reader.setFuture(QtConcurrent::run([path] {
        return MetadataEditor::read(path);
    }));
}

void MetadataEditDialog::invalidate()
{
    m_previewValues.clear();
    m_changes->clear();
    m_apply->setEnabled(false);
    m_status->setText(i18nc("@info", "Preview the updated metadata before applying."));
}

void MetadataEditDialog::preview()
{
    m_changes->clear();
    m_previewValues.clear();
    for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) {
        const QString value = it.value()->text();
        m_previewValues.insert(it.key(), value);
        if (value != m_snapshot.values.value(it.key())) {
            new QTreeWidgetItem(m_changes, {label(it.key()), m_snapshot.values.value(it.key()), value});
        }
    }
    m_apply->setEnabled(m_changes->topLevelItemCount() > 0);
    m_status->setText(m_apply->isEnabled() ? i18nc("@info", "Review the changes before applying.") : i18nc("@info", "There are no metadata changes to apply."));
}

void MetadataEditDialog::apply()
{
    if (m_busy || m_previewValues.isEmpty() || !m_apply->isEnabled()) {
        return;
    }
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Apply Metadata Changes"),
                              i18nc("@info", "Write the previewed metadata to this file? This cannot be undone."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    m_busy = true;
    m_options->setEnabled(false);
    m_preview->setEnabled(false);
    m_apply->setEnabled(false);
    m_status->setText(i18nc("@info", "Writing metadata…"));
    const auto snapshot = m_snapshot;
    const auto values = m_previewValues;
    m_writer.setFuture(QtConcurrent::run([snapshot, values] {
        return MetadataEditor::write(snapshot, values);
    }));
}
