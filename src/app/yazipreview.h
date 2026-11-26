// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "yazimode.h"

#include <KFileItem>

#include <QFrame>
#include <QPointer>

class FolderView;
class InformationPanel;
class PreviewPanel;
class QLabel;
class QProgressBar;
class QStackedWidget;
class QTimer;

/**
 * The preview column of Yazi mode. It follows the focused item, the current
 * index rather than the selection, of a FolderView and shows either a
 * compact PreviewPanel, with the panel's usual file previews and a folder's
 * contents, or the item's file information from an InformationPanel; see
 * setContent(). Updates wait until the focus has rested for UpdateDelay, and
 * starting a new preview cancels the work of the previous one. A slim busy
 * indicator shows while a preview or listing loads. While hidden, the
 * preview is cleared and stops loading; it catches up when shown again.
 *
 * The widget takes keyboard focus so Tab reaches the column even when it only
 * shows a message; Escape returns focus to the folder view.
 */
class YaziPreview : public QFrame
{
    Q_OBJECT

public:
    /// How long, in milliseconds, the focus must rest on an item before it is previewed.
    static constexpr int UpdateDelay = 120;
    /// The InformationPanel fields the information page shows, when the item has them.
    static QStringList informationFields();

    explicit YaziPreview(QWidget *parent = nullptr);
    ~YaziPreview() override;

    /// Follows the focused item of @p view; nullptr stops following and clears the preview.
    void setView(FolderView *view);
    FolderView *view() const;
    /// The item the preview shows, or a null item when it shows none.
    KFileItem item() const;
    PreviewPanel *panel() const;
    InformationPanel *information() const;
    /// Shows the focused item's preview or its file information; switching stops the preview's work.
    void setContent(YaziMode::PreviewContent content);
    YaziMode::PreviewContent content() const;
    /// Whether a focused folder's contents list hidden files.
    void setShowHiddenFiles(bool show);
    /// Previews the focused item now instead of after UpdateDelay.
    void updateNow();
    bool isUpdatePending() const;
    /// The indicator that shows while the preview loads.
    QProgressBar *busyIndicator() const;

Q_SIGNALS:
    /// The preview switched to @p item, which is null when nothing is previewed.
    void itemChanged(const KFileItem &item);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void scheduleUpdate();
    void reset();
    void updateBusyIndicator();
    KFileItem focusedItem() const;

    QPointer<FolderView> m_view;
    QProgressBar *m_busy = nullptr;
    QStackedWidget *m_pages = nullptr;
    PreviewPanel *m_panel = nullptr;
    QWidget *m_informationPage = nullptr;
    QLabel *m_informationTitle = nullptr;
    InformationPanel *m_information = nullptr;
    QTimer *m_timer = nullptr;
    KFileItem m_item;
    YaziMode::PreviewContent m_content = YaziMode::PreviewContent::Preview;
    bool m_shown = false;
};
