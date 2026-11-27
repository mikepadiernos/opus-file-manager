// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudsyncdialog.h"

#include "appidentity.h"
#include "onlineaccounts.h"
#include "symbolicicon.h"

#include <KFile>
#include <KLocalizedString>
#include <KProtocolInfo>
#include <KUrlRequester>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace CloudSync;

namespace
{
QStringList patterns(const QPlainTextEdit *edit)
{
    return edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QString menuText(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

QString stateText(const std::optional<State> &state)
{
    if (!state) {
        return i18nc("@item conflict side", "Deleted");
    }
    if (state->folder) {
        return i18nc("@item conflict side", "Folder");
    }
    const QString size = QLocale().formattedDataSize(state->size);
    if (state->modified <= 0) {
        return size;
    }
    const QString time = QLocale().toString(QDateTime::fromSecsSinceEpoch(state->modified), QLocale::ShortFormat);
    return i18nc("@item conflict side: %1 size, %2 modification time", "%1, modified %2", size, time);
}

void addResolutions(QComboBox *combo)
{
    combo->addItem(i18nc("@item:inlistbox conflict resolution", "Skip"), int(Resolution::Skip));
    combo->addItem(i18nc("@item:inlistbox conflict resolution", "Keep Local Version"), int(Resolution::KeepLocal));
    combo->addItem(i18nc("@item:inlistbox conflict resolution", "Keep Remote Version"), int(Resolution::KeepRemote));
    combo->addItem(i18nc("@item:inlistbox conflict resolution", "Keep Both"), int(Resolution::KeepBoth));
}

enum Column {
    LocalColumn,
    RemoteColumn,
    ModeColumn,
    StatusColumn,
};
}

CloudSyncPairDialog::CloudSyncPairDialog(const Pair &pair, const QList<Pair> &others, KSharedConfig::Ptr config, RcloneMounts *rclone, QWidget *parent)
    : QDialog(parent)
    , m_pair(pair)
    , m_others(others)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_rclone(rclone)
    , m_engineLocations(defaultEngineLocations())
    , m_local(new KUrlRequester(this))
    , m_kind(new QComboBox(this))
    , m_remote(new QLineEdit(this))
    , m_accounts(new QToolButton(this))
    , m_remoteRow(new QWidget(this))
    , m_rcloneRemote(new QLineEdit(this))
    , m_remotes(new QToolButton(this))
    , m_rcloneRow(new QWidget(this))
    , m_mode(new QComboBox(this))
    , m_mirror(new QCheckBox(i18nc("@option:check", "Delete backup copies of deleted items"), this))
    , m_saved(new QComboBox(this))
    , m_include(new QPlainTextEdit(this))
    , m_exclude(new QPlainTextEdit(this))
    , m_recursive(new QCheckBox(i18nc("@option:check", "Synchronize subfolders"), this))
    , m_interval(new QSpinBox(this))
    , m_paused(new QCheckBox(i18nc("@option:check", "Pause background synchronization"), this))
    , m_error(new QLabel(this))
    , m_ok(nullptr)
{
    setWindowTitle(i18nc("@title:window", "Cloud Sync Pair"));
    if (m_pair.id.isEmpty()) {
        m_pair.id = newId();
    }

    m_local->setObjectName(QStringLiteral("cloudSyncLocal"));
    m_local->setMode(KFile::Directory | KFile::LocalOnly);
    m_local->setPlaceholderText(i18nc("@info:placeholder", "Local folder"));
    m_local->setText(pair.localPath);

    m_kind->setObjectName(QStringLiteral("cloudSyncKind"));
    m_kind->addItem(i18nc("@item:inlistbox remote kind", "Network folder or online account"));
    m_kind->addItem(i18nc("@item:inlistbox remote kind", "rclone remote"));
    m_kind->setCurrentIndex(pair.usesRclone() ? 1 : 0);

    m_remote->setObjectName(QStringLiteral("cloudSyncRemote"));
    m_remote->setPlaceholderText(i18nc("@info:placeholder", "webdavs://cloud.example.com/remote.php/dav/files/user/Documents"));
    m_remote->setText(pair.remote.toString(QUrl::RemovePassword));
    m_accounts->setObjectName(QStringLiteral("cloudSyncAccounts"));
    m_accounts->setIcon(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")));
    m_accounts->setToolTip(i18nc("@info:tooltip", "Choose a folder from KDE Online Accounts"));
    m_accounts->setPopupMode(QToolButton::InstantPopup);
    auto *accountsMenu = new QMenu(m_accounts);
    accountsMenu->setToolTipsVisible(true);
    m_accounts->setMenu(accountsMenu);
    connect(accountsMenu, &QMenu::aboutToShow, this, &CloudSyncPairDialog::populateAccounts);
    auto *remoteLayout = new QHBoxLayout(m_remoteRow);
    remoteLayout->setContentsMargins({});
    remoteLayout->addWidget(m_remote, 1);
    remoteLayout->addWidget(m_accounts);

    m_rcloneRemote->setObjectName(QStringLiteral("cloudSyncRcloneRemote"));
    m_rcloneRemote->setPlaceholderText(i18nc("@info:placeholder rclone location", "remote:path"));
    m_rcloneRemote->setText(pair.rcloneRemote);
    m_remotes->setObjectName(QStringLiteral("cloudSyncRcloneRemotes"));
    m_remotes->setIcon(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")));
    m_remotes->setToolTip(i18nc("@info:tooltip", "Choose a remote configured in rclone"));
    m_remotes->setPopupMode(QToolButton::InstantPopup);
    auto *remotesMenu = new QMenu(m_remotes);
    m_remotes->setMenu(remotesMenu);
    connect(remotesMenu, &QMenu::aboutToShow, this, [this, remotesMenu] {
        if (!m_rclone) {
            populateRemotes({}, i18nc("@info", "rclone is not available."));
            return;
        }
        remotesMenu->clear();
        remotesMenu->addAction(i18nc("@item:inmenu", "Loading…"))->setEnabled(false);
        m_rclone->listRemotes();
    });
    if (m_rclone) {
        connect(m_rclone, &RcloneMounts::remotesListed, this, &CloudSyncPairDialog::populateRemotes);
    }
    auto *rcloneLayout = new QHBoxLayout(m_rcloneRow);
    rcloneLayout->setContentsMargins({});
    rcloneLayout->addWidget(m_rcloneRemote, 1);
    rcloneLayout->addWidget(m_remotes);

    m_mode->setObjectName(QStringLiteral("cloudSyncMode"));
    for (const Mode mode : {Mode::Sync, Mode::CloudBackup, Mode::LocalBackup}) {
        m_mode->addItem(modeName(mode), int(mode));
    }
    m_mode->setItemData(0, i18nc("@info:tooltip", "Copy changes on either side to the other"), Qt::ToolTipRole);
    m_mode->setItemData(1, i18nc("@info:tooltip", "Upload local changes; remote changes are left alone"), Qt::ToolTipRole);
    m_mode->setItemData(2, i18nc("@info:tooltip", "Download remote changes; local changes are left alone"), Qt::ToolTipRole);
    m_mode->setCurrentIndex(m_mode->findData(int(pair.mode)));
    m_mirror->setObjectName(QStringLiteral("cloudSyncMirror"));
    m_mirror->setChecked(pair.mirrorDeletions);

    m_saved->setObjectName(QStringLiteral("cloudSyncProfiles"));
    m_profiles = FolderSyncProfiles::load(m_config->group(QStringLiteral("FolderSyncProfiles")));
    m_saved->addItem(i18nc("@item:inlistbox no saved filter", "None"));
    for (const FolderSyncProfiles::Profile &profile : std::as_const(m_profiles)) {
        m_saved->addItem(profile.name);
    }
    m_saved->setEnabled(!m_profiles.isEmpty());
    m_saved->setToolTip(i18nc("@info:tooltip", "Use filters saved in Synchronize Folders"));
    connect(m_saved, &QComboBox::activated, this, [this](int index) {
        if (index <= 0 || index > m_profiles.size()) {
            return;
        }
        const FolderSync::Options &options = m_profiles.at(index - 1).options;
        m_include->setPlainText(options.includePatterns.join(QLatin1Char('\n')));
        m_exclude->setPlainText(options.excludePatterns.join(QLatin1Char('\n')));
        m_recursive->setChecked(options.recursive);
    });

    m_include->setObjectName(QStringLiteral("cloudSyncInclude"));
    m_include->setPlaceholderText(i18nc("@info:placeholder", "Everything; or one pattern per line, such as *.odt"));
    m_include->setPlainText(pair.options.includePatterns.join(QLatin1Char('\n')));
    m_include->setTabChangesFocus(true);
    m_exclude->setObjectName(QStringLiteral("cloudSyncExclude"));
    m_exclude->setPlaceholderText(i18nc("@info:placeholder", "Nothing; or one pattern per line, such as *.tmp"));
    m_exclude->setPlainText(pair.options.excludePatterns.join(QLatin1Char('\n')));
    m_exclude->setTabChangesFocus(true);
    for (QPlainTextEdit *edit : {m_include, m_exclude}) {
        edit->setFixedHeight(edit->fontMetrics().lineSpacing() * 4 + 2 * edit->frameWidth() + 8);
    }
    m_recursive->setObjectName(QStringLiteral("cloudSyncRecursive"));
    m_recursive->setChecked(pair.options.recursive);

    m_interval->setObjectName(QStringLiteral("cloudSyncInterval"));
    m_interval->setRange(0, 24 * 60);
    m_interval->setSpecialValueText(i18nc("@item:valuesuffix synchronization interval", "Only on request"));
    m_interval->setSuffix(i18nc("@item:valuesuffix synchronization interval", " minutes"));
    m_interval->setValue(pair.intervalMinutes);
    m_paused->setObjectName(QStringLiteral("cloudSyncPaused"));
    m_paused->setChecked(pair.paused);

    m_error->setObjectName(QStringLiteral("cloudSyncPairError"));
    m_error->setWordWrap(true);
    m_error->setTextFormat(Qt::PlainText);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:chooser", "Local folder:"), m_local);
    form->addRow(i18nc("@label:listbox", "Remote type:"), m_kind);
    form->addRow(i18nc("@label:textbox", "Remote location:"), m_remoteRow);
    form->addRow(i18nc("@label:textbox", "rclone location:"), m_rcloneRow);
    form->addRow(i18nc("@label:listbox", "Mode:"), m_mode);
    form->addRow(QString(), m_mirror);
    form->addRow(i18nc("@label:listbox", "Saved filters:"), m_saved);
    form->addRow(i18nc("@label:textbox", "Include:"), m_include);
    form->addRow(i18nc("@label:textbox", "Exclude:"), m_exclude);
    form->addRow(QString(), m_recursive);
    form->addRow(i18nc("@label:spinbox", "Synchronize every:"), m_interval);
    form->addRow(QString(), m_paused);

    auto *note = new QLabel(i18nc("@info",
                                  "Passwords are not stored with the pair. Network folders ask for them through KDE Wallet, "
                                  "and rclone uses its own configuration."),
                            this);
    note->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &CloudSyncPairDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CloudSyncPairDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addWidget(m_error);
    layout->addWidget(buttons);

    connect(m_local, &KUrlRequester::textChanged, this, &CloudSyncPairDialog::validate);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &CloudSyncPairDialog::updateKind);
    connect(m_remote, &QLineEdit::textChanged, this, &CloudSyncPairDialog::validate);
    connect(m_rcloneRemote, &QLineEdit::textChanged, this, &CloudSyncPairDialog::validate);
    connect(m_mode, &QComboBox::currentIndexChanged, this, &CloudSyncPairDialog::updateMode);
    connect(m_include, &QPlainTextEdit::textChanged, this, &CloudSyncPairDialog::validate);
    connect(m_exclude, &QPlainTextEdit::textChanged, this, &CloudSyncPairDialog::validate);
    connect(m_recursive, &QCheckBox::toggled, this, &CloudSyncPairDialog::validate);
    connect(m_interval, &QSpinBox::valueChanged, this, &CloudSyncPairDialog::validate);

    updateKind();
    updateMode();
    resize(sizeHint().expandedTo(QSize(560, 0)));
}

Pair CloudSyncPairDialog::pair() const
{
    Pair pair = m_pair;
    const QUrl local = m_local->url();
    pair.localPath = local.isLocalFile() ? local.toLocalFile() : m_local->text().trimmed();
    if (m_kind->currentIndex() == 1) {
        pair.remote = QUrl();
        pair.rcloneRemote = m_rcloneRemote->text().trimmed();
    } else {
        // Pairs never keep a password; KIO asks for credentials when it
        // needs them.
        pair.remote = QUrl(m_remote->text().trimmed());
        pair.remote.setPassword(QString());
        pair.rcloneRemote.clear();
    }
    pair.mode = Mode(m_mode->currentData().toInt());
    pair.mirrorDeletions = pair.mode != Mode::Sync && m_mirror->isChecked();
    pair.options.includePatterns = patterns(m_include);
    pair.options.excludePatterns = patterns(m_exclude);
    pair.options.recursive = m_recursive->isChecked();
    pair.intervalMinutes = m_interval->value();
    pair.paused = m_paused->isChecked();
    return pair;
}

std::optional<std::pair<QString, QString>> CloudSyncPairDialog::outerExclusion() const
{
    return m_outerExclusion;
}

void CloudSyncPairDialog::setEngineLocations(const EngineLocations &locations)
{
    m_engineLocations = locations;
    validate();
}

void CloudSyncPairDialog::updateKind()
{
    auto *form = qobject_cast<QFormLayout *>(static_cast<QVBoxLayout *>(layout())->itemAt(0)->layout());
    const bool rclone = m_kind->currentIndex() == 1;
    form->setRowVisible(m_remoteRow, !rclone);
    form->setRowVisible(m_rcloneRow, rclone);
    validate();
}

void CloudSyncPairDialog::updateMode()
{
    m_mirror->setEnabled(Mode(m_mode->currentData().toInt()) != Mode::Sync);
    validate();
}

void CloudSyncPairDialog::validate()
{
    const Pair current = pair();
    QString error = pairError(current, m_others);
    if (error.isEmpty()) {
        const QString engine = managingEngine(current.localPath, m_engineLocations);
        if (!engine.isEmpty()) {
            error = i18nc("@info %1 is a synchronization client such as Nextcloud",
                          "%1 already synchronizes this folder, or a folder inside or around it. "
                          "Synchronizing it twice could duplicate or lose changes.",
                          engine);
        }
    }
    m_error->setText(error);
    m_error->setVisible(!error.isEmpty());
    if (m_ok) {
        m_ok->setEnabled(error.isEmpty());
    }
}

void CloudSyncPairDialog::accept()
{
    validate();
    if (!m_ok->isEnabled()) {
        return;
    }
    const Pair current = pair();
    m_outerExclusion.reset();
    if (const auto nested = nesting(current, m_others)) {
        const auto other = std::find_if(m_others.cbegin(), m_others.cend(), [&nested](const Pair &pair) {
            return pair.id == nested->otherId;
        });
        const QString otherName = other == m_others.cend() ? QString() : other->localPath;
        const QString question = nested->inside ? i18nc("@info %1 is a folder, %2 a path inside it",
                                                        "This pair is inside the pair for %1. Synchronizing %2 in both pairs can copy its changes twice.\n\n"
                                                        "Exclude it from the outer pair?",
                                                        otherName,
                                                        nested->relativePath)
                                                : i18nc("@info %1 is a folder, %2 a path inside it",
                                                        "The pair for %1 is inside this pair. Synchronizing %2 in both pairs can copy its changes twice.\n\n"
                                                        "Exclude it from this pair?",
                                                        otherName,
                                                        nested->relativePath);
        QMessageBox box(QMessageBox::Question,
                        i18nc("@title:window", "Nested Pairs"),
                        question,
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                        this);
        box.button(QMessageBox::Yes)->setText(i18nc("@action:button", "Exclude from Outer Pair"));
        box.button(QMessageBox::No)->setText(i18nc("@action:button", "Synchronize Both"));
        box.setDefaultButton(QMessageBox::Yes);
        const int answer = box.exec();
        if (answer == QMessageBox::Cancel) {
            return;
        }
        if (answer == QMessageBox::Yes) {
            const QString pattern = exactPattern(nested->relativePath);
            if (nested->inside) {
                m_outerExclusion = std::pair{nested->otherId, pattern};
            } else {
                QStringList excluded = patterns(m_exclude);
                if (!excluded.contains(pattern)) {
                    excluded.append(pattern);
                    m_exclude->setPlainText(excluded.join(QLatin1Char('\n')));
                }
            }
        }
    }
    QDialog::accept();
}

void CloudSyncPairDialog::populateAccounts()
{
    QMenu *menu = m_accounts->menu();
    menu->clear();
    QString error;
    const QList<OnlineAccounts::Account> accounts = OnlineAccounts::readAccounts(&error);
    const OnlineAccounts::ProtocolCheck known = [](const QString &protocol) {
        return KProtocolInfo::isKnownProtocol(protocol);
    };
    for (const OnlineAccounts::Account &account : accounts) {
        const QString title = account.providerDisplayName.isEmpty()
            ? account.displayName
            : i18nc("@title:menu account name (provider name)", "%1 (%2)", account.displayName, account.providerDisplayName);
        menu->addSection(QIcon::fromTheme(account.iconName), menuText(title));
        const QList<OnlineAccounts::Location> locations = OnlineAccounts::locations(account, known);
        for (const OnlineAccounts::Location &location : locations) {
            QAction *action = menu->addAction(QIcon::fromTheme(location.iconName, QIcon::fromTheme(QStringLiteral("folder-cloud"))), menuText(location.title));
            if (!location.isAvailable()) {
                action->setEnabled(false);
                action->setToolTip(location.unavailableReason);
                continue;
            }
            action->setToolTip(location.url.toDisplayString());
            const QUrl url = location.url;
            connect(action, &QAction::triggered, this, [this, url] {
                m_remote->setText(url.toString(QUrl::RemovePassword));
            });
        }
    }
    if (accounts.isEmpty() && error.isEmpty()) {
        menu->addAction(i18nc("@action:inmenu", "No online accounts"))->setEnabled(false);
    }
    if (!error.isEmpty()) {
        menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")), menuText(error))->setEnabled(false);
    }
}

void CloudSyncPairDialog::populateRemotes(const QList<Rclone::Remote> &remotes, const QString &error)
{
    QMenu *menu = m_remotes->menu();
    menu->clear();
    for (const Rclone::Remote &remote : remotes) {
        const QString text = remote.type.isEmpty() ? remote.name : i18nc("@item:inmenu rclone remote (type)", "%1 (%2)", remote.name, remote.type);
        const QString name = remote.name;
        connect(menu->addAction(menuText(text)), &QAction::triggered, this, [this, name] {
            m_rcloneRemote->setText(name + QLatin1Char(':'));
            m_rcloneRemote->setFocus();
        });
    }
    if (remotes.isEmpty() && error.isEmpty()) {
        menu->addAction(i18nc("@action:inmenu", "No rclone remotes"))->setEnabled(false);
    }
    if (!error.isEmpty()) {
        menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")), menuText(error))->setEnabled(false);
    }
}

CloudSyncReviewDialog::CloudSyncReviewDialog(const Pair &pair, const Report &report, QWidget *parent)
    : QDialog(parent)
    , m_changes(new QTreeWidget(this))
    , m_conflicts(new QTreeWidget(this))
{
    setWindowTitle(i18nc("@title:window", "Review Synchronization"));
    auto *layout = new QVBoxLayout(this);

    auto *heading = new QLabel(i18nc("@info %1 local folder, %2 remote location", "%1 and %2", pair.localPath, remoteDisplayName(pair)), this);
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    QFont bold = heading->font();
    bold.setBold(true);
    heading->setFont(bold);
    layout->addWidget(heading);
    if (!report.message.isEmpty()) {
        auto *message = new QLabel(report.message, this);
        message->setObjectName(QStringLiteral("cloudSyncReviewMessage"));
        message->setTextFormat(Qt::PlainText);
        message->setWordWrap(true);
        layout->addWidget(message);
    }

    m_changes->setObjectName(QStringLiteral("cloudSyncChanges"));
    m_changes->setHeaderLabels({i18nc("@title:column", "Item"), i18nc("@title:column", "Change")});
    m_changes->setRootIsDecorated(false);
    m_changes->setUniformRowHeights(true);
    int deletions = 0;
    for (const PreviewItem &change : report.preview) {
        auto *item = new QTreeWidgetItem(m_changes, {change.path, change.description});
        if (change.deletion) {
            ++deletions;
            item->setIcon(0, SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")));
        }
    }
    m_changes->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_changes->header()->setStretchLastSection(false);
    m_changes->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    QString changesTitle = report.preview.isEmpty() ? i18nc("@info", "Nothing needs to be copied, renamed or deleted.")
                                                    : i18ncp("@info", "%1 change:", "%1 changes:", report.preview.size());
    if (deletions) {
        changesTitle += QLatin1Char(' ') + i18ncp("@info", "%1 item will be deleted.", "%1 items will be deleted.", deletions);
    }
    auto *changesLabel = new QLabel(changesTitle, this);
    changesLabel->setObjectName(QStringLiteral("cloudSyncChangesSummary"));
    changesLabel->setWordWrap(true);
    layout->addWidget(changesLabel);
    layout->addWidget(m_changes, 2);
    m_changes->setVisible(!report.preview.isEmpty());

    if (!report.skipped.isEmpty()) {
        auto *skipped = new QLabel(i18ncp("@info",
                                          "%1 change on the other side is left alone in this mode.",
                                          "%1 changes on the other side are left alone in this mode.",
                                          report.skipped.size()),
                                   this);
        skipped->setObjectName(QStringLiteral("cloudSyncSkipped"));
        skipped->setWordWrap(true);
        skipped->setToolTip(report.skipped.mid(0, 20).join(QLatin1Char('\n')));
        layout->addWidget(skipped);
    }

    m_conflicts->setObjectName(QStringLiteral("cloudSyncConflicts"));
    m_conflicts->setHeaderLabels({i18nc("@title:column", "Conflict"),
                                  i18nc("@title:column", "Local Version"),
                                  i18nc("@title:column", "Remote Version"),
                                  i18nc("@title:column", "Resolution")});
    m_conflicts->setRootIsDecorated(false);
    for (const Conflict &conflict : report.conflicts) {
        auto *item = new QTreeWidgetItem(m_conflicts, {conflict.path, stateText(conflict.local), stateText(conflict.remote), QString()});
        if (!conflict.isResolvable()) {
            item->setText(3, i18nc("@item", "Rename one of them"));
            item->setToolTip(3, i18nc("@info:tooltip", "A file and a folder have the same name. Rename one of them, then synchronize again."));
            continue;
        }
        auto *combo = new QComboBox(m_conflicts);
        addResolutions(combo);
        m_conflicts->setItemWidget(item, 3, combo);
        m_choices.append({conflict.path, combo});
    }
    m_conflicts->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_conflicts->header()->setStretchLastSection(false);
    for (int column = 1; column < 4; ++column) {
        m_conflicts->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    if (!report.conflicts.isEmpty()) {
        auto *conflictsLabel = new QLabel(i18ncp("@info",
                                                 "%1 item changed on both sides. Choose which version to keep; skipped conflicts are shown again next time.",
                                                 "%1 items changed on both sides. Choose which version to keep; skipped conflicts are shown again next time.",
                                                 report.conflicts.size()),
                                          this);
        conflictsLabel->setWordWrap(true);
        layout->addWidget(conflictsLabel);
        layout->addWidget(m_conflicts, 1);
        if (!m_choices.isEmpty()) {
            auto *all = new QComboBox(this);
            all->setObjectName(QStringLiteral("cloudSyncResolveAll"));
            addResolutions(all);
            auto *apply = new QPushButton(i18nc("@action:button", "Apply to All"), this);
            apply->setObjectName(QStringLiteral("cloudSyncApplyAll"));
            connect(apply, &QPushButton::clicked, this, [this, all] {
                for (const auto &[path, combo] : std::as_const(m_choices)) {
                    combo->setCurrentIndex(all->currentIndex());
                }
            });
            auto *row = new QHBoxLayout;
            row->addStretch();
            row->addWidget(all);
            row->addWidget(apply);
            layout->addLayout(row);
        }
    } else {
        m_conflicts->hide();
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *synchronize = buttons->addButton(i18nc("@action:button", "Synchronize"), QDialogButtonBox::AcceptRole);
    synchronize->setObjectName(QStringLiteral("cloudSyncConfirm"));
    synchronize->setIcon(SymbolicIcon::fromTheme(QStringLiteral("folder-sync")));
    synchronize->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(QSize(720, 480));
}

QHash<QString, Resolution> CloudSyncReviewDialog::resolutions() const
{
    QHash<QString, Resolution> result;
    for (const auto &[path, combo] : m_choices) {
        result.insert(path, Resolution(combo->currentData().toInt()));
    }
    return result;
}

CloudSyncDialog::CloudSyncDialog(Scheduler *scheduler, KSharedConfig::Ptr config, RcloneMounts *rclone, QWidget *parent)
    : QDialog(parent)
    , m_scheduler(scheduler)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_rclone(rclone)
    , m_pairs(new QTreeWidget(this))
    , m_add(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add…"), this))
    , m_edit(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "Edit…"), this))
    , m_remove(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove"), this))
    , m_syncNow(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("folder-sync")), i18nc("@action:button", "Sync Now"), this))
    , m_review(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("view-preview")), i18nc("@action:button", "Review…"), this))
    , m_cancel(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("process-stop")), i18nc("@action:button", "Stop"), this))
    , m_pause(new QPushButton(this))
    , m_pauseAccount(new QPushButton(this))
    , m_startAtLogin(new QCheckBox(i18nc("@option:check", "Synchronize in the background after login"), this))
    , m_metered(new QCheckBox(i18nc("@option:check", "Pause background synchronization on metered connections"), this))
    , m_message(new QLabel(this))
{
    setWindowTitle(i18nc("@title:window", "Cloud Sync"));
    m_pairs->setObjectName(QStringLiteral("cloudSyncPairs"));
    m_pairs->setHeaderLabels(
        {i18nc("@title:column", "Local Folder"), i18nc("@title:column", "Remote"), i18nc("@title:column", "Mode"), i18nc("@title:column", "Status")});
    m_pairs->setRootIsDecorated(false);
    m_pairs->setUniformRowHeights(true);
    m_pairs->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pairs->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_pairs->header()->setStretchLastSection(true);

    m_add->setObjectName(QStringLiteral("cloudSyncAdd"));
    m_edit->setObjectName(QStringLiteral("cloudSyncEdit"));
    m_remove->setObjectName(QStringLiteral("cloudSyncRemove"));
    m_syncNow->setObjectName(QStringLiteral("cloudSyncNow"));
    m_review->setObjectName(QStringLiteral("cloudSyncReview"));
    m_cancel->setObjectName(QStringLiteral("cloudSyncCancel"));
    m_pause->setObjectName(QStringLiteral("cloudSyncPause"));
    m_pauseAccount->setObjectName(QStringLiteral("cloudSyncPauseAccount"));
    m_startAtLogin->setObjectName(QStringLiteral("cloudSyncStartAtLogin"));
    m_metered->setObjectName(QStringLiteral("cloudSyncMetered"));
    m_message->setObjectName(QStringLiteral("cloudSyncMessage"));
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    m_message->hide();
    m_review->setToolTip(i18nc("@info:tooltip", "Show what the next synchronization would change, and resolve conflicts"));

    auto *side = new QVBoxLayout;
    for (QPushButton *button : {m_add, m_edit, m_remove}) {
        side->addWidget(button);
    }
    side->addSpacing(12);
    for (QPushButton *button : {m_syncNow, m_review, m_cancel, m_pause, m_pauseAccount}) {
        side->addWidget(button);
    }
    side->addStretch();
    auto *top = new QHBoxLayout;
    top->addWidget(m_pairs, 1);
    top->addLayout(side);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top, 1);
    layout->addWidget(m_message);
    layout->addWidget(m_startAtLogin);
    layout->addWidget(m_metered);
    layout->addWidget(buttons);

    m_startAtLogin->setChecked(startsAtLogin());
    m_startAtLogin->setToolTip(i18nc("@info:tooltip", "Start Plasma Opus in the system tray after login, so pairs synchronize on their intervals"));
    connect(m_startAtLogin, &QCheckBox::toggled, this, [this](bool checked) {
        QString error;
        if (!setStartAtLogin(checked, QCoreApplication::applicationFilePath(), AppIdentity::desktopFileName(), &error)) {
            const QSignalBlocker blocker(m_startAtLogin);
            m_startAtLogin->setChecked(!checked);
            showMessage(error);
        }
    });
    if (m_scheduler) {
        m_metered->setChecked(m_scheduler->pausesOnMeteredConnections());
    }
    connect(m_metered, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_scheduler) {
            m_scheduler->setPausesOnMeteredConnections(checked);
        }
    });

    connect(m_add, &QPushButton::clicked, this, &CloudSyncDialog::addPair);
    connect(m_edit, &QPushButton::clicked, this, qOverload<>(&CloudSyncDialog::editPair));
    connect(m_pairs, &QTreeWidget::itemActivated, this, qOverload<>(&CloudSyncDialog::editPair));
    connect(m_remove, &QPushButton::clicked, this, &CloudSyncDialog::removePair);
    connect(m_syncNow, &QPushButton::clicked, this, [this] {
        const QString id = currentId();
        if (m_scheduler && !id.isEmpty()) {
            m_awaitingReview.insert(id);
            m_scheduler->syncNow(id);
        }
    });
    connect(m_review, &QPushButton::clicked, this, [this] {
        review(currentId());
    });
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        const QString id = currentId();
        if (m_scheduler && !id.isEmpty()) {
            m_awaitingReview.remove(id);
            m_scheduler->cancel(id);
        }
    });
    connect(m_pause, &QPushButton::clicked, this, [this] {
        if (!m_scheduler) {
            return;
        }
        QList<Pair> pairs = m_scheduler->pairs();
        for (Pair &pair : pairs) {
            if (pair.id == currentId()) {
                pair.paused = !pair.paused;
            }
        }
        QString error;
        if (!m_scheduler->setPairs(pairs, &error)) {
            showMessage(error);
        }
    });
    connect(m_pauseAccount, &QPushButton::clicked, this, [this] {
        const auto pair = m_scheduler ? m_scheduler->pair(currentId()) : std::nullopt;
        if (!pair) {
            return;
        }
        const QString account = accountKey(*pair);
        m_scheduler->setAccountPaused(account, !m_scheduler->pausedAccounts().contains(account));
    });
    connect(m_pairs, &QTreeWidget::currentItemChanged, this, &CloudSyncDialog::updateButtons);

    if (m_scheduler) {
        connect(m_scheduler, &Scheduler::statusChanged, this, [this](const QString &id) {
            if (!itemFor(id) && m_scheduler->pair(id)) {
                refresh();
            } else if (itemFor(id) && !m_scheduler->pair(id)) {
                refresh();
            } else {
                updateRow(id);
            }
            if (m_awaitingReview.contains(id) && m_scheduler->state(id) == Scheduler::State::Idle) {
                m_awaitingReview.remove(id);
                const Report report = m_scheduler->lastReport(id);
                if (report.outcome == Report::Outcome::NeedsReview) {
                    showReview(id, report);
                }
            }
        });
    }
    refresh();
    resize(QSize(820, 420));
}

