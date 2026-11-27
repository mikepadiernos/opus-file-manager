// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchconversiondialog.h"

#include "imageconversion.h"
#include "operationjob.h"
#include "settings.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <utility>

namespace
{
class ConversionProgress : public QDialog
{
public:
    using QDialog::QDialog;

protected:
    void reject() override
    {
    }
    void closeEvent(QCloseEvent *event) override
    {
        event->ignore();
    }
};
}

void BatchConversionDialog::run(QWidget *parent, const QList<QUrl> &sources)
{
    if (sources.isEmpty()) {
        return;
    }
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("batchConversionDialog"));
    dialog.setWindowTitle(i18nc("@title:window", "Convert Marked Images"));
    dialog.resize(720, 500);
    auto *format = new QComboBox(&dialog);
    format->setObjectName(QStringLiteral("batchFormat"));
    format->addItem(QStringLiteral("PNG"), QByteArray("png"));
    format->addItem(QStringLiteral("JPEG"), QByteArray("jpeg"));
    format->addItem(QStringLiteral("WebP"), QByteArray("webp"));
    format->setCurrentIndex(Settings::galleryBatchFormat());
    auto *quality = new QSpinBox(&dialog);
    quality->setRange(0, 100);
    quality->setObjectName(QStringLiteral("batchQuality"));
    quality->setValue(Settings::galleryBatchQuality());
    auto *resize = new QCheckBox(i18nc("@option:check", "Fit images within these dimensions"), &dialog);
    resize->setObjectName(QStringLiteral("batchResize"));
    resize->setChecked(Settings::galleryBatchResize());
    auto *width = new QSpinBox(&dialog);
    auto *height = new QSpinBox(&dialog);
    width->setRange(1, 20000);
    height->setRange(1, 20000);
    width->setObjectName(QStringLiteral("batchWidth"));
    height->setObjectName(QStringLiteral("batchHeight"));
    width->setValue(Settings::galleryBatchWidth());
    height->setValue(Settings::galleryBatchHeight());
    width->setEnabled(resize->isChecked());
    height->setEnabled(resize->isChecked());
    QObject::connect(resize, &QCheckBox::toggled, width, &QWidget::setEnabled);
    QObject::connect(resize, &QCheckBox::toggled, height, &QWidget::setEnabled);
    auto *dimensions = new QHBoxLayout;
    dimensions->addWidget(width);
    dimensions->addWidget(new QLabel(QStringLiteral("×"), &dialog));
    dimensions->addWidget(height);
    const QString configuredDirectory = Settings::galleryBatchDestination();
    auto *directory = new QLineEdit(configuredDirectory.isEmpty() ? QFileInfo(sources.first().toLocalFile()).absolutePath() : configuredDirectory, &dialog);
    directory->setObjectName(QStringLiteral("batchDirectory"));
    auto *browse = new QPushButton(i18nc("@action:button", "Browse…"), &dialog);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&] {
        const QString selected = QFileDialog::getExistingDirectory(&dialog, i18nc("@title:window", "Choose Destination"), directory->text());
        if (!selected.isEmpty()) {
            directory->setText(selected);
        }
    });
    auto *destination = new QHBoxLayout;
    destination->addWidget(directory, 1);
    destination->addWidget(browse);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Format:"), format);
    form->addRow(i18nc("@label:spinbox", "Quality:"), quality);
    form->addRow(resize);
    form->addRow(i18nc("@label", "Maximum width and height:"), dimensions);
    form->addRow(i18nc("@label", "Destination:"), destination);
    auto *preview = new QListWidget(&dialog);
    preview->setObjectName(QStringLiteral("batchPreview"));
    auto *error = new QLabel(&dialog);
    error->setWordWrap(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(i18nc("@action:button", "Convert"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    auto *notice =
        new QLabel(i18nc("@info", "Files are read from disk; unsaved edits and metadata are not included. Animated images are converted as still images."),
                   &dialog);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addWidget(new QLabel(i18nc("@label", "Destination preview:"), &dialog));
    layout->addWidget(preview, 1);
    layout->addWidget(error);
    layout->addWidget(buttons);

    ImageConversion::Plan plan;
    const auto update = [&] {
        plan = ImageConversion::plan(sources, directory->text(), format->currentData().toByteArray());
        error->setText(plan.error);
        preview->clear();
        for (const auto &entry : std::as_const(plan.entries)) {
            preview->addItem(
                i18nc("@item:inlistbox source and destination", "%1 → %2", entry.source.toDisplayString(QUrl::PreferLocalFile), entry.destination));
        }
        buttons->button(QDialogButtonBox::Ok)->setEnabled(plan.error.isEmpty() && !plan.entries.isEmpty());
        quality->setEnabled(format->currentData().toByteArray() != "png");
    };
    QObject::connect(directory, &QLineEdit::textChanged, &dialog, update);
    QObject::connect(format, &QComboBox::currentIndexChanged, &dialog, update);
    update();
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QByteArray outputFormat = format->currentData().toByteArray();
    const QSize bounds = resize->isChecked() ? QSize(width->value(), height->value()) : QSize();
    const int outputQuality = quality->value();
    const QList<ImageConversion::Entry> entries = plan.entries;
    ConversionProgress progress(parent);
    progress.setWindowTitle(i18nc("@title:window", "Converting Images"));
    auto *progressLayout = new QVBoxLayout(&progress);
    progressLayout->addWidget(new QLabel(i18nc("@info", "Converting images…"), &progress));
    auto *indicator = new QProgressBar(&progress);
    indicator->setRange(0, 0);
    progressLayout->addWidget(indicator);
    QFutureWatcher<QList<ImageConversion::Failure>> watcher;
    QObject::connect(&watcher, &QFutureWatcherBase::finished, &progress, &QDialog::accept);
    watcher.setFuture(QtConcurrent::run([entries, outputFormat, bounds, outputQuality] {
        return ImageConversion::convert(entries, outputFormat, bounds, outputQuality);
    }));
    QPointer<OperationJob> job = OperationJob::track(new OperationJob(OperationJob::Kind::ImageConversion, i18nc("@title job", "Converting images")));
    job->setDestination(QUrl::fromLocalFile(directory->text()));
    if (!watcher.isFinished()) {
        progress.exec();
    }
    const QList<ImageConversion::Failure> failures = watcher.result();
    if (job) {
        job->finish(failures.isEmpty()
                        ? QString()
                        : i18nc("@info", "%1 of %2 images could not be converted. Details are shown in Plasma Opus.", failures.size(), entries.size()));
    }
    if (failures.isEmpty()) {
        KMessageBox::information(parent, i18np("Converted one image.", "Converted %1 images.", entries.size()));
    } else {
        QStringList messages;
        for (const auto &failure : failures) {
            messages.append(i18nc("@info source, destination and error",
                                  "%1 → %2: %3",
                                  failure.entry.source.toDisplayString(QUrl::PreferLocalFile),
                                  failure.entry.destination,
                                  failure.error));
        }
        KMessageBox::errorList(parent, i18nc("@info", "%1 of %2 images could not be converted.", failures.size(), entries.size()), messages);
    }
}
