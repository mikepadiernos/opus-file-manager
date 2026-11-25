// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataimportdialog.h"

#include "metadataeditor.h"
#include "settings.h"
#include "taggingsettings.h"

#include <KLocalizedString>
#include <KMessageWidget>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPromise>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

MetadataImportDialog::MetadataImportDialog(const QString &root, const QStringList &absolutePaths, QWidget *parent)
    : QDialog(parent)
    , m_root(root)
    , m_paths(absolutePaths)
    , m_keys(TaggingSettings::embeddedMetadataKeys())
    , m_savedMappings(TaggingSettings::embeddedMetadataMappings())
    , m_intro(new QLabel(this))
    , m_form(new QFormLayout)
    , m_replace(new QCheckBox(i18nc("@option:check", "&Replace existing field values"), this))
    , m_preview(new QPushButton(QIcon::fromTheme(QStringLiteral("view-preview")), i18nc("@action:button", "&Preview"), this))
    , m_editEmbedded(new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "&Edit Embedded Metadata…"), this))
    , m_editEmbeddedNote(new QLabel(this))
    , m_changes(new QTreeWidget(this))
    , m_summary(new QLabel(this))
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel, this))
    , m_import(m_buttons->addButton(i18nc("@action:button", "&Import"), QDialogButtonBox::AcceptRole))
    , m_watcher(new QFutureWatcher<QList<EmbeddedMetadata::Reading>>(this))
{
    setObjectName(QStringLiteral("metadataImportDialog"));
    setWindowTitle(i18nc("@title:window", "Import Embedded Metadata"));
    m_intro->setObjectName(QStringLiteral("metadataImportIntro"));
    m_intro->setWordWrap(true);
    m_intro->setText(
        i18ncp("@info", "Reading the embedded metadata of the selected item…", "Reading the embedded metadata of %1 items…", absolutePaths.size()));
    m_replace->setObjectName(QStringLiteral("metadataImportReplace"));
    m_replace->setChecked(TaggingSettings::embeddedMetadataPolicy() == MetadataImport::Policy::Replace);
    m_replace->setToolTip(i18nc("@info:tooltip", "When unchecked, fields that already have a different value keep it"));
    m_preview->setObjectName(QStringLiteral("metadataImportPreview"));
    m_preview->setEnabled(false);
    m_editEmbedded->setObjectName(QStringLiteral("metadataImportEditEmbedded"));
    if (Settings::allowEmbeddedMetadataWrites()) {
        m_editEmbedded->setToolTip(i18nc("@info:tooltip", "Change the metadata stored in the file itself, with a preview before it is written"));
    } else {
        const QString off = i18nc("@info", "Writing to files is turned off in Preferences › Tagging › Privacy.");
        m_editEmbedded->setEnabled(false);
        m_editEmbedded->setToolTip(off);
        m_editEmbeddedNote->setText(off);
    }
    m_editEmbedded->hide();
    m_editEmbeddedNote->setObjectName(QStringLiteral("metadataImportEditEmbeddedNote"));
    m_editEmbeddedNote->setWordWrap(true);
    m_editEmbeddedNote->hide();
    m_changes->setObjectName(QStringLiteral("metadataImportChanges"));
    m_changes->setHeaderLabels(
        {i18nc("@title:column", "Item"), i18nc("@title:column", "Property"), i18nc("@title:column", "Value"), i18nc("@title:column", "Change")});
    m_changes->setRootIsDecorated(false);
    m_changes->setUniformRowHeights(true);
    m_changes->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_summary->setObjectName(QStringLiteral("metadataImportSummary"));
    m_summary->setWordWrap(true);
    m_error->setObjectName(QStringLiteral("metadataImportError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);
    m_error->hide();
    m_import->setObjectName(QStringLiteral("metadataImportApply"));
    m_import->setIcon(QIcon::fromTheme(QStringLiteral("document-import")));
    m_import->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_intro);
    auto *mappings = new QGroupBox(i18nc("@title:group", "Import As"), this);
    mappings->setObjectName(QStringLiteral("metadataImportMappings"));
    mappings->setLayout(m_form);
    layout->addWidget(mappings);
    layout->addWidget(m_replace);
    auto *row = new QHBoxLayout;
    row->addWidget(m_preview);
    row->addWidget(m_editEmbedded);
    row->addWidget(m_editEmbeddedNote, 1);
    row->addStretch();
    layout->addLayout(row);
    layout->addWidget(m_changes, 1);
    layout->addWidget(m_summary);
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    QString error;
    m_library = TagLibrary::Library::load(root, &error);
    if (m_library.isNull()) {
        m_error->setText(i18nc("@info", "Could not read the tag library: %1", error));
        m_error->show();
    }

    connect(m_replace, &QCheckBox::toggled, this, &MetadataImportDialog::invalidatePreview);
    connect(m_preview, &QPushButton::clicked, this, &MetadataImportDialog::preview);
    connect(m_editEmbedded, &QPushButton::clicked, this, [this] {
        if (!Settings::allowEmbeddedMetadataWrites()) {
            return;
        }
        Q_EMIT editEmbeddedRequested(m_paths.constFirst());
        reject();
    });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &MetadataImportDialog::import);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_watcher, &QFutureWatcherBase::finished, this, [this] {
        if (!m_watcher->isCanceled() && m_watcher->future().resultCount() > 0) {
            showReadings(m_watcher->result());
        }
    });
    m_watcher->setFuture(QtConcurrent::run(
        [](QPromise<QList<EmbeddedMetadata::Reading>> &promise, const QStringList &paths, const QStringList &keys) {
            QList<EmbeddedMetadata::Reading> readings;
            for (const QString &path : paths) {
                if (promise.isCanceled()) {
                    return;
                }
                readings.append(EmbeddedMetadata::read(path, keys));
            }
            promise.addResult(readings);
        },
        absolutePaths,
        m_keys));
    resize(720, 560);
}