void CloudSyncDialog::review(const QString &id)
{
    if (!m_scheduler || id.isEmpty() || !m_scheduler->pair(id)) {
        return;
    }
    if (QTreeWidgetItem *item = itemFor(id)) {
        m_pairs->setCurrentItem(item);
    }
    if (m_scheduler->needsReview(id) && m_scheduler->state(id) == Scheduler::State::Idle) {
        showReview(id, m_scheduler->lastReport(id));
        return;
    }
    m_awaitingReview.insert(id);
    m_scheduler->preview(id);
}

void CloudSyncDialog::refresh()
{
    const QString selected = currentId();
    m_pairs->clear();
    if (m_scheduler) {
        const QList<Pair> pairs = m_scheduler->pairs();
        for (const Pair &pair : pairs) {
            auto *item = new QTreeWidgetItem(m_pairs);
            item->setData(LocalColumn, Qt::UserRole, pair.id);
            updateRow(pair.id);
            if (pair.id == selected) {
                m_pairs->setCurrentItem(item);
            }
        }
    }
    if (!m_pairs->currentItem() && m_pairs->topLevelItemCount() > 0) {
        m_pairs->setCurrentItem(m_pairs->topLevelItem(0));
    }
    for (int column = 0; column < StatusColumn; ++column) {
        m_pairs->resizeColumnToContents(column);
    }
    updateButtons();
}

