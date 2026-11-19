// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pathfilter.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

class PathFilterEditor : public QWidget
{
public:
    explicit PathFilterEditor(const QString &objectPrefix, QWidget *parent = nullptr);

    PathFilter filter() const;
    void setFilter(const PathFilter &filter);

private:
    QLineEdit *m_path;
    QLineEdit *m_parent;
    QPlainTextEdit *m_excluded;
    QSpinBox *m_depth;
};
