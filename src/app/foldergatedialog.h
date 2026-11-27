// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QWidget;

/**
 * Window-modal password dialogs for password-gated folders. Each dialog
 * stays open after a wrong password, shows why, and deletes itself when it
 * closes; FolderGates::changed() reports the result.
 */
namespace FolderGateDialog
{
/// Asks for a new password for the local @p folder, after warning that the gate applies only inside Plasma Opus.
void protect(QWidget *parent, const QString &folder);
/// Asks for the password of @p gate and unlocks it.
void unlock(QWidget *parent, const QString &gate);
/// Asks for the current and a new password of @p gate.
void changePassword(QWidget *parent, const QString &gate);
/// Asks for the password of @p gate and removes its protection.
void removeProtection(QWidget *parent, const QString &gate);
}
