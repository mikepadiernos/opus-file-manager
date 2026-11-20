// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <QRegion>
#include <QSet>
#include <QWidget>

namespace KParts
{
class ReadOnlyPart;
}
class QLabel;
class QMenu;
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
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void watchGeometry();
    void scheduleBlurUpdate();
    void updateBlur();
    void setStatus(const QString &text, bool informational = false);
    void dismissStatus();

    QVBoxLayout *m_layout;
    QLabel *m_status;
    QWidget *m_statusRow;
    QMenu *m_panelMenu;
    QTabWidget *m_tabs;
    QList<QPointer<KParts::ReadOnlyPart>> m_parts;
    QRegion m_blurRegion;
    QSet<QString> m_dismissedInformationalMessages;
    bool m_statusIsInformational = false;
    bool m_blurUpdatePending = false;
    bool m_warnedBlurInterface = false;
    bool m_warnedContextMenuInterface = false;
};
