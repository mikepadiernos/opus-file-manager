// SPDX-License-Identifier: GPL-3.0-or-later

#include "activitycontroller.h"
#include "settings.h"
#include "symbolicicon.h"

#include <KLocalizedString>
#include <KMessageWidget>
#include <KSharedConfig>
#include <PlasmaActivities/Consumer>
#include <PlasmaActivities/Info>

#include <QAction>
#include <QWidget>

PlasmaActivitySource::PlasmaActivitySource(QObject *parent)
    : ActivitySource(parent)
    , m_consumer(new KActivities::Consumer(this))
{
    connect(m_consumer, &KActivities::Consumer::serviceStatusChanged, this, &ActivitySource::availabilityChanged);
    connect(m_consumer, &KActivities::Consumer::currentActivityChanged, this, &ActivitySource::currentActivityChanged);
    connect(m_consumer, &KActivities::Consumer::activitiesChanged, this, &ActivitySource::activitiesChanged);
}

bool PlasmaActivitySource::isAvailable() const
{
    return m_consumer->serviceStatus() == KActivities::Consumer::Running;
}

QString PlasmaActivitySource::currentActivity() const
{
    return isAvailable() ? m_consumer->currentActivity() : QString();
}

QMap<QString, QString> PlasmaActivitySource::activities() const
{
    QMap<QString, QString> names;
    if (!isAvailable()) {
        return names;
    }
    const QStringList ids = m_consumer->activities();
    for (const QString &id : ids) {
        const KActivities::Info info(id);
        names.insert(id, info.name());
    }
    return names;
}

