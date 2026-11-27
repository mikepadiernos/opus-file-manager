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
class QPushButton;
class QTemporaryDir;

/// Opens the user's preferred mail client with the selected files attached.
/// The dialog lists what will be attached and why other items cannot be,
/// prepares private local copies of remote files only with consent, and
/// never sends mail.
class SendByEmailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendByEmailDialog(const KFileItemList &items, QWidget *parent = nullptr);
    ~SendByEmailDialog() override;

    /// Prepares any consented remote copies, then opens the composer.
    void compose();

    /// The private folder holding prepared copies of remote files, or an
    /// empty string. It is kept until Plasma Opus quits once the composer
    /// opens, and removed if preparing the copies fails or is stopped.
    QString copiesPath() const;

Q_SIGNALS:
    /// Emitted when the mail client was asked to open a composer, or the
    /// request failed. @p error is empty on success.
    void composeFinished(const QString &error);

private:
    bool copiesRemoteFiles() const;
    void discardCopies();
    void updateState();
    void prepareNextCopy();
    void copyFinished(KJob *job);
    void cancelCopies();
    void launch(const QList<QUrl> &attachments);
    void launchThroughPortal(const QList<QUrl> &attachments);
    void finish(const QString &error, int attached);
    void showMessage(int type, const QString &text);

    MailHandoff::Selection m_selection;
    std::optional<MailHandoff::Client> m_client;
    KMessageWidget *m_message = nullptr;
    QLabel *m_summary = nullptr;
    QListWidget *m_items = nullptr;
    QCheckBox *m_prepareCopies = nullptr;
    QLabel *m_clientLabel = nullptr;
    QPushButton *m_compose = nullptr;
    QPushButton *m_cancel = nullptr;
    std::unique_ptr<QTemporaryDir> m_copies;
    QString m_copiesPath;
    QList<QUrl> m_prepared;
    QPointer<KJob> m_copyJob;
    bool m_busy = false;
    bool m_composed = false;
};
