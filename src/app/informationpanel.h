// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudassets.h"

#include <KFileItem>
#include <QHash>
#include <QWidget>

#include <optional>

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
    /**
     * Shows what the Nextcloud server reports about @p url in a separate
     * section, or @p status alone, such as "Loading…" or an error, when
     * @p metadata is empty. It is ignored unless @p url is the item shown,
     * and showing another item hides the section until it is set again.
     */
    void setServerMetadata(const QUrl &url, const std::optional<Nextcloud::FileMetadata> &metadata, const QString &status = {});

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
    /// Shows the Nextcloud server's metadata of the item, when it belongs to the item.
    void refreshServerMetadata();

    QFormLayout *m_layout = nullptr;
    QToolButton *m_panelOptions = nullptr;
    QLabel *m_tags = nullptr;
    QLabel *m_libraryFields = nullptr;
    QLabel *m_encryption = nullptr;
    QLabel *m_serverHeading = nullptr;
    QHash<QString, QLabel *> m_serverValues;
    QUrl m_serverUrl;
    std::optional<Nextcloud::FileMetadata> m_serverMetadata;
    QString m_serverStatus;
    KFileItem m_item;
    QStringList m_fields;
    QHash<QString, QLabel *> m_values;
    bool m_emptyFieldsHidden = false;
};
