// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "embeddedmetadata.h"
#include "metadataimport.h"
#include "taglibrary.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QHash>
#include <QList>

class KMessageWidget;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QTreeWidget;

/**
 * Imports the embedded EXIF and audio metadata of items in one library as
 * tags or field values. The files are read in the background; each property
 * found is mapped to tags, a field or nothing, and the import is previewed
 * before anything changes. Only the library document is written.
 */
class MetadataImportDialog : public QDialog
{
    Q_OBJECT

public:
    /// Imports from the items at @p absolutePaths, which must all be in the library at @p root.
    MetadataImportDialog(const QString &root, const QStringList &absolutePaths, QWidget *parent = nullptr);
    ~MetadataImportDialog() override;

    /// Whether the files' metadata has been read.
    bool isReady() const;
    /// The mapping for property @p key, or null when no item has it.
    QComboBox *mapping(const QString &key) const;
    /// Previews the import with the current mappings; returns false when it can't be made.
    bool preview();
    /// Applies the previewed import; on success emits libraryChanged() and accepts the dialog.
    bool import();
    const MetadataImport::Plan &plan() const;
    MetadataImport::Result importResult() const;

Q_SIGNALS:
    /// The files' metadata has been read and the mappings are shown.
    void ready();
    void libraryChanged(const QString &root);
    /// The user asked to edit the metadata stored in the file at @p path.
    void editEmbeddedRequested(const QString &path);

private:
    void showReadings(const QList<EmbeddedMetadata::Reading> &readings);
    void addMapping(const QString &key, const QStringList &examples);
    void invalidatePreview();
    QString describe(const MetadataImport::Change &change) const;

    QString m_root;
    QStringList m_paths;
    TagLibrary::Library m_library;
    QList<EmbeddedMetadata::Reading> m_readings;
    QHash<QString, QComboBox *> m_mappings;
    MetadataImport::Plan m_plan;
    MetadataImport::Result m_result;
    bool m_ready = false;
    bool m_previewed = false;
    QLabel *m_intro;
    QFormLayout *m_form;
    QCheckBox *m_replace;
    QPushButton *m_preview;
    QPushButton *m_editEmbedded;
    QLabel *m_editEmbeddedNote;
    QTreeWidget *m_changes;
    QLabel *m_summary;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
    QPushButton *m_import;
    QFutureWatcher<QList<EmbeddedMetadata::Reading>> *m_watcher;
};
