// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "settings.h"
#include "settingswidgets.h"

#include <KLocalizedString>
#include <KUrlRequester>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

namespace
{
QComboBox *comboBox(const QString &entry, const QStringList &choices)
{
    auto *combo = new QComboBox;
    combo->setObjectName(QStringLiteral("kcfg_") + entry);
    combo->addItems(choices);
    return combo;
}

QCheckBox *checkBox(const QString &entry, const QString &text)
{
    auto *check = new QCheckBox(text);
    check->setObjectName(QStringLiteral("kcfg_") + entry);
    return check;
}

QSpinBox *pixelSpinBox(const QString &entry, int minimum, int maximum)
{
    auto *spin = new QSpinBox;
    spin->setObjectName(QStringLiteral("kcfg_") + entry);
    spin->setRange(minimum, maximum);
    spin->setSuffix(i18nc("@item:valuesuffix pixels", " px"));
    return spin;
}

template<typename Editor>
Editor *bound(Editor *editor, const QString &entry, const char *property)
{
    editor->setObjectName(QStringLiteral("kcfg_") + entry);
    editor->setProperty("kcfg_property", QByteArray(property));
    return editor;
}

KUrlRequester *folderRequester(const QString &entry)
{
    auto *requester = new KUrlRequester;
    requester->setObjectName(QStringLiteral("kcfg_") + entry);
    requester->setMode(KFile::Directory | KFile::ExistingOnly);
    return requester;
}
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : KConfigDialog(parent, QLatin1String(Name), Settings::self())
{
    setFaceType(KPageDialog::List);
    addPage(createStartupPage(), i18nc("@title:tab", "Startup"), QStringLiteral("go-home"));
    addPage(createPanesPage(), i18nc("@title:tab", "Panes and Tabs"), QStringLiteral("view-split-left-right"));
    addPage(createViewsPage(), i18nc("@title:tab", "Views"), QStringLiteral("view-list-details"));
    addPage(createPreviewsPage(), i18nc("@title:tab", "Previews"), QStringLiteral("view-preview"));
}

QWidget *SettingsDialog::createStartupPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    QComboBox *mode = comboBox(QStringLiteral("StartupMode"),
                               {i18nc("@item:inlistbox", "Restore the last session"),
                                i18nc("@item:inlistbox", "Open the home folder"),
                                i18nc("@item:inlistbox", "Open fixed folders")});
    KUrlRequester *left = folderRequester(QStringLiteral("LeftStartUrl"));
    KUrlRequester *right = folderRequester(QStringLiteral("RightStartUrl"));
    const auto updateFixed = [mode, left, right] {
        const bool fixed = mode->currentIndex() == Settings::EnumStartupMode::FixedFolders;
        left->setEnabled(fixed);
        right->setEnabled(fixed);
    };
    connect(mode, &QComboBox::currentIndexChanged, page, updateFixed);
    updateFixed();

    form->addRow(i18nc("@label:listbox", "On startup:"), mode);
    form->addRow(i18nc("@label:chooser", "First pane:"), left);
    form->addRow(i18nc("@label:chooser", "Second pane:"), right);
    form->addRow(i18nc("@label:chooser", "Home folder:"), folderRequester(QStringLiteral("HomeUrl")));
    return page;
}

QWidget *SettingsDialog::createPanesPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Start with two panes:"),
                 comboBox(QStringLiteral("DualPaneAtStartup"),
                          {i18nc("@item:inlistbox", "As last used"), i18nc("@item:inlistbox", "Always"), i18nc("@item:inlistbox", "Never")}));
    form->addRow(i18nc("@title:group", "Tabs:"), checkBox(QStringLiteral("AlwaysShowTabBar"), i18nc("@option:check", "Always show the tab bars")));
    form->addRow(QString(), checkBox(QStringLiteral("NewTabsAfterCurrent"), i18nc("@option:check", "Open new tabs next to the current tab")));
    form->addRow(i18nc("@title:group", "Panes:"), checkBox(QStringLiteral("TabSwitchesPanes"), i18nc("@option:check", "Tab key switches between the panes")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowNavigationBar"), i18nc("@option:check", "Show the navigation bar of each pane")));
    form->addRow(i18nc("@title:group", "Navigation:"),
                 checkBox(QStringLiteral("FollowSymlinks"), i18nc("@option:check", "Always follow symbolic links to their target")));
    return page;
}

QWidget *SettingsDialog::createViewsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "View mode in new windows:"),
                 comboBox(QStringLiteral("DefaultViewMode"), {i18nc("@item:inlistbox", "Details"), i18nc("@item:inlistbox", "Icons")}));
    form->addRow(i18nc("@label:listbox", "Sort by in new windows:"),
                 comboBox(QStringLiteral("SortColumn"),
                          {i18nc("@item:inlistbox", "Name"),
                           i18nc("@item:inlistbox", "Size"),
                           i18nc("@item:inlistbox", "Modified"),
                           i18nc("@item:inlistbox", "Permissions"),
                           i18nc("@item:inlistbox", "Owner"),
                           i18nc("@item:inlistbox", "Group"),
                           i18nc("@item:inlistbox", "Type")}));
    form->addRow(QString(), checkBox(QStringLiteral("SortDescending"), i18nc("@option:check", "Descending")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowHiddenFiles"), i18nc("@option:check", "Show hidden files in new windows")));
    form->addRow(i18nc("@title:group", "Sorting:"), checkBox(QStringLiteral("FoldersFirst"), i18nc("@option:check", "Folders before files")));
    form->addRow(i18nc("@label:spinbox", "Details icon size:"), pixelSpinBox(QStringLiteral("DetailsIconSize"), 16, 64));
    form->addRow(i18nc("@label:spinbox", "Icons and previews size:"), pixelSpinBox(QStringLiteral("IconsIconSize"), 32, 256));
    form->addRow(i18nc("@title:group", "Details columns:"), checkBox(QStringLiteral("ShowSizeColumn"), i18nc("@option:check", "Size")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowModifiedColumn"), i18nc("@option:check", "Modified")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowTypeColumn"), i18nc("@option:check", "Type")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowPermissionsColumn"), i18nc("@option:check", "Permissions")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowOwnerColumn"), i18nc("@option:check", "Owner")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowGroupColumn"), i18nc("@option:check", "Group")));
    form->addRow(i18nc("@title:group", "Tooltips:"), checkBox(QStringLiteral("ShowToolTips"), i18nc("@option:check", "Show the full name of shortened items")));
    return page;
}

QWidget *SettingsDialog::createPreviewsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowPreviews"), i18nc("@option:check", "Show previews in new windows")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowRemotePreviews"), i18nc("@option:check", "Show previews in remote folders")));
    form->addRow(i18nc("@label:spinbox", "Skip files larger than:"), bound(new PreviewSizeSpinBox, QStringLiteral("MaximumPreviewSize"), "bytes"));
    form->addRow(i18nc("@label", "Show previews for:"), bound(new PreviewPluginList, QStringLiteral("PreviewPlugins"), "plugins"));
    return page;
}
