// SPDX-License-Identifier: GPL-3.0-or-later

#include "mailpreview.h"

#include "fileoperations.h"
#include "localizeddates.h"
#include "localizednumbers.h"
#include "symbolicicon.h"

#include <KIO/Global>
#include <KJob>
#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTemporaryDir>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <memory>

namespace
{
struct Extraction {
    QList<QUrl> files;
    QString error;
};

QString headerLabel(const QString &name)
{
    if (name == QLatin1String("From")) {
        return i18nc("@label mail header", "From:");
    }
    if (name == QLatin1String("To")) {
        return i18nc("@label mail header", "To:");
    }
    if (name == QLatin1String("Cc")) {
        return i18nc("@label mail header", "Cc:");
    }
    if (name == QLatin1String("Date")) {
        return i18nc("@label mail header", "Date:");
    }
    return i18nc("@label mail header", "Subject:");
}

QLabel *plainLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}

/// Decodes the attachments again from the file, so a message that changed
/// after it was previewed is reported instead of extracting the wrong data.
Extraction writeAttachments(const QString &path, const QList<int> &indexes, const QStringList &names, const QString &folder)
{
    Extraction result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MailMessage::FileLimit) {
        result.error = file.error() != QFileDevice::NoError ? file.errorString() : i18nc("@info", "The message is too large.");
        return result;
    }
    const QByteArray data = file.readAll();
    for (int i = 0; i < indexes.size(); ++i) {
        const auto content = MailMessage::attachmentData(data, indexes.at(i), names.at(i));
        if (!content) {
            result.error = i18nc("@info", "The message changed after it was previewed. Select it again, then extract the attachments.");
            return result;
        }
        QFile out(QDir(folder).filePath(names.at(i)));
        if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly) || out.write(*content) != content->size() || !out.flush()) {
            result.error = out.errorString();
            return result;
        }
        result.files.append(QUrl::fromLocalFile(out.fileName()));
    }
    return result;
}
}

