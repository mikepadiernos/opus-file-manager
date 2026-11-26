// SPDX-License-Identifier: GPL-3.0-or-later

#include "chooserdialog.h"

#include "folderview.h"
#include "namefilter.h"
#include "symbolicicon.h"

#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KIO/Global>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KStandardGuiItem>
#include <KUrlNavigator>
#include <KWindowStateSaver>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

using namespace FileChooser;

namespace
{
QString defaultTitle(const Request &request)
{
    switch (request.mode) {
    case Mode::Open:
        if (request.directory) {
            return request.multiple ? i18nc("@title:window", "Select Folders") : i18nc("@title:window", "Select Folder");
        }
        return request.multiple ? i18nc("@title:window", "Open Files") : i18nc("@title:window", "Open File");
    case Mode::Save:
        return i18nc("@title:window", "Save File");
    case Mode::SaveFiles:
        return i18nc("@title:window", "Save Files");
    }
    return {};
}

QString defaultAcceptLabel(const Request &request)
{
    if (request.mode != Mode::Open) {
        return i18nc("@action:button", "&Save");
    }
    return request.directory ? i18nc("@action:button", "&Select") : i18nc("@action:button", "&Open");
}

QString filterLabel(const Filter &filter)
{
    if (!filter.name.isEmpty()) {
        return filter.name;
    }
    QStringList patterns;
    for (const FilterRule &rule : filter.rules) {
        patterns.append(rule.pattern);
    }
    return patterns.join(QStringLiteral(", "));
}
}