MetadataImportDialog::~MetadataImportDialog()
{
    m_watcher->cancel();
}

bool MetadataImportDialog::isReady() const
{
    return m_ready;
}

QComboBox *MetadataImportDialog::mapping(const QString &key) const
{
    return m_mappings.value(key);
}

const MetadataImport::Plan &MetadataImportDialog::plan() const
{
    return m_plan;
}

MetadataImport::Result MetadataImportDialog::importResult() const
{
    return m_result;
}

void MetadataImportDialog::showReadings(const QList<EmbeddedMetadata::Reading> &readings)
{
    m_readings = readings;
    QHash<QString, QStringList> examples;
    int supported = 0;
    for (const EmbeddedMetadata::Reading &reading : readings) {
        supported += reading.supported ? 1 : 0;
        for (const EmbeddedMetadata::Value &value : reading.values) {
            QStringList &list = examples[value.key];
            if (list.size() < 3 && !list.contains(value.text)) {
                list.append(value.text);
            }
        }
    }
    const QMap<QString, QString> saved = m_library.isNull() ? QMap<QString, QString>() : MetadataImport::resolveMappings(m_library, m_savedMappings, m_keys);
    for (const QString &key : std::as_const(m_keys)) {
        if (examples.contains(key)) {
            addMapping(key, examples.value(key), saved.value(key));
        }
    }
    if (m_mappings.isEmpty()) {
        if (m_keys.isEmpty()) {
            m_intro->setText(i18nc("@info", "Reading embedded metadata is turned off for every property in Preferences › Tagging › Embedded Metadata."));
        } else if (supported == 0) {
            m_intro->setText(i18ncp("@info",
                                    "The selected item has no embedded metadata that can be read. Images and audio files can be imported.",
                                    "None of the %1 selected items has embedded metadata that can be read. Images and audio files can be imported.",
                                    readings.size()));
        } else if (m_keys.size() < EmbeddedMetadata::keys().size()) {
            m_intro->setText(i18nc("@info",
                                   "No supported EXIF or audio properties were found in the selected items. Some properties aren't read because "
                                   "they are turned off in Preferences › Tagging › Embedded Metadata."));
        } else {
            m_intro->setText(i18nc("@info", "No supported EXIF or audio properties were found in the selected items."));
        }
    } else {
        m_intro->setText(i18ncp("@info",
                                "Choose how to import the embedded metadata of the selected item, then preview the changes. The file isn't changed.",
                                "Choose how to import the embedded metadata of %1 selected items, then preview the changes. Files aren't changed.",
                                readings.size()));
    }
    if (readings.size() == 1 && readings.constFirst().supported && MetadataEditor::supports(readings.constFirst().mimeType)) {
        m_editEmbedded->show();
        m_editEmbeddedNote->setVisible(!m_editEmbedded->isEnabled());
    }
    m_ready = true;
    invalidatePreview();
    Q_EMIT ready();
}

