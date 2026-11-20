// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
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

private:
    QVBoxLayout *m_layout;
    QLabel *m_status;
    QTabWidget *m_tabs;
    QList<QPointer<KParts::ReadOnlyPart>> m_parts;
};
