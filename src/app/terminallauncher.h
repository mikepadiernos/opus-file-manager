// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "terminalproviders.h"

#include <functional>

class QString;
class QUrl;
class QWidget;

/**
 * Opens folders in separate terminal applications. Konsole and Kwi start with
 * --workdir, Yakuake receives a new session over D-Bus, and the system
 * default uses KDE's configured terminal.
 */
namespace TerminalLauncher
{
/// Receives failures, and notices such as a Yakuake session opening while its window is hidden.
using Report = std::function<void(const QString &message)>;

/// How long to wait for a Yakuake that was not running to accept sessions.
constexpr int yakuakeStartTimeoutMs = 15000;

void open(TerminalProviders::Provider provider, const QUrl &folder, QWidget *window, const Report &report);
}
