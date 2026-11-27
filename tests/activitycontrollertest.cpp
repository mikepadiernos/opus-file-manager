// SPDX-License-Identifier: GPL-3.0-or-later

#include "activitycontroller.h"
#include "activityassociationseditor.h"
#include "activityreporting.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"

#include <KMessageWidget>
#include <KSharedConfig>
#include <KUrlRequester>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

namespace
{
class FakeActivities : public ActivitySource
{
public:
    bool available = true;
    QString current;
    QMap<QString, QString> names;

    bool isAvailable() const override
    {
        return available;
    }
    QString currentActivity() const override
    {
        return available ? current : QString();
    }
    QMap<QString, QString> activities() const override
    {
        return available ? names : QMap<QString, QString>{};
    }
    void switchTo(const QString &id)
    {
        current = id;
        Q_EMIT currentActivityChanged(id);
    }
};

const QString WorkId = QStringLiteral("{11111111-1111-1111-1111-111111111111}");
const QString PlayId = QStringLiteral("{22222222-2222-2222-2222-222222222222}");
}

class ActivityControllerTest : public QObject
{
    Q_OBJECT

    WorkspaceCatalog::Catalog m_catalog;
    QString m_currentWorkspace;

    struct Fixture {
        QWidget window;
        ActivityController *controller = nullptr;
        FakeActivities *source = nullptr;
    };

    void makeController(Fixture &fixture)
    {
        fixture.controller = new ActivityController(
            &fixture.window,
            [this] {
                return m_catalog;
            },
            [this] {
                return m_currentWorkspace;
            });
        fixture.source = new FakeActivities;
        fixture.source->current = QStringLiteral("home");
        fixture.source->names = {{QStringLiteral("home"), QStringLiteral("Home")}, {QStringLiteral("work"), QStringLiteral("Work")}};
        fixture.controller->setSource(fixture.source);
    }

    static ActivityAssociations::Association association(const QString &activityId, const QString &workspaceId)
    {
        ActivityAssociations::Association association;
        association.activityId = activityId;
        association.workspaceId = workspaceId;
        return association;
    }

