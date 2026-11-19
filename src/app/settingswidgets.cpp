// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingswidgets.h"

#include <KIO/PreviewJob>
#include <KLocalizedString>
#include <KPluginMetaData>

#include <algorithm>
#include <limits>

namespace
{
constexpr qulonglong MiB = 1024 * 1024;
constexpr qulonglong NoLimit = std::numeric_limits<qulonglong>::max();
constexpr int MaximumMiB = 100000;
}

PreviewPluginList::PreviewPluginList(QWidget *parent)
    : QListWidget(parent)
{
    QList<KPluginMetaData> available = KIO::PreviewJob::availableThumbnailerPlugins();
    std::sort(available.begin(), available.end(), [](const KPluginMetaData &a, const KPluginMetaData &b) {
        return QString::localeAwareCompare(a.name(), b.name()) < 0;
    });
    for (const KPluginMetaData &plugin : std::as_const(available)) {
        auto *entry = new QListWidgetItem(plugin.name(), this);
        entry->setData(Qt::UserRole, plugin.pluginId());
        entry->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        entry->setCheckState(Qt::Unchecked);
    }
    connect(this, &QListWidget::itemChanged, this, &PreviewPluginList::pluginsChanged);
}

QStringList PreviewPluginList::plugins() const
{
    QStringList ids;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Checked) {
            ids.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return ids;
}

void PreviewPluginList::setPlugins(const QStringList &plugins)
{
    for (int row = 0; row < count(); ++row) {
        const bool enabled = plugins.contains(item(row)->data(Qt::UserRole).toString());
        item(row)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
}

PreviewSizeSpinBox::PreviewSizeSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    setRange(0, MaximumMiB);
    setSuffix(i18nc("@item:valuesuffix mebibytes", " MiB"));
    setSpecialValueText(i18nc("@item:inlistbox preview size limit", "No limit"));
    connect(this, &QSpinBox::valueChanged, this, &PreviewSizeSpinBox::bytesChanged);
}

qulonglong PreviewSizeSpinBox::bytes() const
{
    return value() == 0 ? NoLimit : qulonglong(value()) * MiB;
}

void PreviewSizeSpinBox::setBytes(qulonglong bytes)
{
    if (bytes == NoLimit) {
        setValue(0);
        return;
    }
    const qulonglong mib = std::max<qulonglong>(1, (bytes + MiB - 1) / MiB);
    setValue(int(std::min<qulonglong>(mib, MaximumMiB)));
}
