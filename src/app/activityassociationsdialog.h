// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "activityassociations.h"
#include "workspacecatalog.h"

#include <QDialog>
#include <QMap>

class ActivityController;
class KMessageWidget;
class KUrlRequester;
class QCheckBox;
class QComboBox;
class QListWidget;

/// Associates each Plasma Activity with a workspace, the behaviours to use
/// when switching to it, and a default folder for each pane.
///
/// Activities that no longer exist keep their associations until they are
/// cleared here. The dialog only reads Activities; it never creates, deletes
/// or switches them.
class ActivityAssociationsDialog : public QDialog
{
    Q_OBJECT

public:
    ActivityAssociationsDialog(ActivityController *controller, const WorkspaceCatalog::Catalog &catalog, QWidget *parent = nullptr);

    /// The associations as edited so far, including the selected one.
    QList<ActivityAssociations::Association> associations();

    void accept() override;

private:
    void populate();
    void loadForm(const QString &activityId);
    void storeForm();
    void updateEnabled();
    QString selectedId() const;

    ActivityController *m_controller;
    WorkspaceCatalog::Catalog m_catalog;
    QList<ActivityAssociations::Association> m_associations;
    QMap<QString, QString> m_names;
    QString m_formId;
    KMessageWidget *m_message;
    QCheckBox *m_enabled;
    QCheckBox *m_offer;
    QListWidget *m_activities;
    QComboBox *m_workspace;
    QComboBox *m_behaviorMode;
    QCheckBox *m_locationPresentation;
    QCheckBox *m_searchScope;
    QCheckBox *m_layoutBehavior;
    QCheckBox *m_paneTabRestoration;
    QCheckBox *m_appearanceBehavior;
    QCheckBox *m_yaziBehavior;
    KUrlRequester *m_primaryFolder;
    KUrlRequester *m_secondaryFolder;
};
