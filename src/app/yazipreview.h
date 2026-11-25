// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QFrame>
#include <QPointer>

class FolderView;
class PreviewPanel;
class QTimer;

/**
 * The preview column of Yazi mode: a compact PreviewPanel that follows the
 * focused item, the current index rather than the selection, of a FolderView.
 * Files get the panel's usual previews; folders get a summary. Updates wait
 * until the focus has rested for UpdateDelay, and starting a new preview
 * cancels the work of the previous one. While hidden, the preview is cleared
 * and stops loading; it catches up when shown again.
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

    explicit YaziPreview(QWidget *parent = nullptr);
    ~YaziPreview() override;

    /// Follows the focused item of @p view; nullptr stops following and clears the preview.
    void setView(FolderView *view);
    FolderView *view() const;
    /// The item the preview shows, or a null item when it shows none.
    KFileItem item() const;
    PreviewPanel *panel() const;
    /// Previews the focused item now instead of after UpdateDelay.
    void updateNow();
    bool isUpdatePending() const;

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
    KFileItem focusedItem() const;

    QPointer<FolderView> m_view;
    PreviewPanel *m_panel = nullptr;
    QTimer *m_timer = nullptr;
    KFileItem m_item;
    bool m_shown = false;
};
