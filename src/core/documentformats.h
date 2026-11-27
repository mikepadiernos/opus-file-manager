// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * Formats the preview panel shows through an installed document viewer part,
 * such as Okular's or Calligra's, and fonts, which it shows itself.
 */
namespace DocumentFormats
{
enum class Kind {
    None,
    Pdf,
    Office,
    Ebook,
    Comic,
    DjVu,
    HelpFile,
    Font,
};

/// The format kind of @p mimeType, following MIME aliases and inheritance.
Kind kind(const QString &mimeType);
/// Whether the preview panel shows @p mimeType through a document viewer part.
bool usesDocumentViewer(const QString &mimeType);
/// What to install when no viewer part handles @p kind.
QString missingViewerMessage(Kind kind);
/// Whether the preview panel previews @p kind, as chosen under Preferences › Applications.
bool previewEnabled(Kind kind);
/// Why the preview panel doesn't preview @p kind, and where to turn its previews on.
QString previewDisabledMessage(Kind kind);

/// Fonts larger than this aren't previewed, as Qt loads the whole file.
constexpr qint64 FontPreviewLimit = 64 * 1024 * 1024;
/// Whether Plasma's font installer accepts @p mimeType; web fonts are only previewed.
bool isInstallableFont(const QString &mimeType);
/// The sample text shown when none is set.
QString defaultSampleText();
/// The sample text that font previews show, as set under Preferences › Applications › Ebooks and Fonts.
QString sampleText();
/// Plasma's kfontinst font installer, or an empty string when it isn't in PATH.
QString findFontInstaller();
/// Why fonts can't be installed, or an empty string when they can.
QString fontInstallerUnavailableReason();
/**
 * The arguments that install @p urls with kfontinst: absolute local paths
 * or remote URLs, which it downloads itself.
 */
QStringList fontInstallArguments(const QList<QUrl> &urls);
}
