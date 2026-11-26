// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "filechooser.h"

#include <KFileItem>

#include <QDialog>
#include <QUrl>

#include <functional>

class FolderView;
class KFilePlacesModel;
class KFilePlacesView;
class KMessageWidget;
class KUrlNavigator;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;

/**
 * The compact chooser that answers XDG portal file chooser requests, with
 * Places, a location bar, a Plasma Opus folder view and a name filter.
 * It offers the request's file filters and choices, and asks before
 * overwriting files when saving.
 */
class ChooserDialog : public QDialog
{
    Q_OBJECT

public:
    /// Asked with the files that already exist; returns whether to replace them.
    using OverwriteConfirmation = std::function<bool(const QList<QUrl> &existing)>;

    explicit ChooserDialog(const FileChooser::Request &request, QWidget *parent = nullptr);
    ~ChooserDialog() override;

    const FileChooser::Request &request() const;
    /// The chosen files, choices and filter, once the dialog was accepted.
    FileChooser::Result chooserResult() const;

    QUrl folder() const;
    void setFolder(const QUrl &url);

    FolderView *view() const;
    /// The file name field, shown when saving one file.
    QLineEdit *nameEdit() const;
    /// The file type filter, shown when the request offers filters.
    QComboBox *filterCombo() const;
    QLineEdit *searchEdit() const;
    /// The message shown when the selection cannot be accepted.
    QString errorText() const;

    void setOverwriteConfirmation(const OverwriteConfirmation &confirmation);

    void accept() override;

private:
    void applyViewFilter();
    void onItemActivated(const KFileItem &item);
    void updateAcceptButton();
    QStringList currentGlobs() const;
    void showError(const QString &message);
    /// Converts URLs such as desktop:/ to local files, or returns false with an error shown.
    bool toLocalUrls(QList<QUrl> *urls);
    bool confirmOverwrite(const QList<QUrl> &targets);
    void finishAccept(const QList<QUrl> &urls);

    FileChooser::Request m_request;
    FileChooser::Result m_result;
    KFilePlacesModel *m_placesModel = nullptr;
    KFilePlacesView *m_places = nullptr;
    KUrlNavigator *m_navigator = nullptr;
    FolderView *m_view = nullptr;
    QLineEdit *m_search = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_filters = nullptr;
    KMessageWidget *m_message = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QList<std::pair<QString, QWidget *>> m_choices;
    OverwriteConfirmation m_confirmOverwrite;
};
