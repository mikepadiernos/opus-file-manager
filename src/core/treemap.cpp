// SPDX-License-Identifier: GPL-3.0-or-later

#include "treemap.h"

#include <algorithm>
#include <vector>

QList<QRectF> Treemap::layout(const QList<quint64> &weights, const QRectF &bounds)
{
    QList<QRectF> result(weights.size());
    if (bounds.isEmpty()) {
        return result;
    }

    struct Item {
        qsizetype index;
        quint64 weight;
    };
    std::vector<Item> items;
    for (qsizetype i = 0; i < weights.size(); ++i) {
        if (weights[i]) {
            items.push_back({i, weights[i]});
        }
    }
    if (items.empty()) {
        return result;
    }
    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        return a.weight > b.weight;
    });
    std::vector<long double> prefix(items.size() + 1, 0);
    for (size_t i = 0; i < items.size(); ++i) {
        prefix[i + 1] = prefix[i] + items[i].weight;
    }

    struct Range {
        size_t first;
        size_t last;
        QRectF rect;
    };
    std::vector<Range> pending{{0, items.size(), bounds}};
    while (!pending.empty()) {
        const Range range = pending.back();
        pending.pop_back();
        if (range.last - range.first == 1) {
            result[items[range.first].index] = range.rect;
            continue;
        }
        const long double total = prefix[range.last] - prefix[range.first];
        const long double middle = prefix[range.first] + total / 2;
        const auto begin = prefix.begin() + range.first + 1;
        const auto end = prefix.begin() + range.last;
        auto split = std::lower_bound(begin, end, middle);
        if (split == end) {
            --split;
        } else if (split != begin && middle - *(split - 1) < *split - middle) {
            --split;
        }
        const size_t pivot = size_t(split - prefix.begin());
        const qreal fraction = total > 0 ? qreal((*split - prefix[range.first]) / total) : 0;
        QRectF first = range.rect;
        QRectF second = range.rect;
        if (range.rect.width() >= range.rect.height()) {
            const qreal cut = range.rect.width() * fraction;
            first.setWidth(cut);
            second.setLeft(range.rect.left() + cut);
        } else {
            const qreal cut = range.rect.height() * fraction;
            first.setHeight(cut);
            second.setTop(range.rect.top() + cut);
        }
        pending.push_back({pivot, range.last, second});
        pending.push_back({range.first, pivot, first});
    }
    return result;
}
