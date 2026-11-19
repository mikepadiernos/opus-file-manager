// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "draftstore.h"

#include <QWidget>

namespace KTextEditor
{
class Document;
}
class QLabel;
class QComboBox;
class QTextBrowser;
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
    KTextEditor::Document *document() const;

Q_SIGNALS:
    void placementRequested(EditorPanel::Placement placement);
    void hideRequested();

private:
    void updateTitle();
    bool saveDraft();
    void showDrafts();
    void setDraftError(const QString &error);
    KTextEditor::Document *m_document = nullptr;
    QLabel *m_title = nullptr;
    QTextBrowser *m_preview = nullptr;
    DraftStore m_drafts;
    QString m_draftId;
    QTimer *m_draftTimer = nullptr;
    QLabel *m_draftStatus = nullptr;
    QWidget *m_recovery = nullptr;
    QComboBox *m_draftChoice = nullptr;
    QList<DraftStore::Draft> m_availableDrafts;
    quint64 m_editRevision = 0;
    bool m_opening = false;
};
