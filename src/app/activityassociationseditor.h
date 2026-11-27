// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "activityassociations.h"
#include "workspacecatalog.h"

#include <QMap>
#include <QPointer>
#include <QWidget>

class ActivitySource;
class KMessageWidget;
class KUrlRequester;
class QCheckBox;
class QComboBox;
class QListWidget;
class QPushButton;

/// Edits the association of each Plasma Activity with a workspace, the
/// behaviours to use when switching to it, and a default folder for each pane.
///
/// It lists the existing Activities and those that only have a saved
/// association, such as removed ones, which keep it until it is cleared here.
/// It only reads Activities; it never creates, deletes or switches them, and
/// saves nothing itself: associations() returns the edited associations.
class ActivityAssociationsEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ActivityAssociationsEditor(QWidget *parent = nullptr);

    /// Reads Activities from @p source, or from none when it is null, as when the service cannot be reached.
    void setSource(ActivitySource *source);
    /// Offers the workspaces of @p catalog, keeping the edits made so far.
    void setCatalog(const WorkspaceCatalog::Catalog &catalog);
    /// Shows @p associations for editing and selects the current Activity.
    void setAssociations(const QList<ActivityAssociations::Association> &associations);
    /// The associations as edited so far, including the selected one.
    QList<ActivityAssociations::Association> associations();
    /// Enables or disables editing, as when associations are turned off.
    void setEditable(bool editable);

Q_SIGNALS:
    /// The user changed an association.
    void changed();

private:
    void populate();
    void refresh();
    void loadForm(const QString &activityId);
    void storeForm();
    void updateEnabled();
    void edited();
    QString selectedId() const;
    void select(const QString &activityId);

    QPointer<ActivitySource> m_source;
    WorkspaceCatalog::Catalog m_catalog;
    QList<ActivityAssociations::Association> m_associations;
    QMap<QString, QString> m_names;
    QString m_formId;
    bool m_editable = true;
    /// Whether the form is being filled in, so its signals are not edits.
    bool m_loading = false;
    KMessageWidget *m_message;
    KMessageWidget *m_error;
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
    QPushButton *m_clear;
};