void CloudSyncDialog::updateRow(const QString &id)
{
    QTreeWidgetItem *item = itemFor(id);
    const auto pair = m_scheduler ? m_scheduler->pair(id) : std::nullopt;
    if (!item || !pair) {
        return;
    }
    item->setText(LocalColumn, pair->localPath);
    item->setIcon(LocalColumn, SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")));
    item->setText(RemoteColumn, remoteDisplayName(*pair));
    item->setText(ModeColumn, modeName(pair->mode));

    const Report report = m_scheduler->lastReport(id);
    QString status;
    QString icon;
    switch (m_scheduler->state(id)) {
    case Scheduler::State::Running:
        status = m_scheduler->progress(id);
        if (status.isEmpty()) {
            status = i18nc("@info:status", "Synchronizing…");
        }
        icon = QStringLiteral("view-refresh");
        break;
    case Scheduler::State::Queued:
        status = i18nc("@info:status", "Queued");
        icon = QStringLiteral("chronometer");
        break;
    case Scheduler::State::Idle:
        status = summary(report);
        if (report.outcome == Report::Outcome::NeedsReview || !report.conflicts.isEmpty()) {
            icon = QStringLiteral("dialog-warning");
        } else if (report.outcome == Report::Outcome::Failed || report.outcome == Report::Outcome::Partial) {
            icon = QStringLiteral("dialog-error");
        } else if (report.outcome == Report::Outcome::Offline) {
            icon = QStringLiteral("network-disconnect");
        } else if (report.outcome == Report::Outcome::Succeeded) {
            icon = QStringLiteral("checkmark");
        }
        if (pair->paused) {
            status = i18nc("@info:status %1 is the last result", "Paused. %1", status);
            icon = QStringLiteral("media-playback-pause");
        } else if (m_scheduler->isPaused(*pair)) {
            status = i18nc("@info:status %1 is the last result", "Account paused. %1", status);
            icon = QStringLiteral("media-playback-pause");
        }
        break;
    }
    item->setText(StatusColumn, status);
    item->setIcon(StatusColumn, icon.isEmpty() ? QIcon() : SymbolicIcon::fromTheme(icon));
    QStringList details{status};
    for (const ItemError &error : std::as_const(report.errors)) {
        if (details.size() > 10) {
            break;
        }
        details.append(error.path.isEmpty() ? error.message : i18nc("@info:tooltip %1 path, %2 error", "%1: %2", error.path, error.message));
    }
    if (report.freeBytes >= 0) {
        details.append(i18nc("@info:tooltip", "%1 free on the remote", QLocale().formattedDataSize(report.freeBytes)));
    }
    if (report.finishedAt.isValid()) {
        details.append(i18nc("@info:tooltip", "Last run: %1", QLocale().toString(report.finishedAt, QLocale::ShortFormat)));
    }
    item->setToolTip(StatusColumn, details.join(QLatin1Char('\n')));
    if (item == m_pairs->currentItem()) {
        updateButtons();
    }
}

void CloudSyncDialog::updateButtons()
{
    const QString id = currentId();
    const auto pair = m_scheduler && !id.isEmpty() ? m_scheduler->pair(id) : std::nullopt;
    const bool selected = pair.has_value();
    const Scheduler::State state = selected ? m_scheduler->state(id) : Scheduler::State::Idle;
    m_add->setEnabled(m_scheduler);
    m_edit->setEnabled(selected);
    m_remove->setEnabled(selected);
    m_syncNow->setEnabled(selected && state == Scheduler::State::Idle);
    m_review->setEnabled(selected && state == Scheduler::State::Idle);
    m_cancel->setEnabled(selected && state != Scheduler::State::Idle);
    m_pause->setEnabled(selected);
    m_pauseAccount->setEnabled(selected);
    const bool paused = selected && pair->paused;
    m_pause->setText(paused ? i18nc("@action:button", "Resume") : i18nc("@action:button", "Pause"));
    m_pause->setIcon(SymbolicIcon::fromTheme(paused ? QStringLiteral("media-playback-start") : QStringLiteral("media-playback-pause")));
    const bool accountPaused = selected && m_scheduler->pausedAccounts().contains(accountKey(*pair));
    m_pauseAccount->setText(accountPaused ? i18nc("@action:button", "Resume Account") : i18nc("@action:button", "Pause Account"));
    m_pauseAccount->setIcon(SymbolicIcon::fromTheme(accountPaused ? QStringLiteral("media-playback-start") : QStringLiteral("media-playback-pause")));
    m_pauseAccount->setToolTip(selected ? i18nc("@info:tooltip %1 is an account", "Pause or resume every pair of %1", accountDisplayName(*pair)) : QString());
}

QString CloudSyncDialog::currentId() const
{
    const QTreeWidgetItem *item = m_pairs->currentItem();
    return item ? item->data(LocalColumn, Qt::UserRole).toString() : QString();
}

QTreeWidgetItem *CloudSyncDialog::itemFor(const QString &id) const
{
    for (int i = 0; i < m_pairs->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_pairs->topLevelItem(i);
        if (item->data(LocalColumn, Qt::UserRole).toString() == id) {
            return item;
        }
    }
    return nullptr;
}

void CloudSyncDialog::addPair()
{
    Pair pair;
    pair.intervalMinutes = 15;
    editPair(pair, true);
}

void CloudSyncDialog::editPair()
{
    const auto pair = m_scheduler ? m_scheduler->pair(currentId()) : std::nullopt;
    if (pair) {
        editPair(*pair, false);
    }
}

void CloudSyncDialog::editPair(const Pair &pair, bool isNew)
{
    if (!m_scheduler) {
        return;
    }
    QList<Pair> others = m_scheduler->pairs();
    others.removeIf([&pair](const Pair &other) {
        return other.id == pair.id;
    });
    auto *dialog = new CloudSyncPairDialog(pair, others, m_config, m_rclone, this);
    dialog->setWindowTitle(isNew ? i18nc("@title:window", "Add Cloud Sync Pair") : i18nc("@title:window", "Edit Cloud Sync Pair"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        if (!m_scheduler) {
            return;
        }
        const Pair edited = dialog->pair();
        const auto exclusion = dialog->outerExclusion();
        QList<Pair> pairs = m_scheduler->pairs();
        bool replaced = false;
        for (Pair &existing : pairs) {
            if (existing.id == edited.id) {
                existing = edited;
                replaced = true;
            } else if (exclusion && existing.id == exclusion->first && !existing.options.excludePatterns.contains(exclusion->second)) {
                existing.options.excludePatterns.append(exclusion->second);
            }
        }
        if (!replaced) {
            pairs.append(edited);
        }
        QString error;
        if (!m_scheduler->setPairs(pairs, &error)) {
            showMessage(error);
            return;
        }
        m_message->hide();
        refresh();
        if (QTreeWidgetItem *item = itemFor(edited.id)) {
            m_pairs->setCurrentItem(item);
        }
    });
    dialog->open();
}

void CloudSyncDialog::removePair()
{
    const QString id = currentId();
    if (!m_scheduler || id.isEmpty()) {
        return;
    }
    // Removing a pair keeps both folders; only the pair and its snapshot go.
    QList<Pair> pairs = m_scheduler->pairs();
    pairs.removeIf([&id](const Pair &pair) {
        return pair.id == id;
    });
    m_awaitingReview.remove(id);
    QString error;
    if (!m_scheduler->setPairs(pairs, &error)) {
        showMessage(error);
        return;
    }
    refresh();
}

void CloudSyncDialog::showReview(const QString &id, const Report &report)
{
    const auto pair = m_scheduler ? m_scheduler->pair(id) : std::nullopt;
    if (!pair) {
        return;
    }
    if (m_reviewDialog) {
        m_reviewDialog->close();
    }
    auto *dialog = new CloudSyncReviewDialog(*pair, report, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_reviewDialog = dialog;
    connect(dialog, &QDialog::accepted, this, [this, dialog, id, report] {
        if (m_scheduler) {
            m_scheduler->confirm(id, dialog->resolutions(), report);
        }
    });
    dialog->open();
}

void CloudSyncDialog::showMessage(const QString &message)
{
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
}