ActivityController::ActivityController(QWidget *window, CatalogProvider catalog, CurrentWorkspace currentWorkspace)
    : QObject(window)
    , m_window(window)
    , m_catalog(std::move(catalog))
    , m_currentWorkspace(std::move(currentWorkspace))
    , m_message(new KMessageWidget(window))
    , m_apply(new QAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-ok-apply")), i18nc("@action:button", "Apply"), m_message))
    , m_dismiss(new QAction(i18nc("@action:button", "Not Now"), m_message))
{
    m_message->setObjectName(QStringLiteral("activityAssociationOffer"));
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->hide();
    m_apply->setObjectName(QStringLiteral("activityAssociationApply"));
    m_dismiss->setObjectName(QStringLiteral("activityAssociationDismiss"));
    connect(m_apply, &QAction::triggered, this, [this] {
        const ActivityAssociations::Plan plan = m_offered;
        hideOffer();
        if (!plan.isEmpty()) {
            Q_EMIT applyRequested(plan);
        }
    });
    connect(m_dismiss, &QAction::triggered, this, &ActivityController::hideOffer);
}

ActivityController::~ActivityController() = default;

void ActivityController::setSource(ActivitySource *source)
{
    if (m_source == source) {
        return;
    }
    delete m_source;
    m_source = source;
    hideOffer();
    if (!m_source) {
        return;
    }
    m_source->setParent(this);
    m_lastActivity = m_source->currentActivity();
    connect(m_source, &ActivitySource::currentActivityChanged, this, &ActivityController::currentActivityChanged);
    connect(m_source, &ActivitySource::activitiesChanged, this, &ActivityController::reconcile);
    connect(m_source, &ActivitySource::availabilityChanged, this, [this] {
        if (!m_source->isAvailable()) {
            hideOffer();
            return;
        }
        reconcile();
    });
    reconcile();
}

void ActivityController::currentActivityChanged(const QString &id)
{
    const QString previous = m_lastActivity;
    m_lastActivity = id;
    // The service reports the current Activity once it starts answering; that is not a change.
    if (previous.isEmpty() || id.isEmpty() || id == previous) {
        return;
    }
    offerCurrent();
}

ActivitySource *ActivityController::source() const
{
    return m_source;
}

KMessageWidget *ActivityController::messageWidget() const
{
    return m_message;
}

QList<ActivityAssociations::Association> ActivityController::associations() const
{
    return ActivityAssociations::load(KSharedConfig::openConfig()->group(QLatin1String(ActivityAssociations::Group)));
}

void ActivityController::setAssociations(const QList<ActivityAssociations::Association> &associations)
{
    auto config = KSharedConfig::openConfig();
    ActivityAssociations::save(config->group(QLatin1String(ActivityAssociations::Group)), associations);
    config->sync();
}

void ActivityController::applySettings()
{
    if (!Settings::activityAssociations() || !Settings::offerActivityAssociation()) {
        hideOffer();
    }
}

void ActivityController::reconcile()
{
    // Without the service there is no knowing which Activities still exist.
    if (!m_source || !m_source->isAvailable()) {
        return;
    }
    QList<ActivityAssociations::Association> stored = associations();
    if (ActivityAssociations::reconcile(stored, m_source->activities())) {
        setAssociations(stored);
    }
}

QString ActivityController::activityName(const QString &id) const
{
    QString name = m_source ? m_source->activities().value(id) : QString();
    if (name.isEmpty()) {
        const auto stored = associations();
        if (const auto *association = ActivityAssociations::find(stored, id)) {
            name = association->activityName;
        }
    }
    return name.isEmpty() ? id : name;
}

void ActivityController::offerCurrent()
{
    if (!m_source || !m_source->isAvailable() || !Settings::activityAssociations() || !Settings::offerActivityAssociation()) {
        hideOffer();
        return;
    }
    reconcile();
    const QString id = m_source->currentActivity();
    const auto stored = associations();
    const auto *association = ActivityAssociations::find(stored, id);
    const ActivityAssociations::Plan plan =
        association ? ActivityAssociations::plan(*association, m_catalog(), m_currentWorkspace()) : ActivityAssociations::Plan{};
    if (plan.isEmpty() && plan.problems.isEmpty()) {
        hideOffer();
        return;
    }
    const QString name = activityName(id);
    QString text;
    const bool folders = !plan.primaryFolder.isEmpty() || !plan.secondaryFolder.isEmpty();
    if (!plan.workspaceId.isEmpty() && folders) {
        text = i18nc("@info %1 is a workspace name, %2 an Activity name",
                     "Switch to the %1 workspace and open the default folders of the %2 Activity?",
                     plan.workspaceName,
                     name);
    } else if (!plan.workspaceId.isEmpty()) {
        text = i18nc("@info %1 is a workspace name, %2 an Activity name", "Switch to the %1 workspace for the %2 Activity?", plan.workspaceName, name);
    } else if (folders) {
        text = i18nc("@info %1 is an Activity name", "Open the default folders of the %1 Activity in new tabs?", name);
    } else {
        text = i18nc("@info %1 is an Activity name", "The association of the %1 Activity cannot be applied.", name);
    }
    if (!plan.problems.isEmpty()) {
        text += QLatin1Char(' ') + plan.problems.join(QLatin1Char(' '));
    }
    m_offered = plan;
    m_message->removeAction(m_apply);
    m_message->removeAction(m_dismiss);
    m_message->setText(text);
    m_message->setMessageType(plan.problems.isEmpty() ? KMessageWidget::Information : KMessageWidget::Warning);
    if (!plan.isEmpty()) {
        m_message->addAction(m_apply);
    }
    m_message->addAction(m_dismiss);
    if (m_message->isHideAnimationRunning() || m_message->isHidden()) {
        m_message->animatedShow();
    }
}

void ActivityController::applyCurrent()
{
    hideOffer();
    if (!m_source || !m_source->isAvailable()) {
        Q_EMIT statusMessage(i18nc("@info:status", "The Plasma Activities service is not running."));
        return;
    }
    const QString id = m_source->currentActivity();
    const auto stored = associations();
    const auto *association = ActivityAssociations::find(stored, id);
    if (!association) {
        Q_EMIT statusMessage(i18nc("@info:status %1 is an Activity name", "The %1 Activity has no association.", activityName(id)));
        return;
    }
    const ActivityAssociations::Plan plan = ActivityAssociations::plan(*association, m_catalog(), QString());
    if (!plan.problems.isEmpty()) {
        Q_EMIT statusMessage(plan.problems.join(QLatin1Char(' ')));
    }
    if (!plan.isEmpty()) {
        Q_EMIT applyRequested(plan);
    }
}

ActivityAssociations::Plan ActivityController::offeredPlan() const
{
    return m_offered;
}

void ActivityController::hideOffer()
{
    m_offered = {};
    m_message->removeAction(m_apply);
    m_message->removeAction(m_dismiss);
    if (!m_message->isHidden() && !m_message->isHideAnimationRunning()) {
        m_message->animatedHide();
    }
}
