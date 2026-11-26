// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;
class QVBoxLayout;

/**
 * Previews a font file with editable sample text at several sizes, and
 * installs it through Plasma's font installer. The font is registered with
 * the application only while it is shown.
 */
class FontPreview : public QWidget
{
    Q_OBJECT

public:
    explicit FontPreview(QWidget *parent = nullptr);
    ~FontPreview() override;

    /**
     * Shows the font in the local file @p path.
     * @param source the file's original location, which the installer is given
     * @return why the font can't be shown, or an empty string
     */
    QString open(const QString &path, const QUrl &source, const QString &mimeType);
    void clear();
    QString family() const;
    QString styleName() const;
    QString sampleText() const;

private:
    void updateSamples();
    void install();

    QLabel *m_name = nullptr;
    QLineEdit *m_sample = nullptr;
    QVBoxLayout *m_samples = nullptr;
    QToolButton *m_install = nullptr;
    QLabel *m_status = nullptr;
    int m_fontId = -1;
    QString m_family;
    QString m_styleName;
    QUrl m_source;
};