void MetadataImportDialog::addMapping(const QString &key, const QStringList &examples, const QString &saved)
{
    auto *combo = new QComboBox(this);
    combo->setObjectName(QStringLiteral("metadataMapping_") + key);
    const QString name = EmbeddedMetadata::displayName(key);
    combo->setAccessibleName(name);
    combo->addItem(i18nc("@item:inlistbox", "Don't import"), QString());
    combo->addItem(QIcon::fromTheme(QStringLiteral("tag")), i18nc("@item:inlistbox", "Tags"), MetadataImport::tagsTarget());
    if (saved == MetadataImport::tagsTarget()) {
        combo->setCurrentIndex(1);
    }
    if (!m_library.isNull()) {
        for (const TagLibrary::Field &field : m_library.fields()) {
            const QString label = field.type == TagLibrary::FieldType::Date ? i18nc("@item:inlistbox field name and type", "Field: %1 (date)", field.name)
                                                                            : i18nc("@item:inlistbox field name", "Field: %1", field.name);
            combo->addItem(label, field.id);
            if (field.id == saved) {
                combo->setCurrentIndex(combo->count() - 1);
            }
        }
        // Without a saved mapping, suggest a field named like the property, such as a "Genre" field for the genre.
        for (int i = 2; saved.isEmpty() && i < combo->count(); ++i) {
            const TagLibrary::Field *field = m_library.field(combo->itemData(i).toString());
            if (field && field->name.compare(name, Qt::CaseInsensitive) == 0) {
                combo->setCurrentIndex(i);
                break;
            }
        }
    }
    combo->setToolTip(i18nc("@info:tooltip", "For example: %1", examples.join(QStringLiteral(", "))));
    connect(combo, &QComboBox::currentIndexChanged, this, &MetadataImportDialog::invalidatePreview);
    m_form->addRow(i18nc("@label:listbox property name", "%1:", name), combo);
    m_mappings.insert(key, combo);
}

void MetadataImportDialog::invalidatePreview()
{
    m_previewed = false;
    m_plan = {};
    m_changes->clear();
    m_import->setEnabled(false);
    bool mapped = false;
    for (const QComboBox *combo : std::as_const(m_mappings)) {
        mapped = mapped || !combo->currentData().toString().isEmpty();
    }
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    m_preview->setEnabled(m_ready && writable && mapped);
    if (!m_ready) {
        m_summary->clear();
    } else if (m_library.isReadOnly()) {
        m_summary->setText(i18nc("@info", "The library is read-only, so nothing can be imported."));
    } else if (m_mappings.isEmpty()) {
        m_summary->clear();
    } else {
        m_summary->setText(mapped ? i18nc("@info", "Preview the import to see what will change.")
                                  : i18nc("@info", "Choose at least one property to import as tags or a field."));
    }
}

