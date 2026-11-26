// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "activityassociations.h"
#include "workspacecatalog.h"

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class KMessageWidget;
class QAction;
class QWidget;

namespace KActivities
{
class Consumer;
}

/// The Activities that exist and which one is current.
class ActivitySource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Whether the Activities service is running and answering.
    virtual bool isAvailable() const = 0;
    virtual QString currentActivity() const = 0;
    /// The existing Activities' names by id.
    virtual QMap<QString, QString> activities() const = 0;

Q_SIGNALS:
    void availabilityChanged();
    void currentActivityChanged(const QString &id);
    void activitiesChanged();
};

/// Reads Activities from the Plasma Activities service. It never creates,
/// deletes or switches an Activity.
class PlasmaActivitySource : public ActivitySource
{
    Q_OBJECT

public:
    explicit PlasmaActivitySource(QObject *parent = nullptr);

    bool isAvailable() const override;
    QString currentActivity() const override;
    QMap<QString, QString> activities() const override;

private:
    KActivities::Consumer *m_consumer;
};

/// Offers to apply the current Activity's workspace and default folders.
///
/// When the current Activity changes, and associations are enabled, the
/// controller shows its message with Apply and Not Now actions. The Activity
/// that is current when Plasma Opus starts is not offered, so a restored
/// session stays as it was. It applies
/// nothing on its own: applyRequested() is emitted only when the user chooses
/// to apply.
class ActivityController : public QObject
{
    Q_OBJECT

public:
    using CatalogProvider = std::function<WorkspaceCatalog::Catalog()>;
    using CurrentWorkspace = std::function<QString()>;

    /// @param window parents the offer message, which the window places
    ActivityController(QWidget *window, CatalogProvider catalog, CurrentWorkspace currentWorkspace);
    ~ActivityController() override;

    /// Replaces the Activities source and takes ownership of it.
    void setSource(ActivitySource *source);
    ActivitySource *source() const;

    /// The message that offers to apply an association. It starts hidden.
    KMessageWidget *messageWidget() const;

    QList<ActivityAssociations::Association> associations() const;
    void setAssociations(const QList<ActivityAssociations::Association> &associations);

    /// Hides the offer once associations or offers are turned off.
    void applySettings();

    /// Offers the current Activity's association when there is one to apply.
    void offerCurrent();

    /// Applies the current Activity's association now, switching to its
    /// workspace even when it is already active.
    void applyCurrent();

    /// The plan the visible offer applies, or an empty plan.
    ActivityAssociations::Plan offeredPlan() const;

Q_SIGNALS:
    /// The user chose to apply @p plan.
    void applyRequested(const ActivityAssociations::Plan &plan);
    /// A short message for the status bar.
    void statusMessage(const QString &text);

private:
    void reconcile();
    void hideOffer();
    void currentActivityChanged(const QString &id);
    QString activityName(const QString &id) const;

    QPointer<QWidget> m_window;
    CatalogProvider m_catalog;
    CurrentWorkspace m_currentWorkspace;
    ActivitySource *m_source = nullptr;
    KMessageWidget *m_message;
    QAction *m_apply;
    QAction *m_dismiss;
    ActivityAssociations::Plan m_offered;
    /// The Activity that was current when last seen, so the first one reported is not offered.
    QString m_lastActivity;
};