MailPreview::MailPreview(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_headers = new QFormLayout;
    m_headers->setObjectName(QStringLiteral("mailPreviewHeaders"));
    m_headers->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_headers->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->addLayout(m_headers);

    m_notice = new QLabel(this);
    m_notice->setObjectName(QStringLiteral("mailPreviewNotice"));
    m_notice->setTextFormat(Qt::PlainText);
    m_notice->setWordWrap(true);
    m_notice->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_notice->hide();
    layout->addWidget(m_notice);

    m_body = new QPlainTextEdit(this);
    m_body->setObjectName(QStringLiteral("mailPreviewBody"));
    m_body->setReadOnly(true);
    m_body->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_body->setAccessibleName(i18nc("@info:accessibility", "Message text"));
    layout->addWidget(m_body, 3);

    auto *attachmentsHeader = new QHBoxLayout;
    m_attachmentsLabel = new QLabel(this);
    m_attachmentsLabel->setObjectName(QStringLiteral("mailPreviewAttachmentsLabel"));
    m_attachmentsLabel->setTextFormat(Qt::PlainText);
    attachmentsHeader->addWidget(m_attachmentsLabel, 1);
    m_extract = new QToolButton(this);
    m_extract->setObjectName(QStringLiteral("mailPreviewExtract"));
    m_extract->setIcon(SymbolicIcon::fromTheme(QStringLiteral("archive-extract")));
    m_extract->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_extract->setToolTip(i18nc("@info:tooltip", "Save copies of the attachments in the folder that holds the message"));
    attachmentsHeader->addWidget(m_extract);
    layout->addLayout(attachmentsHeader);

    m_attachments = new QListWidget(this);
    m_attachments->setObjectName(QStringLiteral("mailPreviewAttachments"));
    m_attachments->setAccessibleName(i18nc("@info:accessibility", "Attachments"));
    m_attachments->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_attachments->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    layout->addWidget(m_attachments, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("mailPreviewStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_status->hide();
    layout->addWidget(m_status);

    connect(m_attachments, &QListWidget::itemSelectionChanged, this, &MailPreview::updateExtract);
    connect(m_extract, &QToolButton::clicked, this, [this] {
        QList<int> rows;
        const auto selected = m_attachments->selectedItems();
        for (const QListWidgetItem *item : selected) {
            rows.append(m_attachments->row(item));
        }
        std::sort(rows.begin(), rows.end());
        extract(rows);
    });
    clear();
}

void MailPreview::open(const MailMessage::Message &message, const QUrl &source)
{
    clear();
    m_source = source;
    for (const MailMessage::Header &header : message.headers) {
        QString value = header.value;
        if (header.name == QLatin1String("Date")) {
            const QDateTime date = QDateTime::fromString(value, Qt::RFC2822Date);
            if (date.isValid()) {
                value = LocalizedDates::dateTime(date.toLocalTime());
            }
        }
        QLabel *field = plainLabel(value, this);
        if (header.name == QLatin1String("Subject")) {
            QFont font = field->font();
            font.setBold(true);
            field->setFont(font);
        }
        m_headers->addRow(headerLabel(header.name), field);
    }

    QStringList notices;
    if (message.encrypted) {
        notices << i18nc("@info", "This message is encrypted. Plasma Opus does not decrypt messages; open it in your mail client to read it.");
    }
    if (message.signedMessage) {
        notices << i18nc("@info", "This message is signed. The signature was not verified.");
    }
    if (message.bodyFromHtml) {
        notices << i18nc("@info", "Shown as plain text. Images, links and other remote content were not loaded.");
    }
    if (message.bodyTruncated) {
        notices << i18nc("@info", "The message text is too long to show in full.");
    }
    m_notice->setText(notices.join(QLatin1Char(' ')));
    m_notice->setVisible(!notices.isEmpty());

    m_body->setPlainText(message.body);
    m_body->setPlaceholderText(message.encrypted ? QString() : i18nc("@info", "This message has no text."));

    m_attachmentInfo = message.attachments;
    const QMimeDatabase mimeDatabase;
    for (const MailMessage::Attachment &attachment : message.attachments) {
        const QMimeType mime = mimeDatabase.mimeTypeForName(attachment.mimeType);
        const QString comment = mime.isValid() ? mime.comment() : attachment.mimeType;
        auto *item = new QListWidgetItem(QIcon::fromTheme(mime.iconName(), QIcon::fromTheme(mime.genericIconName())),
                                         i18nc("@item:inlistbox attachment name, its type and size",
                                               "%1 (%2, %3)",
                                               attachment.fileName,
                                               comment,
                                               LocalizedNumbers::shape(KIO::convertSize(attachment.size))),
                                         m_attachments);
        item->setToolTip(attachment.fileName);
    }
    m_attachmentsLabel->setText(message.attachments.isEmpty() ? i18nc("@info", "No attachments")
                                                              : i18ncp("@info", "One attachment", "%1 attachments", message.attachments.size()));
    m_attachments->setVisible(!message.attachments.isEmpty());
    updateExtract();
}

void MailPreview::clear()
{
    ++m_generation;
    m_extracting = false;
    m_source = {};
    while (m_headers->rowCount() > 0) {
        m_headers->removeRow(0);
    }
    m_notice->clear();
    m_notice->hide();
    m_body->clear();
    m_attachments->clear();
    m_attachmentInfo.clear();
    m_attachmentsLabel->clear();
    m_status->clear();
    m_status->hide();
    updateExtract();
}

QUrl MailPreview::source() const
{
    return m_source;
}

void MailPreview::updateExtract()
{
    const bool selected = !m_attachments->selectedItems().isEmpty();
    m_extract->setText(selected ? i18nc("@action:button", "Extract Selected") : i18nc("@action:button", "Extract All"));
    m_extract->setVisible(!m_attachmentInfo.isEmpty());
    m_extract->setEnabled(!m_extracting && !m_attachmentInfo.isEmpty() && m_source.isLocalFile());
}

void MailPreview::extract(const QList<int> &rows)
{
    if (m_extracting || !m_source.isLocalFile() || m_attachmentInfo.isEmpty()) {
        return;
    }
    QList<int> indexes = rows;
    if (indexes.isEmpty()) {
        for (int i = 0; i < m_attachmentInfo.size(); ++i) {
            indexes.append(i);
        }
    }
    QStringList names;
    for (const int index : std::as_const(indexes)) {
        if (index < 0 || index >= m_attachmentInfo.size()) {
            return;
        }
        names.append(m_attachmentInfo.at(index).fileName);
    }
    auto staging = std::make_shared<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/opus-file-manager-attachments-XXXXXX"));
    if (!staging->isValid()) {
        finishExtraction(QString(), i18nc("@info %1 is an error", "Could not prepare the attachments: %1", staging->errorString()));
        return;
    }
    m_extracting = true;
    m_status->setText(i18nc("@info", "Extracting…"));
    m_status->show();
    updateExtract();

    const quint64 generation = m_generation;
    const QUrl destination = m_source.adjusted(QUrl::RemoveFilename);
    auto *watcher = new QFutureWatcher<Extraction>(this);
    connect(watcher, &QFutureWatcher<Extraction>::finished, this, [this, watcher, staging, generation, destination] {
        const Extraction result = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation) {
            return;
        }
        if (!result.error.isEmpty()) {
            finishExtraction(QString(), i18nc("@info %1 is an error", "Could not extract the attachments: %1", result.error));
            return;
        }
        KJob *job = FileOperations::copy(result.files, destination, window());
        if (!job) {
            finishExtraction(QString(), QString());
            return;
        }
        const int count = result.files.size();
        const QString folder = destination.isLocalFile() ? QDir::toNativeSeparators(destination.toLocalFile()) : destination.toDisplayString();
        // The staging folder is removed once KIO has copied the files.
        connect(job, &KJob::result, this, [this, staging, generation, count, folder](KJob *finished) {
            if (generation != m_generation) {
                return;
            }
            if (finished->error() == KJob::KilledJobError || finished->error() == KIO::ERR_USER_CANCELED) {
                finishExtraction(i18nc("@info", "Extraction was cancelled."), QString());
            } else if (finished->error()) {
                finishExtraction(QString(), i18nc("@info %1 is an error", "Could not extract the attachments: %1", finished->errorString()));
            } else {
                finishExtraction(i18ncp("@info %2 is a folder", "Extracted one attachment to %2.", "Extracted %1 attachments to %2.", count, folder),
                                 QString());
            }
        });
    });
    watcher->setFuture(QtConcurrent::run(writeAttachments, m_source.toLocalFile(), indexes, names, staging->path()));
}

void MailPreview::finishExtraction(const QString &status, const QString &error)
{
    m_extracting = false;
    m_status->setText(error.isEmpty() ? status : error);
    m_status->setVisible(!m_status->text().isEmpty());
    updateExtract();
    Q_EMIT extractionFinished(error);
}
