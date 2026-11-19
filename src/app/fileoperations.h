// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QList>
#include <QUrl>

class QWidget;

/**
 * Thin wrappers around KIO jobs. Every job gets the main window as its
 * UI delegate parent so confirmation and rename dialogs are modal to it,
 * and every undoable job is recorded with KIO::FileUndoManager.
 */
namespace FileOperations
{
void copy(const QList<QUrl> &sources, const QUrl &destination, QWidget *window);
void move(const QList<QUrl> &sources, const QUrl &destination, QWidget *window);
void trash(const QList<QUrl> &urls, QWidget *window);
void deletePermanently(const QList<QUrl> &urls, QWidget *window);
void rename(const KFileItemList &items, QWidget *window);

void copyToClipboard(const QList<QUrl> &urls, bool cut);
/// Returns false when the clipboard holds nothing pasteable.
bool paste(const QUrl &destination, QWidget *window);
/// The text KIO suggests for the paste action given the current clipboard.
QString pasteActionText(const KFileItem &destination, bool *enabled);

void open(const KFileItem &item, QWidget *window);
void openTerminal(const QUrl &folder, QWidget *window);
void showProperties(const KFileItemList &items, QWidget *window);
}
