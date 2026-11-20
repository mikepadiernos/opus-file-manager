// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "savedpathfilters.h"

#include <KSharedConfig>
#include <QDialog>

class PathFilterEditor;
class QComboBox;
class QLabel;
class QLineEdit;

class PathFilterPresetsDialog : public QDialog
{
public:
    explicit PathFilterPresetsDialog(QWidget *parent = nullptr, KSharedConfig::Ptr config = {});

    void reject() override;

private:
    void reload(const QString &selected = {});
    void save();
    void remove();
    bool currentPresets(QList<SavedPathFilters::Preset> *presets);
    bool discardChanges();

    KSharedConfig::Ptr m_config;
    QList<SavedPathFilters::Preset> m_presets;
    QComboBox *m_list;
    QLineEdit *m_name;
    PathFilterEditor *m_editor;
    QLabel *m_status;
    int m_selectedIndex = -1;
};
