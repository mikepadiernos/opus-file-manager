// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <KGuiItem>

#include <QString>

class QWidget;

/**
 * The questions asked before creating, removing or moving a library, shared
 * by the Tags menu and Preferences › Tagging › Libraries.
 */
namespace LibraryConfirmations
{
struct Confirmation {
    QString title;
    QString question;
    KGuiItem action;
    /// The KMessageBox "don't ask again" name, or empty to always ask.
    QString dontAskAgainName;
    /// Whether the question is a warning rather than a plain question.
    bool warning = false;
};

/**
 * Asks before registering @p root.
 * @param storage where the document will be kept, from LibraryRegistry::storageFor()
 * @param preferred where the settings ask new documents to be kept
 */
Confirmation create(const QString &root, TagLibrary::Storage storage, TagLibrary::Storage preferred);

/// Asks before unregistering @p root; its document is kept.
Confirmation remove(const QString &root);

/// Asks before moving the document of @p root to @p target storage.
Confirmation moveDocument(const QString &root, TagLibrary::Storage target);

/// Shows @p confirmation over @p parent; returns whether the user accepted.
bool ask(QWidget *parent, const Confirmation &confirmation);
}
