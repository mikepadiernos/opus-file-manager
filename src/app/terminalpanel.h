// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <QRegion>
#include <QWidget>

namespace KParts
{
class ReadOnlyPart;
}
class QLabel;
class QTabWidget;
class QUrl;
class QVBoxLayout;

class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel() override;
    void start(const QUrl &folder);
    void follow(const QUrl &folder);

Q_SIGNALS:
    void blurRegionChanged(const QRegion &region);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void watchGeometry();
    void scheduleBlurUpdate();
    void updateBlur();

    QVBoxLayout *m_layout;
    QLabel *m_status;
    QTabWidget *m_tabs;
    QList<QPointer<KParts::ReadOnlyPart>> m_parts;
    QRegion m_blurRegion;
    bool m_blurUpdatePending = false;
    bool m_warnedBlurInterface = false;
};
