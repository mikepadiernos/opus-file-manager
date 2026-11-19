// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QAbstractItemView;

/**
 * Makes @p view exactly as tall as its rows, without scroll bars, and keeps
 * it so as rows are added, removed, expanded or collapsed. For views stacked
 * in one scrolling column.
 * @param view the list, tree or places view to fit
 */
void fitHeightToContents(QAbstractItemView *view);
