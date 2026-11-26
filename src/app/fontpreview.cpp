// SPDX-License-Identifier: GPL-3.0-or-later

#include "fontpreview.h"
#include "documentformats.h"
#include "symbolicicon.h"

#include <KConfigGroup>
#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRawFont>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
const int SampleSizes[] = {10, 14, 20, 28, 40};

KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("FontPreview"));
}
}

FontPreview::FontPreview(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout;
    m_name = new QLabel(this);
    m_name->setObjectName(QStringLiteral("fontPreviewName"));
    m_name->setWordWrap(true);
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont nameFont = m_name->font();
    nameFont.setBold(true);
    m_name->setFont(nameFont);
    header->addWidget(m_name, 1);
    m_install = new QToolButton(this);
    m_install->setObjectName(QStringLiteral("fontPreviewInstall"));
    m_install->setText(i18nc("@action:button", "Install Font…"));
    m_install->setIcon(SymbolicIcon::fromTheme(QStringLiteral("font-install")));
    m_install->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_install, &QToolButton::clicked, this, &FontPreview::install);
    header->addWidget(m_install);
    layout->addLayout(header);

    m_sample = new QLineEdit(this);
    m_sample->setObjectName(QStringLiteral("fontPreviewSample"));
    m_sample->setPlaceholderText(DocumentFormats::defaultSampleText());
    m_sample->setClearButtonEnabled(true);
    m_sample->setAccessibleName(i18nc("@label:textbox", "Sample text"));
    m_sample->setToolTip(i18nc("@info:tooltip", "Type the text to preview in this font"));
    m_sample->setText(stateGroup().readEntry("SampleText", QString()));
    connect(m_sample, &QLineEdit::textChanged, this, [this](const QString &text) {
        KConfigGroup group = stateGroup();
        group.writeEntry("SampleText", text);
        group.sync();
        updateSamples();
    });
    layout->addWidget(m_sample);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *samples = new QWidget(scroll);
    m_samples = new QVBoxLayout(samples);
    for (const int size : SampleSizes) {
        auto *label = new QLabel(samples);
        label->setObjectName(QStringLiteral("fontPreviewSample%1").arg(size));
        label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText);
        label->setProperty("pointSize", size);
        m_samples->addWidget(label);
    }
    m_samples->addStretch();
    scroll->setWidget(samples);
    layout->addWidget(scroll, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("fontPreviewStatus"));
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);
}

FontPreview::~FontPreview()
{
    clear();
}

QString FontPreview::open(const QString &path, const QUrl &source, const QString &mimeType)
{
    clear();
    const QFileInfo info(path);
    if (!info.isFile()) {
        return i18nc("@info", "Could not open this font.");
    }
    if (info.size() > DocumentFormats::FontPreviewLimit) {
        return i18nc("@info", "This font is too large to preview.");
    }
    m_fontId = QFontDatabase::addApplicationFont(path);
    const QStringList families = m_fontId < 0 ? QStringList() : QFontDatabase::applicationFontFamilies(m_fontId);
    if (families.isEmpty()) {
        clear();
        return i18nc("@info", "Could not load this font. It may be damaged or in an unsupported format.");
    }
    m_family = families.first();
    const QRawFont raw(path, 12);
    m_styleName = raw.isValid() ? raw.styleName() : QString();
    m_source = source;
    m_name->setText(m_styleName.isEmpty() ? m_family : i18nc("@title font family and style", "%1 %2", m_family, m_styleName));

    const QString reason = DocumentFormats::isInstallableFont(mimeType) ? DocumentFormats::fontInstallerUnavailableReason()
                                                                        : i18nc("@info:tooltip", "Plasma's font installer does not install this font format.");
    m_install->setEnabled(reason.isEmpty());
    m_install->setToolTip(reason.isEmpty() ? i18nc("@info:tooltip", "Install this font with Plasma's font installer") : reason);
    m_status->hide();
    updateSamples();
    return {};
}

void FontPreview::clear()
{
    if (m_fontId >= 0) {
        QFontDatabase::removeApplicationFont(m_fontId);
        m_fontId = -1;
    }
    m_family.clear();
    m_styleName.clear();
    m_source = {};
    m_name->clear();
    m_status->hide();
    for (int i = 0; i < m_samples->count(); ++i) {
        if (auto *label = qobject_cast<QLabel *>(m_samples->itemAt(i)->widget())) {
            label->clear();
        }
    }
}

QString FontPreview::family() const
{
    return m_family;
}

QString FontPreview::styleName() const
{
    return m_styleName;
}

QString FontPreview::sampleText() const
{
    return m_sample->text().isEmpty() ? DocumentFormats::defaultSampleText() : m_sample->text();
}

void FontPreview::updateSamples()
{
    if (m_family.isEmpty()) {
        return;
    }
    const QString text = sampleText();
    for (int i = 0; i < m_samples->count(); ++i) {
        auto *label = qobject_cast<QLabel *>(m_samples->itemAt(i)->widget());
        if (!label) {
            continue;
        }
        QFont font(m_family);
        if (!m_styleName.isEmpty()) {
            font.setStyleName(m_styleName);
        }
        font.setPointSize(label->property("pointSize").toInt());
        label->setFont(font);
        label->setText(text);
        label->setToolTip(i18nc("@info:tooltip %1 is a font size", "%1 points", label->property("pointSize").toInt()));
    }
}

void FontPreview::install()
{
    const QString installer = DocumentFormats::findFontInstaller();
    if (installer.isEmpty() || m_source.isEmpty()) {
        m_status->setText(DocumentFormats::fontInstallerUnavailableReason());
        m_status->show();
        return;
    }
    auto *job = new KIO::CommandLauncherJob(installer, DocumentFormats::fontInstallArguments({m_source}), this);
    job->setDesktopName(QStringLiteral("org.kde.kfontinst"));
    connect(job, &KJob::result, this, [this](KJob *finished) {
        if (finished->error()) {
            m_status->setText(i18nc("@info %1 is the error", "Could not start the font installer: %1", finished->errorString()));
            m_status->show();
        }
    });
    job->start();
}
