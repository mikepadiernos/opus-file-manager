// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QObject>
#include <QUrl>

#include <functional>

class KActionCollection;
class QWidget;

/**
 * Owns the tagging actions in the Tags menu and the dialogs they open, so
 * the main window only supplies its current folder and selection.
 */
class TaggingController : public QObject
{
    Q_OBJECT

public:
    struct Context {
        std::function<QUrl()> folder;
        std::function<KFileItemList()> selection;
        std::function<void(const QString &)> status;
    };

    TaggingController(QWidget *window, KActionCollection *actions, Context context);

    /// Enables the actions for the current folder and selection.
    void updateActions();

    /// The registered library containing the active folder, or an empty string.
    QString currentLibrary() const;

    void createLibrary();
    void removeLibrary();
    /// Indexes or refreshes @p root with progress; returns the dialog.
    QWidget *refreshLibrary(const QString &root);
    /// Opens the review of missing entries in @p root; returns the dialog.
    QWidget *reviewLibrary(const QString &root);
    enum class ManagerPage {
        Tags,
        Groups,
        Fields,
    };
    /// Opens the tag manager for @p root on @p page, reusing an open one; returns the dialog.
    QWidget *manageTags(const QString &root, ManagerPage page = ManagerPage::Tags);
    /**
     * Opens the field editor for @p absolutePaths, or reports why they can't
     * be edited together in the status bar and returns null.
     */
    QWidget *editFields(const QStringList &absolutePaths);
    /**
     * Opens the embedded metadata import for @p absolutePaths, or reports why
     * they can't be imported together in the status bar and returns null.
     */
    QWidget *importMetadata(const QStringList &absolutePaths);
    /// Opens the export of @p root's metadata; returns the dialog.
    QWidget *exportLibrary(const QString &root);
    /// Opens the restore of @p root's metadata from an export; returns the dialog.
    QWidget *restoreLibrary(const QString &root);

Q_SIGNALS:
    /// A library was created, removed or changed.
    void libraryChanged(const QString &root);
    /// The user asked to edit the metadata stored in the file at @p path.
    void editEmbeddedRequested(const QString &path);

private:
    /// The selected items' local paths, or an empty list after reporting why they can't be used.
    QStringList selectedPaths(const QString &none, const QString &remote, const QString &link) const;
    QWidget *m_window;
    KActionCollection *m_actions;
    Context m_context;
};
