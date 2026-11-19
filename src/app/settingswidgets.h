// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QListWidget>
#include <QSpinBox>
#include <QWidget>

class RuleListEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList rules READ rules WRITE setRules NOTIFY rulesChanged USER true)

public:
    enum class Kind {
        Ignore,
        Color
    };
    explicit RuleListEditor(Kind kind, QWidget *parent = nullptr);
    QStringList rules() const;
    void setRules(const QStringList &rules);

Q_SIGNALS:
    void rulesChanged();

private:
    void editRule(int row);
    Kind m_kind;
    QListWidget *m_list;
};

class IconInformationList : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList information READ information WRITE setInformation NOTIFY informationChanged USER true)

public:
    explicit IconInformationList(QWidget *parent = nullptr);
    QStringList information() const;
    void setInformation(const QStringList &information);

Q_SIGNALS:
    void informationChanged();
};

/**
 * Checkable list of the installed KIO thumbnailer plugins, bound to a
 * StringList setting of plugin ids by KConfigDialog.
 */
class PreviewPluginList : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList plugins READ plugins WRITE setPlugins NOTIFY pluginsChanged USER true)

public:
    explicit PreviewPluginList(QWidget *parent = nullptr);

    /// Ids of the checked plugins, in list order.
    QStringList plugins() const;
    /**
     * Checks exactly the given plugins.
     * @param plugins plugin ids; ids that are not installed are ignored
     */
    void setPlugins(const QStringList &plugins);

Q_SIGNALS:
    void pluginsChanged();
};

/**
 * File size limit edited in MiB and stored in bytes. Zero in the spin box
 * stands for no limit, stored as the largest qulonglong.
 */
class PreviewSizeSpinBox : public QSpinBox
{
    Q_OBJECT
    Q_PROPERTY(qulonglong bytes READ bytes WRITE setBytes NOTIFY bytesChanged USER true)

public:
    explicit PreviewSizeSpinBox(QWidget *parent = nullptr);

    qulonglong bytes() const;
    /**
     * Shows a byte count, rounded up to whole MiB.
     * @param bytes the limit; the largest qulonglong means no limit
     */
    void setBytes(qulonglong bytes);

Q_SIGNALS:
    void bytesChanged();
};
