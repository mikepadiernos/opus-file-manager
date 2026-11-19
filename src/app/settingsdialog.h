// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KPageDialog>

#include <QColor>
#include <QHash>
#include <QMap>

class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;

class SettingsDialog : public KPageDialog
{
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    void addColorRule();
    void editColorRule();
    void removeColorRule();
    void setColorRuleRow(int row, const QString &extension, const QColor &color);

    QHash<QString, QCheckBox *> m_columnChecks;
    QHash<QString, QCheckBox *> m_iconInformationChecks;
    QComboBox *m_groupBy = nullptr;
    QComboBox *m_sizeFormat = nullptr;
    QComboBox *m_dateFormat = nullptr;
    QCheckBox *m_showSeconds = nullptr;
    QSpinBox *m_iconNameLines = nullptr;
    QCheckBox *m_showPanels = nullptr;
    QComboBox *m_panelPosition = nullptr;
    QCheckBox *m_autoFocusFolders = nullptr;
    QCheckBox *m_showPanelTitles = nullptr;
    QCheckBox *m_showFavorites = nullptr;
    QCheckBox *m_showPlaces = nullptr;
    QCheckBox *m_showFolderTree = nullptr;
    QComboBox *m_defaultArchiveFormat = nullptr;
    QCheckBox *m_showTabIcons = nullptr;
    QCheckBox *m_expandTabs = nullptr;
    QPlainTextEdit *m_ignoreEntries = nullptr;
    QTableWidget *m_colorRules = nullptr;
};