    static QAction *messageAction(ActivityController *controller, const QString &name)
    {
        const QList<QAction *> actions = controller->messageWidget()->actions();
        for (QAction *action : actions) {
            if (action->objectName() == name) {
                return action;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        WorkspaceCatalog::Workspace work;
        work.id = WorkId;
        work.name = QStringLiteral("Office");
        WorkspaceCatalog::Workspace play;
        play.id = PlayId;
        play.name = QStringLiteral("Games");
        m_catalog.workspaces = {work, play};
        m_catalog.activeId = PlayId;
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::setActivityAssociations(true);
        KSharedConfig::openConfig()->deleteGroup(QLatin1String(ActivityAssociations::Group));
        m_currentWorkspace = PlayId;
    }

    void offersOnlyWhenTheActivityChanges()
    {
        Fixture fixture;
        makeController(fixture);
        fixture.controller->setAssociations({association(QStringLiteral("work"), WorkId)});
        QSignalSpy applied(fixture.controller, &ActivityController::applyRequested);

        // The Activity current at startup is left alone.
        fixture.source->switchTo(QStringLiteral("home"));
        QVERIFY(fixture.controller->messageWidget()->isHidden());

        fixture.source->switchTo(QStringLiteral("work"));
        QVERIFY(!fixture.controller->messageWidget()->isHidden());
        QCOMPARE(fixture.controller->offeredPlan().workspaceId, WorkId);
        QVERIFY(fixture.controller->messageWidget()->text().contains(QStringLiteral("Office")));
        QVERIFY(applied.isEmpty());

        QAction *apply = messageAction(fixture.controller, QStringLiteral("activityAssociationApply"));
        QVERIFY(apply);
        apply->trigger();
        QCOMPARE(applied.size(), 1);
        QCOMPARE(applied.at(0).at(0).value<ActivityAssociations::Plan>().workspaceId, WorkId);
        QVERIFY(fixture.controller->offeredPlan().isEmpty());
    }

    void notNowAppliesNothing()
    {
        Fixture fixture;
        makeController(fixture);
        fixture.controller->setAssociations({association(QStringLiteral("work"), WorkId)});
        QSignalSpy applied(fixture.controller, &ActivityController::applyRequested);
        fixture.source->switchTo(QStringLiteral("work"));
        QAction *dismiss = messageAction(fixture.controller, QStringLiteral("activityAssociationDismiss"));
        QVERIFY(dismiss);
        dismiss->trigger();
        QVERIFY(applied.isEmpty());
        QVERIFY(fixture.controller->offeredPlan().isEmpty());
        QVERIFY(fixture.controller->messageWidget()->actions().isEmpty());
    }

    void respectsTheSettings()
    {
        Fixture fixture;
        makeController(fixture);
        fixture.controller->setAssociations({association(QStringLiteral("work"), WorkId)});
        Settings::setActivityAssociations(false);
        fixture.source->switchTo(QStringLiteral("work"));
        QVERIFY(fixture.controller->offeredPlan().isEmpty());

        Settings::setActivityAssociations(true);
        Settings::setOfferActivityAssociation(false);
        fixture.source->switchTo(QStringLiteral("home"));
        fixture.source->switchTo(QStringLiteral("work"));
        QVERIFY(fixture.controller->offeredPlan().isEmpty());

        // Applying by hand still works with offers turned off.
        QSignalSpy applied(fixture.controller, &ActivityController::applyRequested);
        fixture.controller->applyCurrent();
        QCOMPARE(applied.size(), 1);
    }

    void skipsTheActiveWorkspaceUnlessAppliedByHand()
    {
        Fixture fixture;
        makeController(fixture);
        fixture.controller->setAssociations({association(QStringLiteral("work"), PlayId)});
        fixture.source->switchTo(QStringLiteral("work"));
        QVERIFY(fixture.controller->offeredPlan().isEmpty());
        QVERIFY(fixture.controller->messageWidget()->isHidden());

        QSignalSpy applied(fixture.controller, &ActivityController::applyRequested);
        fixture.controller->applyCurrent();
        QCOMPARE(applied.size(), 1);
        QCOMPARE(applied.at(0).at(0).value<ActivityAssociations::Plan>().workspaceId, PlayId);
    }

    void warnsAboutMissingParts()
    {
        Fixture fixture;
        makeController(fixture);
        auto entry = association(QStringLiteral("work"), QStringLiteral("{33333333-3333-3333-3333-333333333333}"));
        fixture.controller->setAssociations({entry});
        fixture.source->switchTo(QStringLiteral("work"));
        QVERIFY(!fixture.controller->messageWidget()->isHidden());
        QCOMPARE(fixture.controller->messageWidget()->messageType(), KMessageWidget::Warning);
        QVERIFY(!messageAction(fixture.controller, QStringLiteral("activityAssociationApply")));
        // The association is kept for when the workspace comes back.
        QCOMPARE(fixture.controller->associations().size(), 1);
    }

    void keepsAssociationsOfRemovedActivities()
    {
        Fixture fixture;
        makeController(fixture);
        fixture.controller->setAssociations({association(QStringLiteral("gone"), WorkId)});
        fixture.source->available = false;
        Q_EMIT fixture.source->availabilityChanged();
        QVERIFY(!fixture.controller->associations().at(0).removed);
        QSignalSpy status(fixture.controller, &ActivityController::statusMessage);
        fixture.controller->applyCurrent();
        QCOMPARE(status.size(), 1);

        fixture.source->available = true;
        Q_EMIT fixture.source->availabilityChanged();
        const auto stored = fixture.controller->associations();
        QCOMPARE(stored.size(), 1);
        QVERIFY(stored.at(0).removed);
        QCOMPARE(stored.at(0).workspaceId, WorkId);
    }

    void editorEditsAssociations()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Fixture fixture;
        makeController(fixture);
        auto removed = association(QStringLiteral("old"), PlayId);
        removed.activityName = QStringLiteral("Retired");

        ActivityAssociationsEditor editor;
        editor.setSource(fixture.source);
        editor.setCatalog(m_catalog);
        editor.setAssociations({removed});
        QSignalSpy changed(&editor, &ActivityAssociationsEditor::changed);
        auto *list = editor.findChild<QListWidget *>(QStringLiteral("activityList"));
        QVERIFY(list);
        QCOMPARE(list->count(), 3);
        QVERIFY(editor.findChild<KMessageWidget *>(QStringLiteral("activityServiceUnavailable"))->isHidden());
        QStringList texts;
        for (int row = 0; row < list->count(); ++row) {
            texts.append(list->item(row)->text());
        }
        QVERIFY(texts.contains(QStringLiteral("Home (current)")));
        QVERIFY(texts.contains(QStringLiteral("Retired (removed)")));
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("home"));
        QVERIFY(changed.isEmpty());

        auto *workspace = editor.findChild<QComboBox *>(QStringLiteral("activityWorkspace"));
        workspace->setCurrentIndex(workspace->findData(WorkId));
        auto *mode = editor.findChild<QComboBox *>(QStringLiteral("activityBehaviorMode"));
        mode->setCurrentIndex(mode->findData(int(WorkspaceCatalog::BehaviorMode::Selected)));
        auto *appearance = editor.findChild<QCheckBox *>(QStringLiteral("activityAppearanceBehavior"));
        QVERIFY(appearance->isEnabled());
        appearance->setChecked(false);
        editor.findChild<KUrlRequester *>(QStringLiteral("activityPrimaryFolder"))->setUrl(QUrl::fromLocalFile(dir.path()));
        QVERIFY(!changed.isEmpty());

        // Clearing the removed Activity's association drops it from the list.
        for (int row = 0; row < list->count(); ++row) {
            if (list->item(row)->data(Qt::UserRole).toString() == QStringLiteral("old")) {
                list->setCurrentRow(row);
            }
        }
        editor.findChild<QPushButton *>(QStringLiteral("activityClearAssociation"))->click();
        QCOMPARE(list->count(), 2);

        const auto stored = editor.associations();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.at(0).activityId, QStringLiteral("home"));
        QCOMPARE(stored.at(0).activityName, QStringLiteral("Home"));
        QCOMPARE(stored.at(0).workspaceId, WorkId);
        QVERIFY(!stored.at(0).inheritBehaviors);
        QCOMPARE(stored.at(0).behaviors.mode, WorkspaceCatalog::BehaviorMode::Selected);
        QVERIFY(!stored.at(0).behaviors.appearance);
        QVERIFY(stored.at(0).behaviors.layout);
        QCOMPARE(stored.at(0).primaryFolder, QUrl::fromLocalFile(dir.path()));