bool MetadataImportDialog::preview()
{
    if (!m_ready || !m_preview->isEnabled()) {
        return false;
    }
    QString error;
    const TagLibrary::Library library = TagLibrary::Library::load(m_root, &error);
    if (library.isNull()) {
        m_error->setText(i18nc("@info", "Could not read the tag library: %1", error));
        m_error->show();
        return false;
    }
    m_library = library;
    m_error->hide();
    QMap<QString, QString> mappings;
    for (auto it = m_mappings.cbegin(); it != m_mappings.cend(); ++it) {
        if (const QString target = it.value()->currentData().toString(); !target.isEmpty()) {
            mappings.insert(it.key(), target);
        }
    }
    m_plan = MetadataImport::preview(m_library, m_readings, mappings, m_replace->isChecked() ? MetadataImport::Policy::Replace : MetadataImport::Policy::Keep);
    m_plan.tagColor = TaggingSettings::defaultTagColor();
    m_changes->clear();
    for (const MetadataImport::Change &change : std::as_const(m_plan.changes)) {
        auto *item = new QTreeWidgetItem(m_changes,
                                         {QDir(m_root).relativeFilePath(change.path),
                                          change.key.isEmpty() ? QString() : EmbeddedMetadata::displayName(change.key),
                                          change.value,
                                          describe(change)});
        item->setToolTip(0, change.path);
        item->setToolTip(3, describe(change));
        item->setData(0, Qt::UserRole, static_cast<int>(change.action));
    }
    for (int column = 0; column < m_changes->columnCount() - 1; ++column) {
        m_changes->resizeColumnToContents(column);
    }

    using MetadataImport::Action;
    QStringList parts;
    if (const int n = m_plan.count(Action::AssignTag)) {
        parts.append(i18ncp("@info", "1 tag to assign", "%1 tags to assign", n));
    }
    if (const int n = m_plan.count(Action::CreateTag)) {
        parts.append(i18ncp("@info", "1 new tag to assign", "%1 new tags to assign", n));
    }
    if (const int n = m_plan.count(Action::SetField)) {
        parts.append(i18ncp("@info", "1 value to set", "%1 values to set", n));
    }
    if (const int n = m_plan.count(Action::Unchanged)) {
        parts.append(i18ncp("@info", "1 already imported", "%1 already imported", n));
    }
    if (const int n = m_plan.count(Action::Conflict)) {
        parts.append(i18ncp("@info", "1 conflict skipped", "%1 conflicts skipped", n));
    }
    if (const int n = m_plan.count(Action::Invalid)) {
        parts.append(i18ncp("@info", "1 invalid value skipped", "%1 invalid values skipped", n));
    }
    if (const int n = m_plan.count(Action::Unsupported)) {
        parts.append(i18ncp("@info", "1 item without readable metadata", "%1 items without readable metadata", n));
    }
    if (!m_plan.hasChanges()) {
        parts.prepend(i18nc("@info", "Nothing to import"));
    }
    m_summary->setText(parts.join(i18nc("@info list separator", ", ")) + QLatin1Char('.'));
    m_previewed = true;
    m_import->setEnabled(m_plan.hasChanges());
    return true;
}

bool MetadataImportDialog::import()
{
    if (!m_previewed || !m_plan.hasChanges()) {
        return false;
    }
    QString error;
    if (!MetadataImport::apply(m_plan, &m_result, &error)) {
        m_error->setText(i18nc("@info", "Could not import the metadata: %1", error));
        m_error->show();
        return false;
    }
    saveMappings();
    Q_EMIT libraryChanged(m_root);
    accept();
    return true;
}

void MetadataImportDialog::saveMappings()
{
    // Mappings of properties the selected items don't have are kept.
    MetadataImport::NamedMappings mappings = TaggingSettings::embeddedMetadataMappings();
    for (auto it = m_mappings.cbegin(); it != m_mappings.cend(); ++it) {
        const QString target = it.value()->currentData().toString();
        const TagLibrary::Field *field = target.isEmpty() ? nullptr : m_library.field(target);
        if (target == MetadataImport::tagsTarget()) {
            mappings.insert(it.key(), QString());
        } else if (field) {
            mappings.insert(it.key(), field->name);
        } else {
            mappings.remove(it.key());
        }
    }
    TaggingSettings::setEmbeddedMetadataMappings(mappings);
    Settings::self()->save();
}

QString MetadataImportDialog::describe(const MetadataImport::Change &change) const
{
    using MetadataImport::Action;
    const TagLibrary::Field *field = change.target.isEmpty() ? nullptr : m_library.field(change.target);
    switch (change.action) {
    case Action::AssignTag:
        return i18nc("@info", "Assign the tag “%1”", m_library.displayName(change.target));
    case Action::CreateTag:
        return i18nc("@info", "Create and assign the tag “%1”", change.value);
    case Action::SetField:
        return change.current.isEmpty() ? i18nc("@info", "Set “%1”", field ? field->name : change.target)
                                        : i18nc("@info", "Replace “%2” in “%1”", field ? field->name : change.target, change.current);
    case Action::Unchanged:
        return i18nc("@info", "Already imported");
    case Action::Conflict:
    case Action::Invalid:
    case Action::Unsupported:
        return change.detail;
    }
    return {};
}
