// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mailmessage.h"

#include <QUrl>
#include <QWidget>

#include <functional>
#include <memory>

class QFormLayout;
class QTemporaryDir;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QToolButton;

/**
 * Shows a local mail file in the preview panel: its main headers, plain-text
 * body and attachment list. Attachments are extracted only when asked, into
 * the message's folder through KIO, with the normal conflict handling.
 */
class MailPreview : public QWidget
{
    Q_OBJECT

public:
    explicit MailPreview(QWidget *parent = nullptr);
    ~MailPreview() override;

    using Opener = std::function<void(const QUrl &file)>;

    /// Shows @p message, read from the local file @p source.
    void open(const MailMessage::Message &message, const QUrl &source);
    void clear();
    QUrl source() const;

    /**
     * Extracts the attachments in @p rows, or all of them when @p rows is
     * empty, into the folder that holds the message.
     */
    void extract(const QList<int> &rows);

    /**
     * Opens a read-only private copy of the attachment in @p row with its
     * registered application. Programs are never run. The copies are kept
     * while the preview exists, because the application may read them later.
     */
    void openAttachment(int row);

    /// Replaces how prepared copies are opened; used by tests.
    void setOpener(Opener opener);

Q_SIGNALS:
    /// Emitted when an extraction finishes. @p error is empty on success and
    /// when the user cancelled.
    void extractionFinished(const QString &error);
    /// Emitted when opening an attachment finishes. @p error is empty when the
    /// copy was handed to its application.
    void openFinished(const QString &error);

private:
    void updateExtract();
    void finishExtraction(const QString &status, const QString &error);

    QFormLayout *m_headers = nullptr;
    QLabel *m_notice = nullptr;
    QPlainTextEdit *m_body = nullptr;
    QLabel *m_attachmentsLabel = nullptr;
    QListWidget *m_attachments = nullptr;
    QToolButton *m_extract = nullptr;
    QToolButton *m_open = nullptr;
    QLabel *m_status = nullptr;
    QUrl m_source;
    QList<MailMessage::Attachment> m_attachmentInfo;
    bool m_extracting = false;
    bool m_opening = false;
    quint64 m_generation = 0;
    std::unique_ptr<QTemporaryDir> m_openCopies;
    int m_openCount = 0;
    Opener m_opener;
};
