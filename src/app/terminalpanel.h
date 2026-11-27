// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "terminalproviders.h"

#include <QPointer>
#include <QRegion>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QWidget>

namespace KParts
{
class ReadOnlyPart;
}
class QLabel;
class QMenu;
class QTabWidget;
class QVBoxLayout;

class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel() override;
    void start(const QUrl &folder);
    void follow(const QUrl &folder);
    void setTitleVisible(bool visible);
    bool hasSessions() const;
    int sessionCount() const;
    /// The configured provider whose terminal component new sessions load.
    static TerminalProviders::Provider embeddedProvider();
    /// Stores the provider for new sessions; open sessions keep their component.
    void setEmbeddedProvider(TerminalProviders::Provider provider);
    /**
     * Opens Atuin's interactive history search in the session that the
     * AtuinSession setting chooses when its shell is idle, or reports why it
     * can't. A new session starts in @p folder when it is an existing local
     * folder, otherwise in the current session's folder or the home folder.
     * @p sessionIsNew tells that the current session was just opened for the
     * search, so it is used rather than another new one.
     */
    void searchHistory(const QUrl &folder = {}, bool sessionIsNew = false);
    /**
     * Runs @p program with @p arguments, without a shell, in a new session tab
     * started in the local @p folder. The tab closes when the program exits.
     * @return false, after reporting why, when the session can't be opened
     */
    bool openProgram(const QString &program, const QStringList &arguments, const QString &folder, const QString &title);

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
    void sendHistorySearch(const QPointer<KParts::ReadOnlyPart> &part, int attempt, bool sessionIsNew, const QString &folder);
    /// Opens a history search in a new shell session in @p folder.
    void searchHistoryInNewSession(const QString &folder);
    /// The folder for a new history search session: @p folder, the current session's folder or home.
    QString historySessionFolder(const QUrl &folder) const;
    /// Opens a new shell session in the local folder @p path, or reports why it can't.
    KParts::ReadOnlyPart *openShell(const QString &path);
    /// Loads the configured terminal component into a new current tab, or reports why it can't.
    KParts::ReadOnlyPart *createSession(const QString &title, const QString &toolTip);

    QVBoxLayout *m_layout;
    QLabel *m_status;
    QLabel *m_title;
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
