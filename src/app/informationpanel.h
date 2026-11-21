// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>
#include <QHash>
#include <QWidget>

class QFormLayout;
class QLabel;
class QToolButton;

class InformationPanel : public QWidget
{
    Q_OBJECT

public:
    explicit InformationPanel(QWidget *parent = nullptr);
    void setFields(const QStringList &fields);
    void setItem(const KFileItem &item);
    void setTitleVisible(bool visible);

Q_SIGNALS:
    void placementRequested(Qt::DockWidgetArea area);
    void closeRequested();

private:
    void refresh();
    void rebuild();

    QFormLayout *m_layout = nullptr;
    QToolButton *m_panelOptions = nullptr;
    KFileItem m_item;
    QStringList m_fields;
    QHash<QString, QLabel *> m_values;
};
