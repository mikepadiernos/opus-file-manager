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
    /// Whether fields without a value for the item are hidden instead of showing "Unavailable".
    void setEmptyFieldsHidden(bool hidden);

Q_SIGNALS:
    void placementRequested(Qt::DockWidgetArea area);
    void closeRequested();

private:
    void refresh();
    /**
     * Shows the tags of an item in a tag library, unless the settings hide
     * them, with inherited tags in the tooltip and, if the settings ask,
     * after the item's own tags; and its library field values.
     */
    void refreshTags();
    void rebuild();

    QFormLayout *m_layout = nullptr;
    QToolButton *m_panelOptions = nullptr;
    QLabel *m_tags = nullptr;
    QLabel *m_libraryFields = nullptr;
    QLabel *m_encryption = nullptr;
    KFileItem m_item;
    QStringList m_fields;
    QHash<QString, QLabel *> m_values;
    bool m_emptyFieldsHidden = false;
};
