// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/**
 * The optional app-local Qt Style Sheet. It is applied to Plasma Opus
 * windows, and the dialogs they own, after every other appearance option.
 */
namespace CustomStyleSheet
{
/**
 * Checks @p sheet with StyleSheets::check() and then with Qt's own
 * stylesheet parser.
 * @returns an error message, or an empty string when it can be applied
 */
QString error(const QString &sheet);

/**
 * The stylesheet to apply from the settings: empty when it is turned off,
 * suspended for high contrast or invalid.
 * @param error set to the reason when a turned-on stylesheet is refused
 */
QString configured(QString *error = nullptr);
}
