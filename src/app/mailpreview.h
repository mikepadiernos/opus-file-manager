// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mailmessage.h"

#include <QUrl>
#include <QWidget>

class QFormLayout;
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

    /// Shows @p message, read from the local file @p source.
    void open(const MailMessage::Message &message, const QUrl &source);
    void clear();
    QUrl source() const;

    /**
     * Extracts the attachments in @p rows, or all of them when @p rows is
     * empty, into the folder that holds the message.
     */
    void extract(const QList<int> &rows);

Q_SIGNALS:
    /// Emitted when an extraction finishes. @p error is empty on success and
    /// when the user cancelled.
    void extractionFinished(const QString &error);

private:
    void updateExtract();
    void finishExtraction(const QString &status, const QString &error);

    QFormLayout *m_headers = nullptr;
    QLabel *m_notice = nullptr;
    QPlainTextEdit *m_body = nullptr;
    QLabel *m_attachmentsLabel = nullptr;
    QListWidget *m_attachments = nullptr;
    QToolButton *m_extract = nullptr;
    QLabel *m_status = nullptr;
    QUrl m_source;
    QList<MailMessage::Attachment> m_attachmentInfo;
    bool m_extracting = false;
    quint64 m_generation = 0;
};