        // Turned off, nothing can be edited.
        editor.setEditable(false);
        QVERIFY(!list->isEnabled());
        QVERIFY(!workspace->isEnabled());
        QVERIFY(!appearance->isEnabled());
    }

    void editorKeepsAssociationsWithoutTheService()
    {
        Fixture fixture;
        makeController(fixture);
        fixture.source->available = false;
        ActivityAssociationsEditor editor;
        editor.setSource(fixture.source);
        editor.setCatalog(m_catalog);
        editor.setAssociations({association(QStringLiteral("work"), WorkId)});
        QVERIFY(!editor.findChild<KMessageWidget *>(QStringLiteral("activityServiceUnavailable"))->isHidden());
        auto *list = editor.findChild<QListWidget *>(QStringLiteral("activityList"));
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->item(0)->text(), QStringLiteral("work"));
        QCOMPARE(editor.associations(), QList<ActivityAssociations::Association>{association(QStringLiteral("work"), WorkId)});

        // The list follows the service once it is back.
        fixture.source->available = true;
        Q_EMIT fixture.source->availabilityChanged();
        QVERIFY(editor.findChild<KMessageWidget *>(QStringLiteral("activityServiceUnavailable"))->isHidden());
        QCOMPARE(list->count(), 2);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("work"));
        const auto stored = editor.associations();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.at(0).workspaceId, WorkId);
        QCOMPARE(stored.at(0).activityName, QStringLiteral("Work"));
    }

    void windowOpensDefaultFoldersInNewTabs()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("first")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("second")));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        QVERIFY(window.activityController());
        QVERIFY(window.activityController()->messageWidget()->isHidden());
        QCOMPARE(window.activityController()->messageWidget()->parentWidget(), window.centralWidget());
        Pane *first = window.activePane();
        Pane *second = window.inactivePane();
        const int firstTabs = first->findChild<QTabBar *>()->count();
        const int secondTabs = second->findChild<QTabBar *>()->count();

        ActivityAssociations::Plan plan;
        plan.activityId = QStringLiteral("work");
        plan.primaryFolder = QUrl::fromLocalFile(dir.filePath(QStringLiteral("first")));
        plan.secondaryFolder = QUrl::fromLocalFile(dir.filePath(QStringLiteral("second")));
        window.applyActivityPlan(plan);
        QCOMPARE(first->findChild<QTabBar *>()->count(), firstTabs + 1);
        QCOMPARE(second->findChild<QTabBar *>()->count(), secondTabs + 1);
        QCOMPARE(first->url().adjusted(QUrl::StripTrailingSlash), plan.primaryFolder);
        QCOMPARE(second->url().adjusted(QUrl::StripTrailingSlash), plan.secondaryFolder);
    }

    void reportsVisitedFoldersAndOpenedFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("visited")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("tmp")));
        // The test folder is itself temporary, so point temporary files elsewhere.
        const QByteArray tmpDir = qgetenv("TMPDIR");
        qputenv("TMPDIR", QFile::encodeName(dir.filePath(QStringLiteral("tmp"))));
        QList<QUrl> reported;
        ActivityReporting::setReporter([&reported](const QUrl &url) {
            reported.append(url);
        });
        const auto restore = qScopeGuard([&tmpDir] {
            ActivityReporting::setReporter({});
            Settings::setReportActivityUsage(true);
            if (tmpDir.isNull()) {
                qunsetenv("TMPDIR");
            } else {
                qputenv("TMPDIR", tmpDir);
            }
        });
        const QUrl visited = QUrl::fromLocalFile(dir.filePath(QStringLiteral("visited")));

        QVERIFY(Settings::reportActivityUsage());
        ActivityReporting::reportUsed(visited);
        ActivityReporting::reportUsed(QUrl::fromLocalFile(dir.filePath(QStringLiteral("tmp/mail/notes.txt"))));
        ActivityReporting::reportUsed(QUrl(QStringLiteral("trash:/")));
        ActivityReporting::reportUsed(QUrl(QStringLiteral("sftp://ada:secret@host/srv")));
        QCOMPARE(reported, (QList<QUrl>{visited, QUrl(QStringLiteral("sftp://ada@host/srv"))}));

        reported.clear();
        MainWindow window;
        window.activePane()->setUrl(visited);
        QTRY_VERIFY(reported.contains(visited));

        reported.clear();
        Settings::setReportActivityUsage(false);
        ActivityReporting::reportUsed(visited);
        window.activePane()->setUrl(QUrl::fromLocalFile(dir.path()));
        QTRY_COMPARE(window.activePane()->url().adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(dir.path()));
        QVERIFY(reported.isEmpty());
    }
};

QTEST_MAIN(ActivityControllerTest)

#include "activitycontrollertest.moc"
