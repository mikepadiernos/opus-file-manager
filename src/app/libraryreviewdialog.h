// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTreeWidget;

/**
 * Lists the missing entries of a tag library and lets the user relink each
 * to a suggested or chosen file, or forget it. Suggestions are only shown;
 * metadata is attached only when the user relinks an entry.
 */
class LibraryReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LibraryReviewDialog(const QString &root, QWidget *parent = nullptr);

    /// Reloads the library and its missing entries.
    void reload();

    /// Relinks the selected missing entry to the selected suggestion.
    bool relinkToSuggestion();
    /// Relinks the selected missing entry to @p path, an absolute path inside the library.
    bool relinkTo(const QString &path);
    /// Forgets the selected missing entry without asking.
    bool removeSelected();
    /// The last error or result shown.
    QString message() const;

Q_SIGNALS:
    void libraryChanged(const QString &root);

private:
    QString selectedEntry() const;
    void showCandidates();
    void updateButtons();
    void browse();
    void confirmRemove();
    bool change(const std::function<bool(TagLibrary::Library &library, QString *error)> &edit, const QString &done);
    void setMessage(const QString &message);

    QString m_root;
    TagLibrary::Library m_library;
    QTreeWidget *m_missing;
    QTreeWidget *m_candidates;
    QPushButton *m_relink;
    QPushButton *m_browse;
    QPushButton *m_remove;
    QLabel *m_status;
    QString m_message;
};
