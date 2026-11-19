// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpanel.h"

#include <KLocalizedString>
#include <KParts/PartLoader>
#include <KParts/ReadOnlyPart>
#include <KPluginMetaData>
#include <kde_terminal_interface.h>

#include <QDir>
#include <QLabel>
#include <QUrl>
#include <QVBoxLayout>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_status(new QLabel(this))
{
    setObjectName(QStringLiteral("terminalPanel"));
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_status->setObjectName(QStringLiteral("terminalPanelStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_layout->addWidget(m_status);
}

TerminalPanel::~TerminalPanel()
{
    if (m_part) {
        m_part->disconnect(this);
        delete m_part;
    }
}

void TerminalPanel::start(const QUrl &folder)
{
    if (m_part) {
        return;
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
    m_part = result.plugin;
    connect(m_part, &QObject::destroyed, this, [this] {
        m_status->setText(i18nc("@info", "Terminal session ended. Hide and show the terminal to start another."));
    });
    m_layout->addWidget(m_part->widget());
    if (folder.isLocalFile()) {
        m_status->setText(i18nc("@info", "The terminal starts in the active folder. Pane navigation does not change its directory."));
        terminal->showShellInDir(folder.toLocalFile());
    } else {
        m_status->setText(i18nc("@info", "Remote folders cannot be used as a terminal directory; starting in the home folder."));
        terminal->showShellInDir(QDir::homePath());
    }
}
