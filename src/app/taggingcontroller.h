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
    /// Opens the tag manager for @p root, reusing an open one; returns the dialog.
    QWidget *manageTags(const QString &root);

Q_SIGNALS:
    /// A library was created, removed or changed.
    void libraryChanged(const QString &root);

private:
    QWidget *m_window;
    KActionCollection *m_actions;
    Context m_context;
};
