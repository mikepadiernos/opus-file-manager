// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpanel.h"

#include "settings.h"

#include <KLocalizedString>
#include <KParts/PartLoader>
#include <KParts/ReadOnlyPart>
#include <KPluginMetaData>
#include <kde_terminal_interface.h>

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_status(new QLabel(this))
    , m_tabs(new QTabWidget(this))
{
    setObjectName(QStringLiteral("terminalPanel"));
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_status->setObjectName(QStringLiteral("terminalPanelStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_layout->addWidget(m_status);
    m_tabs->setObjectName(QStringLiteral("terminalSessions"));
    m_tabs->setTabsClosable(true);
    m_layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *widget = m_tabs->widget(index);
        for (const auto &part : std::as_const(m_parts)) {
            if (part && part->widget() == widget) {
                if (QMessageBox::question(this,
                                          i18nc("@title:window", "Close Terminal Session"),
                                          i18nc("@info", "Close this terminal session? Running commands and unsaved input may be lost."),
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No)
                    == QMessageBox::Yes) {
                    delete part.data();
                }
                return;
            }
        }
    });
}

TerminalPanel::~TerminalPanel()
{
    for (const auto &part : std::as_const(m_parts)) {
        if (part) {
            part->disconnect(this);
            delete part.data();
        }
    }
}

void TerminalPanel::start(const QUrl &folder)
{
    if (Settings::terminalFollowActiveFolder()) {
        follow(folder);
        return;
    }
    if (!m_parts.isEmpty()) {
        m_status->setText(i18nc("@info", "Pane navigation does not change the terminal directory."));
        return;
    }
    const QUrl initial = folder.isLocalFile() ? folder : QUrl::fromLocalFile(QDir::homePath());
    follow(initial);
    if (!folder.isLocalFile() && !m_parts.isEmpty()) {
        m_status->setText(i18nc("@info", "Remote folders cannot be used as a terminal directory; starting in the home folder."));
    }
}

void TerminalPanel::follow(const QUrl &folder)
{
    const QFileInfo info(folder.toLocalFile());
    if (!folder.isLocalFile() || !info.isAbsolute() || !info.isDir()) {
        m_status->setText(i18nc("@info", "The active folder is not an existing local folder. The terminal has not changed."));
        return;
    }
    const QString path = QDir::cleanPath(info.absoluteFilePath());
    for (const auto &part : std::as_const(m_parts)) {
        if (!part) {
            continue;
        }
        auto *terminal = qobject_cast<TerminalInterface *>(part.data());
        const QString workingDirectory = terminal ? terminal->currentWorkingDirectory() : QString();
        const QString sessionPath = QFileInfo(workingDirectory).canonicalFilePath();
        if (!workingDirectory.isEmpty() && !sessionPath.isEmpty() && sessionPath == info.canonicalFilePath()) {
            m_tabs->setCurrentWidget(part->widget());
            m_status->setText(i18nc("@info", "Terminal sessions remain open until their tabs are closed."));
            return;
        }
    }
    const KPluginMetaData metadata = KPluginMetaData::findPluginById(QStringLiteral("kf6/parts"), QStringLiteral("konsolepart"));
    if (!metadata.isValid()) {
        m_status->setText(i18nc("@info", "The Konsole terminal component is not installed."));
        return;
    }
    const auto result = KParts::PartLoader::instantiatePart<KParts::ReadOnlyPart>(metadata, this, this);
    if (!result) {
        m_status->setText(i18nc("@info", "Could not load the Konsole terminal component: %1", result.errorString));
        return;
    }
    auto *terminal = qobject_cast<TerminalInterface *>(result.plugin);
    if (!terminal || !result.plugin->widget()) {
        delete result.plugin;
        m_status->setText(i18nc("@info", "The Konsole component does not provide a terminal."));
        return;
    }
    auto *part = result.plugin;
    m_parts.append(part);
    const QPointer<QWidget> widget = part->widget();
    connect(part, &QObject::destroyed, this, [this, widget] {
        if (widget) {
            const int index = m_tabs->indexOf(widget);
            if (index >= 0) {
                m_tabs->removeTab(index);
            }
        }
        m_parts.removeAll(QPointer<KParts::ReadOnlyPart>());
        m_status->setText(i18nc("@info", "Terminal session ended. Select another tab or hide and show the terminal to start another."));
    });
    const int index = m_tabs->addTab(part->widget(), info.fileName().isEmpty() ? path : info.fileName());
    m_tabs->setTabToolTip(index, i18nc("@info:tooltip", "Session started in %1", path));
    m_tabs->setCurrentIndex(index);
    m_status->setText(Settings::terminalFollowActiveFolder()
                          ? i18nc("@info", "Terminal sessions remain open until their tabs are closed.")
                          : i18nc("@info", "The terminal starts in the active folder. Pane navigation does not change its directory."));
    terminal->showShellInDir(path);
}
