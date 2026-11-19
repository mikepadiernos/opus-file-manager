// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "draftstore.h"

#include <QWidget>

namespace KTextEditor
{
class Document;
class View;
}
class QLabel;
class QComboBox;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTextEdit;
class QTimer;
class QUrl;

class EditorPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Placement {
        Panel,
        Below,
        Right,
        Maximized,
        Window
    };
    explicit EditorPanel(QWidget *parent = nullptr, const QString &draftDirectory = {});
    bool openUrl(const QUrl &url);
    bool queryClose();
    void applySettings();
    KTextEditor::Document *document() const;

Q_SIGNALS:
    void placementRequested(EditorPanel::Placement placement);
    void hideRequested();

private:
    void updateTitle();
    bool saveDraft();
    void showDrafts();
    void setDraftError(const QString &error);
    void setVisualMode(bool visual);
    void refreshVisual();
    bool syncVisual();
    void updateVisualControls();
    KTextEditor::Document *m_document = nullptr;
    KTextEditor::View *m_view = nullptr;
    QLabel *m_title = nullptr;
    QTextBrowser *m_preview = nullptr;
    QTextEdit *m_visual = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    QPushButton *m_sourceMode = nullptr;
    QPushButton *m_visualMode = nullptr;
    QPushButton *m_visualEnable = nullptr;
    QPushButton *m_visualRetry = nullptr;
    QPushButton *m_visualDiscard = nullptr;
    QWidget *m_visualToolbar = nullptr;
    QLabel *m_visualStatus = nullptr;
    DraftStore m_drafts;
    QString m_draftId;
    QTimer *m_draftTimer = nullptr;
    QLabel *m_draftStatus = nullptr;
    QWidget *m_recovery = nullptr;
    QComboBox *m_draftChoice = nullptr;
    QList<DraftStore::Draft> m_availableDrafts;
    quint64 m_editRevision = 0;
    quint64 m_modeRevision = 0;
    quint64 m_openRevision = 0;
    bool m_opening = false;
    bool m_syncingVisual = false;
    bool m_visualPending = false;
    bool m_visualEditing = false;
};