ChooserDialog::ChooserDialog(const Request &request, QWidget *parent)
    : QDialog(parent)
    , m_request(request)
    , m_placesModel(new KFilePlacesModel(this))
    , m_places(new KFilePlacesView(this))
    , m_navigator(new KUrlNavigator(m_placesModel, QUrl(), this))
    , m_view(new FolderView(this))
    , m_search(new QLineEdit(this))
    , m_message(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("fileChooserDialog"));
    setWindowTitle(request.title.isEmpty() ? defaultTitle(request) : request.title);

    auto *up = new QToolButton(this);
    up->setObjectName(QStringLiteral("fileChooserUp"));
    up->setAutoRaise(true);
    up->setIcon(SymbolicIcon::fromTheme(QStringLiteral("go-up")));
    up->setToolTip(i18nc("@info:tooltip", "Go to the parent folder"));
    m_navigator->setObjectName(QStringLiteral("fileChooserLocation"));
    m_search->setObjectName(QStringLiteral("fileChooserSearch"));
    m_search->setPlaceholderText(i18nc("@info:placeholder", "Filter by name…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(fontMetrics().averageCharWidth() * 30);

    auto *locationRow = new QHBoxLayout;
    locationRow->addWidget(up);
    locationRow->addWidget(m_navigator, 1);
    locationRow->addWidget(m_search);

    m_places->setObjectName(QStringLiteral("fileChooserPlaces"));
    m_places->setModel(m_placesModel);
    m_view->setObjectName(QStringLiteral("fileChooserView"));
    auto *splitter = new QSplitter(this);
    splitter->setObjectName(QStringLiteral("fileChooserSplitter"));
    splitter->addWidget(m_places);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({fontMetrics().averageCharWidth() * 24, fontMetrics().averageCharWidth() * 80});

    m_message->setObjectName(QStringLiteral("fileChooserMessage"));
    m_message->setMessageType(KMessageWidget::Error);
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->hide();

    auto *form = new QFormLayout;
    if (request.mode == Mode::Save) {
        m_name = new QLineEdit(this);
        m_name->setObjectName(QStringLiteral("fileChooserName"));
        m_name->setText(initialName(request));
        form->addRow(i18nc("@label:textbox", "Name:"), m_name);
        connect(m_name, &QLineEdit::textChanged, this, &ChooserDialog::updateAcceptButton);
    } else if (request.mode == Mode::SaveFiles && !request.files.isEmpty()) {
        auto *files = new QLabel(request.files.join(QStringLiteral(", ")), this);
        files->setObjectName(QStringLiteral("fileChooserFiles"));
        files->setWordWrap(true);
        form->addRow(i18ncp("@label", "File:", "Files:", request.files.size()), files);
    }
    if (!request.filters.isEmpty()) {
        m_filters = new QComboBox(this);
        m_filters->setObjectName(QStringLiteral("fileChooserFilter"));
        for (const Filter &filter : request.filters) {
            m_filters->addItem(filterLabel(filter));
        }
        m_filters->setCurrentIndex(qMax(0, request.currentFilter));
        form->addRow(i18nc("@label:listbox", "File type:"), m_filters);
        connect(m_filters, &QComboBox::currentIndexChanged, this, &ChooserDialog::applyViewFilter);
    }
    for (const Choice &choice : request.choices) {
        if (choice.options.isEmpty()) {
            auto *check = new QCheckBox(choice.label, this);
            check->setObjectName(QStringLiteral("fileChooserChoice-") + choice.id);
            check->setChecked(choice.initial == QLatin1String("true"));
            form->addRow(QString(), check);
            m_choices.append({choice.id, check});
            continue;
        }
        auto *combo = new QComboBox(this);
        combo->setObjectName(QStringLiteral("fileChooserChoice-") + choice.id);
        for (const ChoiceOption &option : choice.options) {
            combo->addItem(option.label, option.id);
        }
        combo->setCurrentIndex(qMax(0, combo->findData(choice.initial)));
        form->addRow(choice.label.isEmpty() ? QString() : i18nc("@label:listbox %1 is an application's option name", "%1:", choice.label), combo);
        m_choices.append({choice.id, combo});
    }

    QPushButton *ok = m_buttons->button(QDialogButtonBox::Ok);
    ok->setText(request.acceptLabel.isEmpty() ? defaultAcceptLabel(request) : request.acceptLabel);
    ok->setIcon(SymbolicIcon::fromTheme(request.mode == Mode::Open ? QStringLiteral("document-open") : QStringLiteral("document-save")));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ChooserDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &ChooserDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(locationRow);
    layout->addWidget(splitter, 1);
    layout->addWidget(m_message);
    layout->addLayout(form);
    layout->addWidget(m_buttons);

    connect(up, &QToolButton::clicked, this, [this] {
        setFolder(KIO::upUrl(folder()));
    });
    connect(m_view, &FolderView::goUpRequested, up, &QToolButton::click);
    connect(m_navigator, &KUrlNavigator::urlChanged, this, &ChooserDialog::setFolder);
    connect(m_places, &KFilePlacesView::placeActivated, this, &ChooserDialog::setFolder);
    connect(m_view, &FolderView::urlChanged, this, [this](const QUrl &url) {
        m_navigator->setLocationUrl(url);
        m_places->setUrl(url);
        updateAcceptButton();
    });
    connect(m_view, &FolderView::itemActivated, this, &ChooserDialog::onItemActivated);
    connect(m_view, &FolderView::selectedItemsChanged, this, [this] {
        const KFileItemList items = m_view->selectedItems();
        if (m_name && items.size() == 1 && !items.constFirst().isDir()) {
            m_name->setText(items.constFirst().name());
        }
        m_message->hide();
        updateAcceptButton();
    });
    connect(m_view, &FolderView::typeAheadRequested, this, [this](const QString &text) {
        m_search->setFocus();
        m_search->setText(m_search->text() + text);
    });
    connect(m_search, &QLineEdit::textChanged, this, &ChooserDialog::applyViewFilter);

    resize(fontMetrics().averageCharWidth() * 120, fontMetrics().height() * 32);
    new KWindowStateSaver(this, QStringLiteral("FileChooserDialog"));

    applyViewFilter();
    const QUrl initial = initialFolder(request);
    setFolder(initial.isValid() ? initial : QUrl::fromLocalFile(QDir::homePath()));
    if (m_name) {
        m_name->setFocus();
        const QString name = m_name->text();
        const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
        m_name->setSelection(0, dot > 0 ? dot : name.size());
    } else {
        m_view->setFocus();
    }
}

ChooserDialog::~ChooserDialog() = default;

const Request &ChooserDialog::request() const
{
    return m_request;
}

Result ChooserDialog::chooserResult() const
{
    return m_result;
}

QUrl ChooserDialog::folder() const
{
    return m_view->url();
}

void ChooserDialog::setFolder(const QUrl &url)
{
    if (!url.isValid() || url.matches(m_view->url(), QUrl::StripTrailingSlash)) {
        return;
    }
    m_view->setUrl(url);
}

FolderView *ChooserDialog::view() const
{
    return m_view;
}

QLineEdit *ChooserDialog::nameEdit() const
{
    return m_name;
}

QComboBox *ChooserDialog::filterCombo() const
{
    return m_filters;
}

QLineEdit *ChooserDialog::searchEdit() const
{
    return m_search;
}

QString ChooserDialog::errorText() const
{
    return m_message->isVisibleTo(this) ? m_message->text() : QString();
}

void ChooserDialog::setOverwriteConfirmation(const OverwriteConfirmation &confirmation)
{
    m_confirmOverwrite = confirmation;
}

QStringList ChooserDialog::currentGlobs() const
{
    if (m_filters) {
        return globPatterns(m_request.filters.value(m_filters->currentIndex()));
    }
    return globPatterns(m_request.fixedFilter);
}

void ChooserDialog::applyViewFilter()
{
    const QString search = m_search->text().trimmed();
    if (!search.isEmpty()) {
        m_view->setNameFilter(NameFilter(search, m_request.directory ? NameFilter::Scope::FilesAndFolders : NameFilter::Scope::Files));
        return;
    }
    if (m_request.directory) {
        // Names never contain a slash, so this hides every file.
        m_view->setNameFilter(NameFilter(QStringLiteral("*/"), NameFilter::Scope::Files));
        return;
    }
    const QStringList globs = currentGlobs();
    if (globs.isEmpty() || globs.contains(QStringLiteral("*"))) {
        m_view->setNameFilter(NameFilter());
        return;
    }
    m_view->setNameFilter(NameFilter(globs.join(QLatin1Char(' ')), NameFilter::Scope::Files));
}

void ChooserDialog::onItemActivated(const KFileItem &item)
{
    if (item.isNull()) {
        return;
    }
    if (item.isDir()) {
        setFolder(item.targetUrl());
        return;
    }
    if (m_request.mode == Mode::Open && !m_request.directory) {
        m_view->selectUrls({item.url()});
        accept();
    } else if (m_name) {
        m_name->setText(item.name());
        accept();
    }
}

void ChooserDialog::updateAcceptButton()
{
    bool enabled = true;
    if (m_name) {
        enabled = !m_name->text().trimmed().isEmpty();
    } else if (m_request.mode == Mode::Open && !m_request.directory) {
        enabled = !m_view->selectedItems().isEmpty();
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

void ChooserDialog::showError(const QString &message)
{
    m_message->setText(message);
    m_message->show();
}

bool ChooserDialog::toLocalUrls(QList<QUrl> *urls)
{
    for (QUrl &url : *urls) {
        if (url.isLocalFile()) {
            continue;
        }
        KIO::StatJob *job = KIO::mostLocalUrl(url, KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, this);
        if (job->exec() && job->mostLocalUrl().isLocalFile()) {
            url = job->mostLocalUrl();
            continue;
        }
        showError(i18nc("@info", "%1 is not a local file. Applications can only receive files on this computer.", url.toDisplayString(QUrl::PreferLocalFile)));
        return false;
    }
    return true;
}

bool ChooserDialog::confirmOverwrite(const QList<QUrl> &targets)
{
    QList<QUrl> existing;
    for (const QUrl &target : targets) {
        if (QFileInfo::exists(target.toLocalFile())) {
            existing.append(target);
        }
    }
    if (existing.isEmpty()) {
        return true;
    }
    if (m_confirmOverwrite) {
        return m_confirmOverwrite(existing);
    }
    if (existing.size() == 1) {
        return KMessageBox::warningContinueCancel(this,
                                                  i18nc("@info", "%1 already exists. Do you want to replace it?", existing.constFirst().fileName()),
                                                  i18nc("@title:window", "Replace File"),
                                                  KStandardGuiItem::overwrite())
            == KMessageBox::Continue;
    }
    QStringList names;
    for (const QUrl &url : std::as_const(existing)) {
        names.append(url.fileName());
    }
    return KMessageBox::warningContinueCancelList(this,
                                                  i18nc("@info", "These files already exist. Do you want to replace them?"),
                                                  names,
                                                  i18nc("@title:window", "Replace Files"),
                                                  KStandardGuiItem::overwrite())
        == KMessageBox::Continue;
}

void ChooserDialog::finishAccept(const QList<QUrl> &urls)
{
    m_result.urls = urls;
    m_result.filter = m_filters ? m_filters->currentIndex() : -1;
    m_result.choices.clear();
    for (const auto &[id, widget] : std::as_const(m_choices)) {
        if (auto *check = qobject_cast<QCheckBox *>(widget)) {
            m_result.choices.append({id, check->isChecked() ? QStringLiteral("true") : QStringLiteral("false")});
        } else if (auto *combo = qobject_cast<QComboBox *>(widget)) {
            m_result.choices.append({id, combo->currentData().toString()});
        }
    }
    QDialog::accept();
}

void ChooserDialog::accept()
{
    m_message->hide();
    if (m_request.mode == Mode::Open) {
        KFileItemList items = m_view->selectedItems();
        if (m_request.directory) {
            items.removeIf([](const KFileItem &item) {
                return !item.isDir();
            });
            if (items.isEmpty()) {
                items.append(m_view->rootItem().isNull() ? KFileItem(folder()) : m_view->rootItem());
            }
        } else {
            if (items.size() == 1 && items.constFirst().isDir()) {
                setFolder(items.constFirst().targetUrl());
                return;
            }
            items.removeIf([](const KFileItem &item) {
                return item.isDir();
            });
            if (items.isEmpty()) {
                showError(i18nc("@info", "Select a file."));
                return;
            }
        }
        if (!m_request.multiple && items.size() > 1) {
            showError(m_request.directory ? i18nc("@info", "Select only one folder.") : i18nc("@info", "Select only one file."));
            return;
        }
        QList<QUrl> urls;
        for (const KFileItem &item : std::as_const(items)) {
            urls.append(item.targetUrl());
        }
        if (toLocalUrls(&urls)) {
            finishAccept(urls);
        }
        return;
    }

    QList<QUrl> folderUrl{folder()};
    if (m_request.mode == Mode::SaveFiles) {
        const KFileItemList items = m_view->selectedItems();
        if (items.size() == 1 && items.constFirst().isDir()) {
            folderUrl = {items.constFirst().targetUrl()};
        }
    }
    if (!toLocalUrls(&folderUrl)) {
        return;
    }
    QList<QUrl> targets;
    if (m_request.mode == Mode::SaveFiles) {
        targets = saveFilesTargets(folderUrl.constFirst(), m_request.files);
    } else {
        const QString name = m_filters || !m_request.fixedFilter.rules.isEmpty() ? withFilterExtension(m_name->text(), currentGlobs()) : m_name->text();
        const QUrl target = saveTarget(folderUrl.constFirst(), name);
        if (target.isEmpty()) {
            showError(i18nc("@info", "Enter a file name."));
            return;
        }
        const QFileInfo info(target.toLocalFile());
        if (info.isDir()) {
            setFolder(target);
            m_name->clear();
            return;
        }
        if (!info.absoluteDir().exists()) {
            showError(i18nc("@info", "The folder %1 does not exist.", QDir::toNativeSeparators(info.absolutePath())));
            return;
        }
        targets = {target};
    }
    if (confirmOverwrite(targets)) {
        finishAccept(targets);
    }
}
