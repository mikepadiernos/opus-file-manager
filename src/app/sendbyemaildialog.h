// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mailhandoff.h"

#include <KFileItem>

#include <QDialog>
#include <QPointer>

#include <memory>

class KJob;
class KMessageWidget;
class QCheckBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTemporaryDir;

/// Opens the user's preferred mail client with the selected files attached.
/// The dialog lists what will be attached and why other items cannot be,
/// prepares private local copies of remote files only with consent, lets
/// the files be dragged into a composer, and never sends mail.
class SendByEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendByEmailDialog(const KFileItemList &items, QWidget *parent = nullptr);
    ~SendByEmailDialog() override;

    /// Opens the composer with the local files and any prepared copies.
    void compose();

    /// The private folder holding prepared copies of remote files, or an
    /// empty string. Once the copies are attached or dragged out, the folder
    /// is kept until Plasma Opus quits; otherwise it is removed with the
    /// dialog, or when preparing the copies fails or is stopped.
    QString copiesPath() const;

    /// The file URLs a drag from the list of @p items carries.
    QList<QUrl> dragUrls(const QList<QListWidgetItem *> &items);

Q_SIGNALS:
    /// Emitted when every consented copy of a remote file is ready.
    void copiesPrepared();
    /// Emitted when the mail client was asked to open a composer, or the
    /// request failed. @p error is empty on success.
    void composeFinished(const QString &error);

private:
    bool copiesRemoteFiles() const;
    bool copiesReady() const;
    void consentChanged(bool checked);
    void prepareCopies();
    void prepareNextCopy();
    void copyFinished(KJob *job);
    void cancelCopies();
    void discardCopies();
    void keepCopies();
    void updateState();
    void launch(const QList<QUrl> &attachments);
    void launchThroughPortal(const QList<QUrl> &attachments);
    void finish(const QString &error, int attached);
    void showMessage(int type, const QString &text);

    MailHandoff::Selection m_selection;
    std::optional<MailHandoff::Client> m_client;
    KMessageWidget *m_message = nullptr;
    QLabel *m_summary = nullptr;
    QListWidget *m_items = nullptr;
    QList<QListWidgetItem *> m_remoteItems;
    QCheckBox *m_prepareCopies = nullptr;
    QLabel *m_clientLabel = nullptr;
    QPushButton *m_compose = nullptr;
    QPushButton *m_cancel = nullptr;
    std::unique_ptr<QTemporaryDir> m_copies;
    QString m_copiesPath;
    QList<QUrl> m_prepared;
    QPointer<KJob> m_copyJob;
    bool m_launching = false;
    bool m_composed = false;
};
