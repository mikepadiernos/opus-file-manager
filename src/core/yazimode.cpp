// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfigGroup>
#include <KIO/Global>

#include <QJsonObject>
#include <QVarLengthArray>

#include <algorithm>

namespace
{
constexpr auto SessionKey = "YaziMode";
constexpr auto WorkspaceKey = "yaziMode";
constexpr auto SessionPreviewKey = "YaziPreview";
constexpr auto WorkspacePreviewKey = "yaziPreview";
}

bool YaziMode::readSession(const KConfigGroup &group)
{
    return group.readEntry(SessionKey, false);
}

void YaziMode::writeSession(KConfigGroup &group, bool enabled)
{
    group.writeEntry(SessionKey, enabled);
}

int YaziMode::readSessionPreview(const KConfigGroup &group)
{
    if (!group.hasKey(SessionPreviewKey)) {
        return -1;
    }
    return group.readEntry(SessionPreviewKey, true) ? 1 : 0;
}

void YaziMode::writeSessionPreview(KConfigGroup &group, int preview)
{
    if (preview < 0) {
        group.deleteEntry(SessionPreviewKey);
    } else {
        group.writeEntry(SessionPreviewKey, preview != 0);
    }
}

void YaziMode::writeWorkspacePreview(QJsonObject &object, int preview)
{
    if (preview >= 0) {
        object.insert(QLatin1String(WorkspacePreviewKey), preview != 0);
    }
}

bool YaziMode::readWorkspacePreview(const QJsonObject &object, int &preview)
{
    const auto value = object.value(QLatin1String(WorkspacePreviewKey));
    if (value.isUndefined()) {
        preview = -1;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    preview = value.toBool() ? 1 : 0;
    return true;
}

void YaziMode::writeWorkspace(QJsonObject &object, bool enabled)
{
    object.insert(QLatin1String(WorkspaceKey), enabled);
}

bool YaziMode::readWorkspace(const QJsonObject &object, bool &enabled)
{
    const auto value = object.value(QLatin1String(WorkspaceKey));
    if (value.isUndefined()) {
        enabled = false;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    enabled = value.toBool();
    return true;
}

QUrl YaziMode::parentFolder(const QUrl &current)
{
    if (!current.isValid() || current.isEmpty()) {
        return {};
    }
    const QUrl parent = KIO::upUrl(current);
    if (!parent.isValid() || parent.isEmpty() || parent.adjusted(QUrl::StripTrailingSlash) == current.adjusted(QUrl::StripTrailingSlash)) {
        return {};
    }
    return parent;
}

YaziMode::ColumnWidths YaziMode::columnWidths(int available, bool preview, const Ratios &ratios, int handleWidth)
{
    ColumnWidths widths;
    if (available <= 0) {
        return widths;
    }
    const int handle = qMax(0, handleWidth);
    const bool showPreview = preview && available >= ThreeColumnWidth + 2 * handle;
    const bool showParent = showPreview || available >= TwoColumnWidth + handle;
    if (!showParent) {
        widths.current = available;
        return widths;
    }

    struct Column {
        int *width;
        qint64 weight;
        int minimum;
    };
    QVarLengthArray<Column, 3> pending{{&widths.parent, qMax(1, ratios.parent), MinimumParentWidth},
                                       {&widths.current, qMax(1, ratios.current), MinimumCurrentWidth}};
    if (showPreview) {
        pending.append({&widths.preview, qMax(1, ratios.preview), MinimumPreviewWidth});
    }
    int remaining = available - handle * int(pending.size() - 1);
    // Columns whose share is below their minimum get the minimum, and the
    // others share what is left. The thresholds above leave room for every
    // minimum, so at least one column always keeps its proportional share.
    for (bool clamped = true; clamped;) {
        clamped = false;
        qint64 total = 0;
        for (const Column &column : pending) {
            total += column.weight;
        }
        for (auto it = pending.begin(); it != pending.end(); ++it) {
            if (qint64(remaining) * it->weight / total < it->minimum) {
                *it->width = it->minimum;
                remaining -= it->minimum;
                pending.erase(it);
                clamped = true;
                break;
            }
        }
    }
    qint64 total = 0;
    for (const Column &column : pending) {
        total += column.weight;
    }
    int assigned = 0;
    for (const Column &column : pending) {
        *column.width = int(qint64(remaining) * column.weight / total);
        assigned += *column.width;
    }
    // Rounding leftovers go to the current column when it isn't at its minimum.
    const bool currentPending = std::any_of(pending.cbegin(), pending.cend(), [&widths](const Column &column) {
        return column.width == &widths.current;
    });
    *(currentPending ? &widths.current : pending.last().width) += remaining - assigned;
    return widths;
}
