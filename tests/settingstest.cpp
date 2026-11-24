// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"
#include "appearancestyle.h"
#include "appfonts.h"
#include "carddelegate.h"
#include "colorschemes.h"
#include "combinedpanel.h"
#include "compressdialog.h"
#include "density.h"
#include "detailsview.h"
#include "fileoperations.h"
#include "foldermetadata.h"
#include "foldertree.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "previewpanel.h"
#include "settingsdialog.h"
#include "settingswidgets.h"
#include "tabcolors.h"
#include "tabmodel.h"
#include "treelines.h"
#include "windoweffects.h"

#include <KActionCollection>
#include <KColorButton>
#include <KDirModel>
#include <KFileItem>
#include <KFileItemDelegate>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KMessageWidget>
#include <KPageWidget>
#include <KSharedConfig>
#include <KToolBar>
#include <KUrlNavigator>
#include <KUrlRequester>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDropEvent>
#include <QFile>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSizeGrip>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include <limits>

class ZeroHorizontalMarginStyle : public QProxyStyle
{
public:
    ZeroHorizontalMarginStyle()
        : QProxyStyle(QStringLiteral("Fusion"))
    {
    }

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
    {
        if (metric == PM_LayoutLeftMargin || metric == PM_LayoutRightMargin) {
            return 0;
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

class SettingsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;

    QUrl url(const QString &relative) const
    {
        return QUrl::fromLocalFile(m_root.filePath(relative));
    }

    static KFilePlacesModel *placesOf(MainWindow &window)
    {
        return qobject_cast<KFilePlacesModel *>(window.findChild<KFilePlacesView *>()->model());
    }

    /// Title of the dialog tab that holds @p widget, or an empty string.
    static QString tabTitleOf(const QWidget *widget)
    {
        for (const QWidget *ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (const auto *tabs = qobject_cast<const QTabWidget *>(ancestor)) {
                for (int i = 0; i < tabs->count(); ++i) {
                    if (tabs->widget(i)->isAncestorOf(widget)) {
                        return tabs->tabText(i);
                    }
                }
            }
        }
        return {};
    }

    /// The sidebar the dialog currently shows.
    static QAbstractItemView *sidebarOf(SettingsDialog &dialog)
    {
        const auto views = dialog.findChildren<QAbstractItemView *>();
        for (QAbstractItemView *view : views) {
            if (view->model() == dialog.findChild<KPageWidget *>()->model() && view->isVisibleTo(&dialog)) {
                return view;
            }
        }
        return nullptr;
    }

    /// Theme name of the first sidebar page's icon.
    static QString firstPageIconName(SettingsDialog &dialog)
    {
        const QAbstractItemModel *pages = dialog.findChild<KPageWidget *>()->model();
        return pages->data(pages->index(0, 0), Qt::DecorationRole).value<QIcon>().name();
    }

    static QAction *action(MainWindow &window, const QString &name)
    {
        return window.actionCollection()->action(name);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString serviceMenus = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kio/servicemenus");
        QVERIFY(QDir().mkpath(serviceMenus));
        QFile serviceMenu(serviceMenus + QStringLiteral("/opusfm-settings-test.desktop"));
        QVERIFY(serviceMenu.open(QIODevice::WriteOnly));
        serviceMenu.write(
            "[Desktop Entry]\nType=Service\nMimeType=all/allfiles;\nActions=opusfmSettingsTest;\n\n"
            "[Desktop Action opusfmSettingsTest]\nName=Settings Test Action\nExec=true %F\n");
        serviceMenu.close();
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("c")));
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        QVERIFY(QFile::link(m_root.filePath(QStringLiteral("a")), m_root.filePath(QStringLiteral("link"))));
        m_a = url(QStringLiteral("a"));
        m_b = url(QStringLiteral("b"));
    }

    void init()
    {
        for (const QString &folder : {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}) {
            QFile::remove(m_root.filePath(folder + QStringLiteral("/.mfm.json")));
        }
        Settings::self()->setDefaults();
        // Closing windows with several tabs would otherwise block on a dialog.
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::self()->save();
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void defaultAppearanceAndBehavior()
    {
        QVERIFY(Settings::alwaysShowTabBar());
        QVERIFY(Settings::newTabsAfterCurrent());
        QCOMPARE(Settings::detailsIconSize(), 24);
        QCOMPARE(Settings::fileListMinimapWidth(), 50);
        QCOMPARE(Settings::previewPlacement(), Settings::EnumPreviewPlacement::Right);
        QVERIFY(!Settings::showPreviews());
        QVERIFY(Settings::terminalFollowActiveFolder());
        QVERIFY(Settings::linkPanesAtStartup());
        QVERIFY(Settings::galleryShowInformation());
        QCOMPARE(Settings::fileActivationDestination(), Settings::EnumFileActivationDestination::Preview);
        QVERIFY(!Settings::showPlaces());
        QVERIFY(!Settings::panelTitlesVisible());
        QVERIFY(Settings::informationHover());
        QCOMPARE(Settings::sidebarIconSize(), 16);
    }

    void dialogHasAWidgetForEverySetting()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const auto items = Settings::self()->items();
        for (const KConfigSkeletonItem *item : items) {
            QVERIFY2(dialog.findChild<QWidget *>(QStringLiteral("kcfg_") + item->name()), qPrintable(item->name()));
        }
    }

    void dialogApplyWritesSettings()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StartupMode"))->setCurrentIndex(Settings::EnumStartupMode::FixedFolders);
        dialog.findChild<KUrlRequester *>(QStringLiteral("kcfg_LeftStartUrl"))->setUrl(m_b);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FollowSymlinks"))->setChecked(true);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_FolderSizeRefreshInterval"))->setValue(60);
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(Settings::startupMode(), Settings::EnumStartupMode::FixedFolders);
        QCOMPARE(Settings::leftStartUrl(), m_b);
        QVERIFY(Settings::followSymlinks());
        QCOMPARE(Settings::folderSizeRefreshInterval(), 60);
    }

    void colorSchemesApplyOnlyToPlasmaOpus()
    {
        auto restore = qScopeGuard([] {
            ColorSchemes::apply(QString());
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *combo = dialog.findChild<ColorSchemeCombo *>(QStringLiteral("kcfg_ColorScheme"));
        QVERIFY(combo);
        QCOMPARE(combo->scheme(), QString());
        QCOMPARE(combo->itemData(1).toString(), QLatin1String(ColorSchemes::LightPreset));
        QCOMPARE(combo->itemData(2).toString(), QLatin1String(ColorSchemes::DarkPreset));
        const QStyle *style = QApplication::style();

        combo->setCurrentIndex(combo->findData(QLatin1String(ColorSchemes::DarkPreset)));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::colorScheme(), QLatin1String(ColorSchemes::DarkPreset));
        // Widgets pick up application palette changes from posted events.
        QTRY_COMPARE(window.palette().color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));
        QTRY_COMPARE(dialog.palette().color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));
        QCOMPARE(QApplication::style(), style);

        combo->setCurrentIndex(0);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::colorScheme().isEmpty());
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));
        QVERIFY(!qApp->property("KDE_COLOR_SCHEME_PATH").isValid());
    }

    void unavailableColorSchemesAreKeptAndReported()
    {
        Settings::setColorScheme(QStringLiteral("MissingTestScheme"));
        MainWindow window;
        QVERIFY(window.statusBar()->currentMessage().contains(QLatin1String("MissingTestScheme")));
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));
        SettingsDialog dialog(placesOf(window), &window);
        auto *combo = dialog.findChild<ColorSchemeCombo *>(QStringLiteral("kcfg_ColorScheme"));
        QCOMPARE(combo->scheme(), QStringLiteral("MissingTestScheme"));
        QVERIFY(combo->currentText().contains(QLatin1String("MissingTestScheme")));
    }

    void customColoursPreviewValidateAndRevert()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            ColorSchemes::apply(QString());
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *combo = dialog.findChild<ColorSchemeCombo *>(QStringLiteral("kcfg_ColorScheme"));
        auto *editor = dialog.findChild<ColorPaletteEditor *>(QStringLiteral("kcfg_CustomColors"));
        QVERIFY(combo && editor);
        auto *preview = editor->findChild<QFrame *>(QStringLiteral("colourPreview"));
        auto *warning = editor->findChild<KMessageWidget *>(QStringLiteral("contrastWarning"));
        auto *overrideText = editor->findChild<QCheckBox *>(QStringLiteral("overrideText"));
        auto *textButton = editor->findChild<KColorButton *>(QStringLiteral("colourText"));
        auto *windowButton = editor->findChild<KColorButton *>(QStringLiteral("colourWindow"));
        QVERIFY(preview && warning && overrideText && textButton && windowButton);
        QVERIFY(editor->colors().isEmpty());

        // Roles that are not overridden show the selected scheme's colours.
        combo->setCurrentIndex(combo->findData(QLatin1String(ColorSchemes::DarkPreset)));
        QCOMPARE(windowButton->color(), QColor(32, 35, 38));
        QCOMPARE(preview->palette().color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));
        QVERIFY(!textButton->isEnabled());

        overrideText->setChecked(true);
        QVERIFY(textButton->isEnabled());
        textButton->setColor(QColor(40, 44, 48));
        QCOMPARE(preview->palette().color(QPalette::Active, QPalette::Text), QColor(40, 44, 48));
        QVERIFY(!warning->isHidden());
        QVERIFY(warning->text().contains(QLatin1String("View text on view background")));
        textButton->setColor(QColor(250, 250, 250));
        QVERIFY(warning->isHidden());
        QCOMPARE(editor->colors(), QStringList({QStringLiteral("Text=#fafafa")}));
        // Previewing does not change the application.
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));

        // Revert discards edits that were not applied.
        auto *revert = editor->findChild<QPushButton *>(QStringLiteral("revertColours"));
        QVERIFY(revert && revert->isEnabled());
        revert->click();
        QVERIFY(editor->colors().isEmpty());
        QVERIFY(!revert->isEnabled());
        QVERIFY(!textButton->isEnabled());

        overrideText->setChecked(true);
        textButton->setColor(QColor(250, 250, 250));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::customColors(), QStringList({QStringLiteral("Text=#fafafa")}));
        QTRY_COMPARE(window.palette().color(QPalette::Active, QPalette::Text), QColor(250, 250, 250));
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));
        QVERIFY(!revert->isEnabled());

        editor->findChild<QPushButton *>(QStringLiteral("useSchemeColours"))->click();
        QVERIFY(editor->colors().isEmpty());
        QVERIFY(revert->isEnabled());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
    }

    void accentColourAppliesAndWarnsAboutContrast()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            ColorSchemes::apply(QString());
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *combo = dialog.findChild<ColorSchemeCombo *>(QStringLiteral("kcfg_ColorScheme"));
        auto *chooser = dialog.findChild<AccentColorChooser *>(QStringLiteral("kcfg_AccentColor"));
        auto *editor = dialog.findChild<ColorPaletteEditor *>(QStringLiteral("kcfg_CustomColors"));
        QVERIFY(combo && chooser && editor);
        auto *custom = chooser->findChild<QCheckBox *>(QStringLiteral("customAccent"));
        auto *button = chooser->findChild<KColorButton *>(QStringLiteral("accentColour"));
        auto *warning = chooser->findChild<KMessageWidget *>(QStringLiteral("accentContrastWarning"));
        auto *highlightButton = editor->findChild<KColorButton *>(QStringLiteral("colourHighlight"));
        QVERIFY(custom && button && warning && highlightButton);
        QVERIFY(chooser->accent().isEmpty());
        QVERIFY(!button->isEnabled());

        // Without an app-local accent, the button shows the scheme's accent.
        combo->setCurrentIndex(combo->findData(QLatin1String(ColorSchemes::DarkPreset)));
        const QPalette dark = ColorSchemes::palette(QLatin1String(ColorSchemes::DarkPreset));
        QCOMPARE(button->color(), dark.color(QPalette::Active, QPalette::Accent));

        custom->setChecked(true);
        QVERIFY(button->isEnabled());
        // A dark accent on a dark scheme is hard to see against the view background.
        button->setColor(QColor(30, 30, 30));
        QVERIFY(!warning->isHidden());
        button->setColor(QColor(0xe0, 0x60, 0x20));
        QVERIFY(warning->isHidden());
        QCOMPARE(chooser->accent(), QStringLiteral("#e06020"));
        // The custom colour editor shows the accent for roles it does not override.
        QCOMPARE(highlightButton->color(), QColor(0xe0, 0x60, 0x20));

        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::accentColor(), QStringLiteral("#e06020"));
        QTRY_COMPARE(window.palette().color(QPalette::Active, QPalette::Highlight), QColor(0xe0, 0x60, 0x20));
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Accent), QColor(0xe0, 0x60, 0x20));
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));

        custom->setChecked(false);
        QVERIFY(chooser->accent().isEmpty());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::accentColor().isEmpty());
        QTRY_COMPARE(window.palette().color(QPalette::Active, QPalette::Highlight), dark.color(QPalette::Active, QPalette::Highlight));
    }

    void backgroundBlurReportsWhenUnavailable()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        QVERIFY(WindowEffects::isTranslucent(&window));
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *blur = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_BackgroundBlur"));
        auto *notice = dialog.findChild<KMessageWidget *>(QStringLiteral("windowEffectsNotice"));
        QVERIFY(blur && notice);
        QVERIFY(notice->isHidden());

        // The offscreen platform has no KWin blur effect.
        QVERIFY(!WindowEffects::isBlurAvailable(&window));
        blur->setChecked(true);
        QVERIFY(!notice->isHidden());
        QVERIFY(notice->text().contains(QLatin1String("blur effect is not available")));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::backgroundBlur());
        QCOMPARE(window.statusBar()->currentMessage(), notice->text());

        blur->setChecked(false);
        QVERIFY(notice->isHidden());

        QWidget opaque;
        QVERIFY(WindowEffects::unavailableReason(&opaque, false, false).isEmpty());
        QVERIFY(WindowEffects::unavailableReason(&opaque, false, true).contains(QLatin1String("compositing")));
    }

    void backgroundOpacityKeepsContentOpaque()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        window.openUrls({m_a, m_b});
        QVERIFY(WindowEffects::isTranslucent(&window));
        Pane *pane = window.findChildren<Pane *>().constFirst();
        auto *places = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
        QVERIFY(places);
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Window).alpha(), 255);
        QCOMPARE(pane->view()->palette().color(QPalette::Active, QPalette::Base).alpha(), 255);

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_WindowOpacity"))->setValue(60);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_PaneOpacity"))->setValue(40);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_PanelOpacity"))->setValue(80);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();

        const QPalette app = QApplication::palette();
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Window).alpha(), 153);
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Window).rgb(), app.color(QPalette::Active, QPalette::Window).rgb());
        QCOMPARE(pane->view()->palette().color(QPalette::Active, QPalette::Base).alpha(), 102);
        QCOMPARE(pane->view()->palette().color(QPalette::Active, QPalette::AlternateBase).alpha(), 102);
        QCOMPARE(places->palette().color(QPalette::Active, QPalette::Base).alpha(), 204);
        QCOMPARE(places->palette().color(QPalette::Active, QPalette::Window).alpha(), 204);
        // Text, icons and selections stay opaque.
        for (QWidget *widget : {static_cast<QWidget *>(&window), static_cast<QWidget *>(pane->view()), static_cast<QWidget *>(places)}) {
            for (const QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::Highlight, QPalette::HighlightedText}) {
                QCOMPARE(widget->palette().color(QPalette::Active, role).alpha(), 255);
            }
        }
        // Floating panels are separate opaque windows.
        places->setFloating(true);
        QCOMPARE(places->palette().color(QPalette::Active, QPalette::Base).alpha(), 255);
        places->setFloating(false);
        QCOMPARE(places->palette().color(QPalette::Active, QPalette::Base).alpha(), 204);

        // The pane tint is made translucent too.
        Settings::setPaneTint(Settings::EnumPaneTint::AllPanes);
        window.applySettings();
        QCOMPARE(pane->view()->palette().color(QPalette::Active, QPalette::Base).alpha(), 102);
        QVERIFY(pane->view()->palette().color(QPalette::Active, QPalette::Base).rgb() != app.color(QPalette::Active, QPalette::Base).rgb());

        // Opaque windows keep opaque colours.
        QWidget opaque;
        Pane standalone(placesOf(window), nullptr, &opaque);
        standalone.setBackgroundOpacity(40);
        QCOMPARE(standalone.view()->palette().color(QPalette::Active, QPalette::Base).alpha(), 255);
    }

    void cornerRadiusRoundsHighlightsTabsAndFields()
    {
        const QString originalName = QApplication::style()->name();
        auto restore = qScopeGuard([originalName] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            if (AppearanceStyle::installed()) {
                QApplication::setStyle(QStyleFactory::create(originalName));
            }
        });
        QCOMPARE(AppearanceStyle::radiusForSetting(Settings::EnumCornerRadius::FollowStyle, 6), int(AppearanceStyle::FollowStyle));
        QCOMPARE(AppearanceStyle::radiusForSetting(Settings::EnumCornerRadius::None, 6), 0);
        QCOMPARE(AppearanceStyle::radiusForSetting(Settings::EnumCornerRadius::Large, 6), 8);
        QCOMPARE(AppearanceStyle::radiusForSetting(Settings::EnumCornerRadius::Custom, 99), 24);

        // Following the widget style leaves the application style alone.
        QStyle *original = QApplication::style();
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.applySettings();
        QCOMPARE(QApplication::style(), original);
        QVERIFY(!AppearanceStyle::installed());
        QCOMPARE(AppearanceStyle::cornerRadius(3), 3);

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *corners = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_CornerRadius"));
        auto *custom = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CustomCornerRadius"));
        QVERIFY(corners && custom);
        QVERIFY(!custom->isEnabled());
        corners->setCurrentIndex(Settings::EnumCornerRadius::Custom);
        Q_EMIT corners->activated(Settings::EnumCornerRadius::Custom);
        QVERIFY(custom->isEnabled());
        custom->setValue(8);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::cornerRadius(), int(Settings::EnumCornerRadius::Custom));
        QCOMPARE(Settings::customCornerRadius(), 8);
        AppearanceStyle *style = AppearanceStyle::installed();
        QVERIFY(style);
        QCOMPARE(style->radius(), 8);
        QCOMPARE(AppearanceStyle::cornerRadius(3), 8);
        QCOMPARE(style->baseStyle(), original);
        Pane *pane = window.findChildren<Pane *>().constFirst();
        QCOMPARE(pane->view()->style(), style);

        const QPalette palette = QApplication::palette();
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
        const auto draw = [style](const auto &paint) {
            QImage image(40, 20, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            paint(&painter, image.rect());
            painter.end();
            return image;
        };
        QStyleOptionViewItem item;
        item.palette = palette;
        item.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected;
        item.viewItemPosition = QStyleOptionViewItem::OnlyOne;
        const auto drawItem = [&] {
            return draw([&](QPainter *painter, const QRect &rect) {
                item.rect = rect;
                style->drawPrimitive(QStyle::PE_PanelItemViewItem, &item, painter, nullptr);
            });
        };
        QImage image = drawItem();
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(image.pixel(39, 19)), 0);
        QCOMPARE(image.pixelColor(20, 10), highlight);
        // A selection spanning columns is rounded only at its outer ends.
        item.viewItemPosition = QStyleOptionViewItem::Beginning;
        image = drawItem();
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        QCOMPARE(image.pixelColor(39, 0), highlight);
        item.viewItemPosition = QStyleOptionViewItem::Middle;
        image = drawItem();
        QCOMPARE(image.pixelColor(0, 0), highlight);
        QCOMPARE(image.pixelColor(39, 19), highlight);

        // The selected tab has a fill and an indicator; other tabs are flat.
        QStyleOptionTab tab;
        tab.palette = palette;
        tab.shape = QTabBar::RoundedNorth;
        tab.state = QStyle::State_Enabled | QStyle::State_Active;
        const auto drawTab = [&] {
            return draw([&](QPainter *painter, const QRect &rect) {
                tab.rect = rect;
                style->drawControl(QStyle::CE_TabBarTabShape, &tab, painter, nullptr);
            });
        };
        image = drawTab();
        QCOMPARE(qAlpha(image.pixel(20, 10)), 0);
        tab.state |= QStyle::State_Selected;
        image = drawTab();
        QCOMPARE(qAlpha(image.pixel(1, 1)), 0);
        QCOMPARE(image.pixelColor(20, 17), highlight);
        QVERIFY(qAlpha(image.pixel(20, 10)) > 0);
        QVERIFY(qAlpha(image.pixel(20, 10)) < 255);

        // Square corners fill the whole highlight.
        Settings::setCornerRadius(Settings::EnumCornerRadius::None);
        window.applySettings();
        QCOMPARE(style->radius(), 0);
        item.viewItemPosition = QStyleOptionViewItem::OnlyOne;
        QCOMPARE(drawItem().pixelColor(0, 0), highlight);

        // A replacement widget style is wrapped again.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QTRY_VERIFY(AppearanceStyle::installed());
        QCOMPARE(AppearanceStyle::installed()->radius(), 0);
        QCOMPARE(AppearanceStyle::installed()->baseStyle()->name(), QStringLiteral("fusion"));

        // Following the style again passes drawing through.
        Settings::setCornerRadius(Settings::EnumCornerRadius::FollowStyle);
        window.applySettings();
        QCOMPARE(AppearanceStyle::installed()->radius(), int(AppearanceStyle::FollowStyle));
        QCOMPARE(AppearanceStyle::cornerRadius(3), 3);
    }

    void dividersThinHideAndRevealOnHover()
    {
        const QString originalName = QApplication::style()->name();
        auto restore = qScopeGuard([originalName] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            if (AppearanceStyle::installed()) {
                QApplication::setStyle(QStyleFactory::create(originalName));
            }
        });
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.resize(1000, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *frame = new QFrame(&window);
        frame->setFrameShape(QFrame::StyledPanel);
        const int styleFrameWidth = frame->frameWidth();
        auto *dialogFrame = new QFrame;
        auto deleteDialogFrame = qScopeGuard([dialogFrame] {
            delete dialogFrame;
        });
        dialogFrame->setFrameShape(QFrame::StyledPanel);

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *dividers = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_Dividers"));
        auto *reveal = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RevealDividersOnHover"));
        QVERIFY(dividers && reveal);
        QVERIFY(!reveal->isEnabled());
        dividers->setCurrentIndex(Settings::EnumDividers::Hidden);
        Q_EMIT dividers->activated(Settings::EnumDividers::Hidden);
        QVERIFY(reveal->isEnabled());
        QVERIFY(reveal->isChecked());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::dividers(), int(Settings::EnumDividers::Hidden));
        AppearanceStyle *style = AppearanceStyle::installed();
        QVERIFY(style);
        QCOMPARE(style->options().dividers, AppearanceStyle::Dividers::Hidden);
        QCOMPARE(style->radius(), int(AppearanceStyle::FollowStyle));

        // Frames in the main window disappear; dialog frames stay.
        QCOMPARE(frame->frameWidth(), 0);
        QCOMPARE(dialogFrame->frameWidth(), styleFrameWidth);

        // Hidden splitter handles keep their resize area and hover state.
        auto *splitter = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(splitter);
        QVERIFY(splitter->handle(1)->testAttribute(Qt::WA_Hover));
        QVERIFY(splitter->handleWidth() > 0);
        QCOMPARE(splitter->handle(1)->cursor().shape(), splitter->orientation() == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);

        const QPalette palette = QApplication::palette();
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
        QStyleOption handle;
        handle.palette = palette;
        handle.rect = QRect(0, 0, 7, 40);
        handle.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Horizontal;
        const auto drawHandle = [&] {
            QImage image(7, 40, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style->drawControl(QStyle::CE_Splitter, &handle, &painter, nullptr);
            painter.end();
            return image;
        };
        QImage image = drawHandle();
        QCOMPARE(qAlpha(image.pixel(3, 20)), 0);
        handle.state |= QStyle::State_MouseOver;
        image = drawHandle();
        QCOMPARE(image.pixelColor(3, 20), highlight);
        QCOMPARE(qAlpha(image.pixel(0, 20)), 0);
        QCOMPARE(style->styleHint(QStyle::SH_Table_GridLineColor, &handle), 0);

        Settings::setRevealDividersOnHover(false);
        window.applySettings();
        QCOMPARE(qAlpha(drawHandle().pixel(3, 20)), 0);

        // Thin dividers draw a single line and a one pixel frame.
        Settings::setDividers(Settings::EnumDividers::Thin);
        window.applySettings();
        QCOMPARE(frame->frameWidth(), 1);
        handle.state &= ~QStyle::State_MouseOver;
        image = drawHandle();
        QCOMPARE(image.pixel(3, 20) >> 24, 255u);
        QVERIFY(image.pixelColor(3, 20) != highlight);
        QCOMPARE(qAlpha(image.pixel(0, 20)), 0);
        QCOMPARE(qAlpha(image.pixel(6, 20)), 0);

        // The divider between the panes moves from the keyboard.
        const QList<int> before = splitter->sizes();
        window.actionCollection()->action(QStringLiteral("move_pane_divider_forward"))->trigger();
        QList<int> after = splitter->sizes();
        QVERIFY2(after.at(0) > before.at(0), qPrintable(QStringLiteral("%1 -> %2").arg(before.at(0)).arg(after.at(0))));
        QCOMPARE(after.at(0) + after.at(1), before.at(0) + before.at(1));
        window.actionCollection()->action(QStringLiteral("move_pane_divider_back"))->trigger();
        after = splitter->sizes();
        QVERIFY(qAbs(after.at(0) - before.at(0)) <= 1);

        Settings::setDividers(Settings::EnumDividers::FollowStyle);
        window.applySettings();
        QCOMPARE(frame->frameWidth(), styleFrameWidth);
    }

    void selectionStylesKeepFocusAndSelectionDistinct()
    {
        const QString originalName = QApplication::style()->name();
        auto restore = qScopeGuard([originalName] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            if (AppearanceStyle::installed()) {
                QApplication::setStyle(QStyleFactory::create(originalName));
            }
        });
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.applySettings();
        QVERIFY(!AppearanceStyle::installed());
        QCOMPARE(AppearanceStyle::selectionStyle(), AppearanceStyle::Selection::FollowStyle);
        QVERIFY(AppearanceStyle::hoverHighlight());
        auto *details = window.activePane()->view()->findChild<DetailsView *>();
        QVERIFY(details);
        QVERIFY(!details->alternatingRowColors());

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *selection = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SelectionStyle"));
        auto *hover = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_HoverHighlight"));
        auto *stripes = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_StripeDetailsRows"));
        QVERIFY(selection && hover && stripes);
        QCOMPARE(selection->count(), 5);
        QVERIFY(hover->isChecked());
        QVERIFY(!stripes->isChecked());
        selection->setCurrentIndex(Settings::EnumSelectionStyle::Outlined);
        Q_EMIT selection->activated(Settings::EnumSelectionStyle::Outlined);
        stripes->setChecked(true);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(int(Settings::selectionStyle()), int(Settings::EnumSelectionStyle::Outlined));
        QVERIFY(Settings::stripeDetailsRows());
        QVERIFY(details->alternatingRowColors());
        AppearanceStyle *style = AppearanceStyle::installed();
        QVERIFY(style);
        QCOMPARE(style->options().selection, AppearanceStyle::Selection::Outlined);
        QCOMPARE(AppearanceStyle::selectionStyle(), AppearanceStyle::Selection::Outlined);

        QPalette palette;
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Highlight, QColor(0x3d, 0xae, 0xe9));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        const QColor highlight = palette.color(QPalette::Highlight);
        QStyleOptionViewItem item;
        item.palette = palette;
        item.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected;
        item.viewItemPosition = QStyleOptionViewItem::OnlyOne;
        const auto drawItem = [&](const QWidget *widget = nullptr) {
            QImage image(40, 20, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            item.rect = image.rect();
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &item, &painter, widget);
            painter.end();
            return image;
        };

        // White selected text is unreadable on a light outline selection, so
        // an unprepared item gets a filled selection.
        QImage image = drawItem();
        QCOMPARE(image.pixelColor(20, 10), highlight);
        // Prepared items keep the normal text colour on a light fill with an outline.
        AppearanceStyle::prepareItem(&item);
        QCOMPARE(item.palette.color(QPalette::Active, QPalette::HighlightedText), QColor(Qt::black));
        image = drawItem();
        QVERIFY(qAlpha(image.pixel(20, 10)) > 0);
        QVERIFY(qAlpha(image.pixel(20, 10)) < 128);
        QCOMPARE(qAlpha(image.pixel(0, 10)), 255);
        QCOMPARE(qAlpha(image.pixel(20, 0)), 255);

        // A selection spanning columns has no outline between its cells.
        item.viewItemPosition = QStyleOptionViewItem::Middle;
        image = drawItem();
        QVERIFY(qAlpha(image.pixel(0, 10)) < 128);
        QCOMPARE(qAlpha(image.pixel(20, 0)), 255);
        item.viewItemPosition = QStyleOptionViewItem::OnlyOne;

        // The current item has a dashed ring with or without a selection.
        const auto ringPixels = [](const QImage &image) {
            int opaque = 0;
            for (int x = 6; x < 34; ++x) {
                opaque += qAlpha(image.pixel(x, 2)) > 128 ? 1 : 0;
            }
            return opaque;
        };
        item.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_HasFocus;
        image = drawItem();
        QCOMPARE(qAlpha(image.pixel(20, 10)), 0);
        const int dashes = ringPixels(image);
        QVERIFY2(dashes > 4 && dashes < 28, qPrintable(QString::number(dashes)));
        item.state |= QStyle::State_Selected;
        QVERIFY(ringPixels(drawItem()) > 4);

        // Item views leave focus to the panels.
        QStyleOptionFocusRect focus;
        focus.palette = palette;
        focus.rect = QRect(0, 0, 40, 20);
        focus.state = QStyle::State_Enabled | QStyle::State_HasFocus;
        QImage focusImage(40, 20, QImage::Format_ARGB32_Premultiplied);
        focusImage.fill(Qt::transparent);
        {
            QPainter painter(&focusImage);
            style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, details);
        }
        QCOMPARE(focusImage, [] {
            QImage empty(40, 20, QImage::Format_ARGB32_Premultiplied);
            empty.fill(Qt::transparent);
            return empty;
        }());

        // An accent bar marks the start of the selection.
        Settings::setSelectionStyle(Settings::EnumSelectionStyle::AccentBar);
        window.applySettings();
        item.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected;
        image = drawItem();
        QCOMPARE(qAlpha(image.pixel(1, 10)), 255);
        QVERIFY(qAlpha(image.pixel(20, 10)) < 128);
        QVERIFY(qAlpha(image.pixel(20, 0)) < 128);
        item.direction = Qt::RightToLeft;
        image = drawItem();
        QCOMPARE(qAlpha(image.pixel(38, 10)), 255);
        QVERIFY(qAlpha(image.pixel(1, 10)) < 128);
        item.direction = Qt::LeftToRight;

        // Filled selections are square; rounded ones are not.
        Settings::setSelectionStyle(Settings::EnumSelectionStyle::Filled);
        window.applySettings();
        image = drawItem();
        QCOMPARE(image.pixelColor(0, 0), highlight);
        QCOMPARE(image.pixelColor(20, 10), highlight);
        Settings::setSelectionStyle(Settings::EnumSelectionStyle::Rounded);
        window.applySettings();
        image = drawItem();
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        QCOMPARE(image.pixelColor(20, 10), highlight);

        // Hover highlighting can be turned off, with or without a selection style.
        item.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_MouseOver;
        QVERIFY(qAlpha(drawItem().pixel(20, 10)) > 0);
        Settings::setHoverHighlight(false);
        window.applySettings();
        QVERIFY(!AppearanceStyle::hoverHighlight());
        QCOMPARE(qAlpha(drawItem().pixel(20, 10)), 0);
        Settings::setSelectionStyle(Settings::EnumSelectionStyle::FollowStyle);
        window.applySettings();
        QVERIFY(AppearanceStyle::installed());
        QVERIFY(!AppearanceStyle::hoverHighlight());

        Settings::setStripeDetailsRows(false);
        window.applySettings();
        QVERIFY(!details->alternatingRowColors());
    }

    void treeLinesDrawSolidDottedOrHidden()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QStandardItemModel model;
        auto *parent = new QStandardItem(QStringLiteral("parent"));
        parent->appendRow(new QStandardItem(QStringLiteral("first")));
        parent->appendRow(new QStandardItem(QStringLiteral("second")));
        model.appendRow(parent);
        model.appendRow(new QStandardItem(QStringLiteral("sibling")));
        QTreeView view;
        view.setModel(&model);
        view.setIndentation(20);
        view.expandAll();
        const QModelIndex first = model.index(0, 0, model.index(0, 0));
        const QModelIndex second = model.index(1, 0, model.index(0, 0));

        const auto draw = [&view](const QModelIndex &index, TreeLines::Style style, bool *handled = nullptr) {
            QImage image(40, 20, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            const bool result = TreeLines::drawBranches(&view, &painter, image.rect(), index, style);
            painter.end();
            if (handled) {
                *handled = result;
            }
            return image;
        };
        bool handled = true;
        draw(first, TreeLines::Style::FollowStyle, &handled);
        QVERIFY(!handled);
        Settings::setTreeLines(Settings::EnumTreeLines::Dotted);
        QCOMPARE(TreeLines::configured(), TreeLines::Style::Dotted);

        // The item's branch is in the inner cell; its parent's sibling line
        // continues in the outer cell.
        QImage image = draw(first, TreeLines::Style::Solid, &handled);
        QVERIFY(handled);
        QVERIFY(qAlpha(image.pixel(30, 2)) > 0);
        QVERIFY(qAlpha(image.pixel(30, 18)) > 0);
        QVERIFY(qAlpha(image.pixel(36, 10)) > 0);
        QVERIFY(qAlpha(image.pixel(10, 3)) > 0);
        QCOMPARE(qAlpha(image.pixel(25, 3)), 0);
        // The last child has no line below it.
        image = draw(second, TreeLines::Style::Solid);
        QVERIFY(qAlpha(image.pixel(30, 2)) > 0);
        QCOMPARE(qAlpha(image.pixel(30, 18)), 0);

        image = draw(first, TreeLines::Style::Dotted);
        QVERIFY((qAlpha(image.pixel(30, 2)) > 0) != (qAlpha(image.pixel(30, 3)) > 0));
        QVERIFY((qAlpha(image.pixel(36, 10)) > 0) != (qAlpha(image.pixel(37, 10)) > 0));

        image = draw(first, TreeLines::Style::Hidden, &handled);
        QVERIFY(handled);
        for (int x = 0; x < image.width(); ++x) {
            for (int y = 0; y < image.height(); ++y) {
                QCOMPARE(qAlpha(image.pixel(x, y)), 0);
            }
        }
    }

    void treeLinesUseSingleDepthOrFolderColours()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            TreeLines::clearFolderColorCache();
        });
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("parent")));
        const QString parentPath = root.filePath(QStringLiteral("parent"));

        QStandardItemModel model;
        auto *parent = new QStandardItem(QStringLiteral("parent"));
        parent->setData(QVariant::fromValue(KFileItem(QUrl::fromLocalFile(parentPath))), KDirModel::FileItemRole);
        auto *child = new QStandardItem(QStringLiteral("child"));
        child->appendRow(new QStandardItem(QStringLiteral("grandchild")));
        child->appendRow(new QStandardItem(QStringLiteral("last")));
        parent->appendRow(child);
        parent->appendRow(new QStandardItem(QStringLiteral("second")));
        model.appendRow(parent);
        QTreeView view;
        view.setModel(&model);
        view.setIndentation(20);
        view.expandAll();
        QPalette palette = view.palette();
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, Qt::black);
        view.setPalette(palette);
        const QModelIndex grandchild = model.index(0, 0, model.index(0, 0, model.index(0, 0)));

        // Cells from the left: root level, the parent's children, the item.
        const auto draw = [&view, &grandchild](const TreeLines::Colors &colors) {
            QImage image(60, 20, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            TreeLines::drawBranches(&view, &painter, image.rect(), grandchild, TreeLines::Style::Solid, colors);
            return image;
        };
        const auto isGrey = [](QRgb pixel) {
            return qAlpha(pixel) > 0 && qAbs(qRed(pixel) - qGreen(pixel)) < 8 && qAbs(qGreen(pixel) - qBlue(pixel)) < 8;
        };

        QImage image = draw({});
        QVERIFY(isGrey(image.pixel(50, 2)));
        QVERIFY(isGrey(image.pixel(30, 2)));

        TreeLines::Colors single{TreeLines::Coloring::Single, QColor(0, 120, 0), {}};
        image = draw(single);
        QCOMPARE(QColor(image.pixel(50, 2)), QColor(0, 120, 0));
        QCOMPARE(QColor(image.pixel(30, 2)), QColor(0, 120, 0));

        // Depth colours repeat; a pale colour is darkened against white.
        TreeLines::Colors depth{TreeLines::Coloring::Depth, {}, {QColor(200, 0, 0), QColor(0, 0, 200), QColor(255, 255, 200)}};
        image = draw(depth);
        QCOMPARE(QColor(image.pixel(30, 2)), QColor(0, 0, 200));
        const QColor adjusted(image.pixel(50, 2));
        QVERIFY2(ColorSchemes::contrastRatio(adjusted, Qt::white) >= 3.0, qPrintable(adjusted.name()));
        QVERIFY(adjusted.red() > adjusted.blue());
        depth.palette = {QColor(200, 0, 0), QColor(0, 0, 200)};
        image = draw(depth);
        QCOMPARE(QColor(image.pixel(50, 2)), QColor(200, 0, 0));

        // Lines take the colour of the folder they belong to; folders
        // without a colour keep neutral lines.
        TreeLines::Colors folder{TreeLines::Coloring::Folder, {}, {}};
        image = draw(folder);
        QVERIFY(isGrey(image.pixel(50, 2)));
        QVERIFY(isGrey(image.pixel(30, 2)));
        QVERIFY(FolderMetadata::setFolderColor(parentPath, QStringLiteral("#c00000")));
        image = draw(folder);
        QVERIFY2(isGrey(image.pixel(30, 2)), "The cached colour should be used until it is cleared");
        TreeLines::clearFolderColorCache();
        image = draw(folder);
        QCOMPARE(QColor(image.pixel(30, 2)), QColor(0xc0, 0, 0));
        QVERIFY(isGrey(image.pixel(50, 2)));

        Settings::setTreeLineColors(Settings::EnumTreeLineColors::Depth);
        Settings::setTreeLinePalette({QStringLiteral("#112233"), QStringLiteral("not a colour")});
        const TreeLines::Colors configured = TreeLines::configuredColors();
        QCOMPARE(configured.coloring, TreeLines::Coloring::Depth);
        QCOMPARE(configured.palette, QList<QColor>{QColor(0x11, 0x22, 0x33)});
    }

    void colourListEditorAddsRemovesAndWarnsAboutContrast()
    {
        {
            MainWindow window;
            SettingsDialog dialog(placesOf(window), &window);
            auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TreeLineColors"));
            auto *single = dialog.findChild<KColorButton *>(QStringLiteral("kcfg_TreeLineColor"));
            auto *depths = dialog.findChild<ColorListEditor *>(QStringLiteral("kcfg_TreeLinePalette"));
            QVERIFY(mode && single && depths);
            QVERIFY(!single->isEnabled() && !depths->isEnabled());
            QCOMPARE(depths->colors().size(), 6);
            mode->setCurrentIndex(Settings::EnumTreeLineColors::Single);
            QVERIFY(single->isEnabled() && !depths->isEnabled());
            mode->setCurrentIndex(Settings::EnumTreeLineColors::Depth);
            QVERIFY(!single->isEnabled() && depths->isEnabled());
        }
        ColorListEditor editor;
        editor.setBackground(Qt::white);
        QSignalSpy changed(&editor, &ColorListEditor::colorsChanged);
        editor.setColors({QStringLiteral("#102030"), QStringLiteral("bogus"), QStringLiteral("#405060")});
        QCOMPARE(editor.colors(), (QStringList{QStringLiteral("#102030"), QStringLiteral("#405060")}));
        QCOMPARE(changed.count(), 1);
        auto *add = editor.findChild<QToolButton *>(QStringLiteral("addColour"));
        auto *remove = editor.findChild<QToolButton *>(QStringLiteral("removeColour"));
        auto *notice = editor.findChild<KMessageWidget *>(QStringLiteral("colourListContrastNotice"));
        QVERIFY(add && remove && notice);
        QVERIFY(notice->isHidden());

        add->click();
        QCOMPARE(editor.colors().size(), 3);
        QCOMPARE(editor.colors().constLast(), QStringLiteral("#102030"));
        QCOMPARE(changed.count(), 2);
        remove->click();
        remove->click();
        QCOMPARE(editor.colors(), QStringList{QStringLiteral("#102030")});
        QVERIFY(!remove->isEnabled());

        editor.setColors(QStringList(ColorListEditor::MaximumColors + 2, QStringLiteral("#000000")));
        QCOMPARE(editor.colors().size(), ColorListEditor::MaximumColors);
        QVERIFY(!add->isEnabled());

        editor.setColors({QStringLiteral("#000000"), QStringLiteral("#fafaf0")});
        QVERIFY(!notice->isHidden());
        QVERIFY2(notice->text().contains(QLatin1Char('2')), qPrintable(notice->text()));
        editor.setBackground(Qt::black);
        QVERIFY(!notice->isHidden());
        QVERIFY2(notice->text().contains(QLatin1Char('1')), qPrintable(notice->text()));
        editor.setColors({QStringLiteral("#fafaf0")});
        QVERIFY(notice->isHidden());
    }

    void paneTintFollowsTheChosenPanesAndScheme()
    {
        auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            ColorSchemes::apply(QString());
        });
        MainWindow window;
        window.openUrls({m_a, m_b});
        const QList<Pane *> panes = window.findChildren<Pane *>();
        QCOMPARE(panes.size(), 2);
        Pane *active = panes.at(0)->isActive() ? panes.at(0) : panes.at(1);
        Pane *inactive = active == panes.at(0) ? panes.at(1) : panes.at(0);
        QVERIFY(active->isActive() && !inactive->isActive());
        const auto viewBase = [](Pane *pane) {
            return pane->view()->palette().color(QPalette::Active, QPalette::Base);
        };
        const QColor base = window.palette().color(QPalette::Active, QPalette::Base);
        QCOMPARE(viewBase(active), base);

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *tint = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_PaneTint"));
        auto *color = dialog.findChild<KColorButton *>(QStringLiteral("kcfg_PaneTintColor"));
        auto *strength = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_PaneTintStrength"));
        QVERIFY(tint && color && strength);
        QVERIFY(!color->isEnabled() && !strength->isEnabled());
        tint->setCurrentIndex(Settings::EnumPaneTint::InactivePane);
        QVERIFY(color->isEnabled() && strength->isEnabled());
        color->setColor(QColor(200, 0, 0));
        strength->setValue(20);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::paneTint(), int(Settings::EnumPaneTint::InactivePane));
        QCOMPARE(Settings::paneTintColor(), QColor(200, 0, 0));

        const QColor tinted = ColorSchemes::paneTint(window.palette(), QColor(200, 0, 0), 20).value(QPalette::Base);
        QVERIFY(tinted != base);
        QCOMPARE(viewBase(inactive), tinted);
        QCOMPARE(viewBase(active), base);
        // Text and selection colours are not tinted.
        QCOMPARE(inactive->view()->palette().color(QPalette::Active, QPalette::Text), window.palette().color(QPalette::Active, QPalette::Text));

        // The tint moves with the active pane.
        active->setActive(false);
        inactive->setActive(true);
        QCOMPARE(viewBase(active), tinted);
        QCOMPARE(viewBase(inactive), base);

        // The tint is recomputed over a new colour scheme.
        auto *combo = dialog.findChild<ColorSchemeCombo *>(QStringLiteral("kcfg_ColorScheme"));
        combo->setCurrentIndex(combo->findData(QLatin1String(ColorSchemes::DarkPreset)));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        const QPalette dark = ColorSchemes::palette(QLatin1String(ColorSchemes::DarkPreset));
        QTRY_COMPARE(viewBase(inactive), dark.color(QPalette::Active, QPalette::Base));
        QTRY_COMPARE(viewBase(active), ColorSchemes::paneTint(dark, QColor(200, 0, 0), 20).value(QPalette::Base));

        tint->setCurrentIndex(Settings::EnumPaneTint::None);
        // The dialog tracks user selection in plain combo boxes.
        Q_EMIT tint->activated(tint->currentIndex());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(viewBase(active), dark.color(QPalette::Active, QPalette::Base));
    }

    void pagesGroupFeaturesInTabs()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const QAbstractItemModel *pages = dialog.findChild<KPageWidget *>()->model();
        QStringList titles;
        for (int row = 0; row < pages->rowCount(); ++row) {
            titles.append(pages->index(row, 0).data().toString());
        }
        QCOMPARE(titles,
                 QStringList({QStringLiteral("General"),
                              QStringLiteral("Translation"),
                              QStringLiteral("Backup"),
                              QStringLiteral("Interface"),
                              QStringLiteral("Themes"),
                              QStringLiteral("View"),
                              QStringLiteral("Context Menu"),
                              QStringLiteral("Archives"),
                              QStringLiteral("Files"),
                              QStringLiteral("Workspaces"),
                              QStringLiteral("Gallery"),
                              QStringLiteral("Editor"),
                              QStringLiteral("Tools")}));

        const auto tabOf = [&dialog](const QString &entry) {
            return tabTitleOf(dialog.findChild<QWidget *>(QStringLiteral("kcfg_") + entry));
        };
        QCOMPARE(tabOf(QStringLiteral("SidebarTitlePosition")), QStringLiteral("Settings Dialog"));
        QCOMPARE(tabOf(QStringLiteral("InterfaceLanguage")), QStringLiteral("Language and Layout"));
        QCOMPARE(tabOf(QStringLiteral("CustomDateFormat")), QStringLiteral("Dates"));
        QCOMPARE(tabOf(QStringLiteral("StartupMode")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("FollowSymlinks")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("DimInactivePane")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("InactivePaneOpacity")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("PaneMoveDestination")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("PaneMoveDisplayStyle")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("PaneSwapMode")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("ShowPreviews")), QStringLiteral("Previews"));
        QCOMPARE(tabOf(QStringLiteral("ConfirmClosingMultipleTabs")), QStringLiteral("Confirmations"));
        QCOMPARE(tabOf(QStringLiteral("ExecutableAction")), QStringLiteral("Confirmations"));
        QCOMPARE(tabOf(QStringLiteral("DropAction")), QStringLiteral("Confirmations"));
        QCOMPARE(tabTitleOf(dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmTrash"))), QStringLiteral("Confirmations"));
        QCOMPARE(tabOf(QStringLiteral("ShowPlaces")), QStringLiteral("Panels"));
        QCOMPARE(tabOf(QStringLiteral("ShowFreeSpace")), QStringLiteral("Status and Location Bars"));
        QCOMPARE(tabOf(QStringLiteral("StatusBarIndicatorOrder")), QStringLiteral("Status and Location Bars"));
        QCOMPARE(tabTitleOf(dialog.findChild<QWidget *>(QStringLiteral("configureMainToolbarEntries"))), QStringLiteral("Toolbars"));
        QCOMPARE(tabOf(QStringLiteral("ColorScheme")), QStringLiteral("Colour Scheme"));
        QCOMPARE(tabOf(QStringLiteral("AccentColor")), QStringLiteral("Colour Scheme"));
        QCOMPARE(tabOf(QStringLiteral("PaneTint")), QStringLiteral("Colour Scheme"));
        QCOMPARE(tabOf(QStringLiteral("BackgroundBlur")), QStringLiteral("Effects"));
        QCOMPARE(tabOf(QStringLiteral("PaneOpacity")), QStringLiteral("Effects"));
        QCOMPARE(tabOf(QStringLiteral("CornerRadius")), QStringLiteral("Effects"));
        QCOMPARE(tabOf(QStringLiteral("Dividers")), QStringLiteral("Lines"));
        QCOMPARE(tabOf(QStringLiteral("RevealDividersOnHover")), QStringLiteral("Lines"));
        QCOMPARE(tabOf(QStringLiteral("TreeLines")), QStringLiteral("Lines"));
        QCOMPARE(tabOf(QStringLiteral("TreeLineColors")), QStringLiteral("Lines"));
        QCOMPARE(tabOf(QStringLiteral("TreeLineColor")), QStringLiteral("Lines"));
        QCOMPARE(tabOf(QStringLiteral("TreeLinePalette")), QStringLiteral("Lines"));
        QCOMPARE(tabOf(QStringLiteral("SelectionStyle")), QStringLiteral("Highlights"));
        QCOMPARE(tabOf(QStringLiteral("HoverHighlight")), QStringLiteral("Highlights"));
        QCOMPARE(tabOf(QStringLiteral("StripeDetailsRows")), QStringLiteral("Highlights"));
        QCOMPARE(tabOf(QStringLiteral("CustomColors")), QStringLiteral("Custom Colours"));
        QCOMPARE(tabOf(QStringLiteral("DefaultViewMode")), QStringLiteral("General"));
        QCOMPARE(tabOf(QStringLiteral("ShowFileListMinimap")), QStringLiteral("General"));
        QCOMPARE(tabOf(QStringLiteral("FileListMinimapWidth")), QStringLiteral("General"));
        QCOMPARE(tabOf(QStringLiteral("FolderSizeRefreshInterval")), QStringLiteral("General"));
        QCOMPARE(tabOf(QStringLiteral("DetailsIconSize")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("DefaultSelectionMarkers")), QStringLiteral("Selection"));
        QCOMPARE(tabOf(QStringLiteral("DefaultFilterScope")), QStringLiteral("Filters"));
        QCOMPARE(tabOf(QStringLiteral("CompactIconSize")), QStringLiteral("Compact"));
        QCOMPARE(tabOf(QStringLiteral("CardPreviewSize")), QStringLiteral("Cards"));
        QCOMPARE(tabOf(QStringLiteral("CardColumns")), QStringLiteral("Cards"));
        QCOMPARE(tabOf(QStringLiteral("CardsDensity")), QStringLiteral("Cards"));
        QCOMPARE(tabOf(QStringLiteral("CardInformation")), QStringLiteral("Cards"));
        QCOMPARE(tabOf(QStringLiteral("AutomaticLocalFolderSizes")), QStringLiteral("Folder Formats"));
        QCOMPARE(tabOf(QStringLiteral("ShowFilePins")), QStringLiteral("Metadata"));
        QCOMPARE(tabOf(QStringLiteral("ShowSizeColumn")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("DragFromAnywhere")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("ShowOpenInNewTab")), QStringLiteral("Actions"));
        QCOMPARE(tabOf(QStringLiteral("ShowCopyMoveToMenus")), QStringLiteral("Actions"));
        QCOMPARE(tabOf(QStringLiteral("DetailsDensity")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("IconsDensity")), QStringLiteral("Icons"));
        QCOMPARE(tabOf(QStringLiteral("BarDensity")), QStringLiteral("Status and Location Bars"));
        QCOMPARE(tabOf(QStringLiteral("ToolbarButtonStyle")), QStringLiteral("Toolbars"));
        QCOMPARE(tabOf(QStringLiteral("ToolbarIconSize")), QStringLiteral("Toolbars"));
        QCOMPARE(tabOf(QStringLiteral("LockToolbars")), QStringLiteral("Toolbars"));
        QCOMPARE(tabOf(QStringLiteral("IconsIconSize")), QStringLiteral("Icons"));
        QCOMPARE(tabOf(QStringLiteral("TextPreviewLimitKiB")), QStringLiteral("Previews"));
        QCOMPARE(tabOf(QStringLiteral("FileActivationDestination")), QStringLiteral("Opening"));
        QCOMPARE(tabOf(QStringLiteral("GalleryShowInformation")), QStringLiteral("Information"));
        QCOMPARE(tabOf(QStringLiteral("ImageBackground")), QStringLiteral("Display"));
        QCOMPARE(tabOf(QStringLiteral("GallerySlideshowInterval")), QStringLiteral("Slideshow"));
        QCOMPARE(tabOf(QStringLiteral("GalleryCopyFormat")), QStringLiteral("Image Conversion"));
        QCOMPARE(tabOf(QStringLiteral("EditorOpening")), QStringLiteral("Editor"));
        QCOMPARE(tabOf(QStringLiteral("TerminalFollowActiveFolder")), QStringLiteral("Embedded Terminal"));
        QCOMPARE(tabOf(QStringLiteral("FolderSyncCopyPolicy")), QStringLiteral("Folder Sync"));
    }

    void editorSettingsApply()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *opening = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_EditorOpening"));
        auto *placement = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_EditorEmbeddedPlacement"));
        QVERIFY(opening && placement);
        QVERIFY(!placement->isEnabled());
        opening->setCurrentIndex(Settings::EnumEditorOpening::Embedded);
        QVERIFY(placement->isEnabled());
        placement->setCurrentIndex(Settings::EnumEditorEmbeddedPlacement::Right);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_EditorFontSize"))->setValue(17);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_EditorWordWrap"))->setChecked(true);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_EditorMarkdownMode"))->setCurrentIndex(Settings::EnumEditorMarkdownMode::Visual);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_EditorDraftsEnabled"))->setChecked(false);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_EditorDraftIntervalMs"))->setValue(1200);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_EditorDraftRetentionDays"))->setValue(30);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_EditorPdfPageSize"))->setCurrentIndex(Settings::EnumEditorPdfPageSize::Letter);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_EditorPdfMarginLeft"))->setValue(20);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::editorOpening(), Settings::EnumEditorOpening::Embedded);
        QCOMPARE(Settings::editorEmbeddedPlacement(), Settings::EnumEditorEmbeddedPlacement::Right);
        QCOMPARE(Settings::editorFontSize(), 17);
        QVERIFY(Settings::editorWordWrap());
        QCOMPARE(Settings::editorMarkdownMode(), Settings::EnumEditorMarkdownMode::Visual);
        QVERIFY(!Settings::editorDraftsEnabled());
        QCOMPARE(Settings::editorDraftIntervalMs(), 1200);
        QCOMPARE(Settings::editorDraftRetentionDays(), 30);
        QCOMPARE(Settings::editorPdfPageSize(), Settings::EnumEditorPdfPageSize::Letter);
        QCOMPARE(Settings::editorPdfMarginLeft(), 20);
    }

    void panelPlacementSettingsApply()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        action(window, QStringLiteral("show_preview_panel"))->setChecked(true);
        action(window, QStringLiteral("options_configure"))->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        auto *preview = dialog->findChild<QComboBox *>(QStringLiteral("kcfg_PreviewPlacement"));
        auto *metadata = dialog->findChild<QComboBox *>(QStringLiteral("kcfg_MetadataPlacement"));
        QVERIFY(preview && metadata);
        preview->setCurrentIndex(Settings::EnumPreviewPlacement::Right);
        metadata->setCurrentIndex(Settings::EnumMetadataPlacement::Below);
        dialog->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::previewPlacement(), Settings::EnumPreviewPlacement::Right);
        QCOMPARE(Settings::metadataPlacement(), Settings::EnumMetadataPlacement::Below);
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("previewDock"));
        QVERIFY(dock && dock->isVisible());
        QCOMPARE(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        dialog->close();
    }

    void embeddedTitleSettingAppliesIndependently()
    {
        Settings::setPanelTitlesVisible(true);
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        action(window, QStringLiteral("options_configure"))->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        auto *embedded = dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_EmbeddedPanelTitlesVisible"));
        auto *navigation = dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_PanelTitlesVisible"));
        QVERIFY(embedded && navigation && embedded->isChecked() && navigation->isChecked());
        embedded->setChecked(false);
        dialog->button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::embeddedPanelTitlesVisible());
        QVERIFY(Settings::panelTitlesVisible());
        QVERIFY(window.findChild<QDockWidget *>(QStringLiteral("previewDock"))->titleBarWidget());
        QVERIFY(!window.findChild<QDockWidget *>(QStringLiteral("combinedDock"))->titleBarWidget());
        dialog->close();
    }

    void opensNativeTextEditorConfiguration()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *configure = dialog.findChild<QPushButton *>(QStringLiteral("configureTextEditor"));
        QVERIFY(configure);
        QCOMPARE(tabTitleOf(configure), QStringLiteral("KTextEditor"));
        bool opened = false;
        QTimer closer;
        closer.setInterval(10);
        connect(&closer, &QTimer::timeout, &dialog, [&] {
            if (auto *native = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                opened = native != &dialog;
                native->reject();
            }
        });
        closer.start();
        configure->click();
        closer.stop();
        QVERIFY(opened);
    }

    void gallerySettingsApplyAndRestoreDefaults()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *information = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GalleryShowInformation"));
        auto *interval = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GallerySlideshowInterval"));
        auto *loop = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GallerySlideshowLoop"));
        auto *textLimit = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_TextPreviewLimitKiB"));
        auto *format = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GalleryCopyFormat"));
        auto *quality = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GalleryCopyQuality"));
        auto *destination = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_FileActivationDestination"));
        auto *formatted = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewFormatted"));
        auto *lineNumbers = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewLineNumbers"));
        auto *highlighting = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewSyntaxHighlighting"));
        auto *zoom = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GalleryInitialZoom"));
        auto *thumbnails = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GalleryShowThumbnails"));
        auto *batchResize = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GalleryBatchResize"));
        auto *batchWidth = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GalleryBatchWidth"));
        auto *batchHeight = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GalleryBatchHeight"));
        auto *batchFormat = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GalleryBatchFormat"));
        auto *batchQuality = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GalleryBatchQuality"));
        auto *batchDestination = dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_GalleryBatchDestination"));
        QVERIFY(information && interval && loop && textLimit && format && quality && destination && formatted && lineNumbers && highlighting && zoom
                && thumbnails && batchResize && batchWidth && batchHeight && batchFormat && batchQuality && batchDestination);
        information->setChecked(true);
        interval->setValue(8);
        loop->setChecked(false);
        textLimit->setValue(12);
        format->setCurrentIndex(Settings::EnumGalleryCopyFormat::Webp);
        quality->setValue(72);
        destination->setCurrentIndex(Settings::EnumFileActivationDestination::Preview);
        formatted->setChecked(false);
        lineNumbers->setChecked(false);
        highlighting->setChecked(false);
        zoom->setCurrentIndex(Settings::EnumGalleryInitialZoom::ActualSize);
        thumbnails->setChecked(false);
        batchResize->setChecked(true);
        batchWidth->setValue(800);
        batchHeight->setValue(600);
        batchFormat->setCurrentIndex(Settings::EnumGalleryBatchFormat::Jpeg);
        batchQuality->setValue(77);
        batchDestination->setText(m_root.path());
        dialog.button(QDialogButtonBox::Apply)->click();
        Settings::self()->load();
        QVERIFY(Settings::galleryShowInformation());
        QCOMPARE(Settings::gallerySlideshowInterval(), 8);
        QVERIFY(!Settings::gallerySlideshowLoop());
        QCOMPARE(Settings::textPreviewLimitKiB(), 12);
        QCOMPARE(Settings::galleryCopyFormat(), Settings::EnumGalleryCopyFormat::Webp);
        QCOMPARE(Settings::galleryCopyQuality(), 72);
        QCOMPARE(Settings::fileActivationDestination(), Settings::EnumFileActivationDestination::Preview);
        QVERIFY(!Settings::previewFormatted());
        QVERIFY(!Settings::previewLineNumbers());
        QVERIFY(!Settings::previewSyntaxHighlighting());
        QCOMPARE(Settings::galleryInitialZoom(), Settings::EnumGalleryInitialZoom::ActualSize);
        QVERIFY(!Settings::galleryShowThumbnails());
        QVERIFY(Settings::galleryBatchResize());
        QCOMPARE(Settings::galleryBatchWidth(), 800);
        QCOMPARE(Settings::galleryBatchHeight(), 600);
        QCOMPARE(Settings::galleryBatchFormat(), Settings::EnumGalleryBatchFormat::Jpeg);
        QCOMPARE(Settings::galleryBatchQuality(), 77);
        QCOMPARE(Settings::galleryBatchDestination(), m_root.path());
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(information->isChecked());
        QCOMPARE(interval->value(), 3);
        QVERIFY(loop->isChecked());
        QCOMPARE(textLimit->value(), 1024);
        QCOMPARE(format->currentIndex(), Settings::EnumGalleryCopyFormat::Png);
        QCOMPARE(quality->value(), 90);
        QCOMPARE(destination->currentIndex(), Settings::EnumFileActivationDestination::Preview);
        QVERIFY(formatted->isChecked());
        QVERIFY(lineNumbers->isChecked());
        QVERIFY(highlighting->isChecked());
        QCOMPARE(zoom->currentIndex(), Settings::EnumGalleryInitialZoom::Fit);
        QVERIFY(thumbnails->isChecked());
        QVERIFY(!batchResize->isChecked());
        QCOMPARE(batchWidth->value(), 1024);
        QCOMPARE(batchHeight->value(), 1024);
        QCOMPARE(batchFormat->currentIndex(), Settings::EnumGalleryBatchFormat::Png);
        QCOMPARE(batchQuality->value(), 90);
        QVERIFY(batchDestination->text().isEmpty());
    }

    void applyingTextLimitRefreshesCurrentPreview()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("text.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray(2500, 'x')), 2500);
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        action(window, QStringLiteral("show_preview_panel"))->setChecked(true);
        auto *text = window.findChild<PreviewPanel *>()->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QVERIFY(text);
        QTRY_COMPARE(text->toPlainText().size(), 2500);
        action(window, QStringLiteral("options_configure"))->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        dialog->findChild<QSpinBox *>(QStringLiteral("kcfg_TextPreviewLimitKiB"))->setValue(1);
        dialog->button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(text->toPlainText().size(), 1024);
        dialog->close();
    }

    void applyingPresentationSettingsUpdatesCurrentPreview()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("page.html")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("<h1>Hello</h1>"), 14);
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        action(window, QStringLiteral("show_preview_panel"))->setChecked(true);
        auto *panel = window.findChild<PreviewPanel *>();
        auto *rendered = panel->findChild<QTextBrowser *>(QStringLiteral("previewRenderedText"));
        auto *source = panel->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *lineNumbers = panel->findChild<QWidget *>(QStringLiteral("previewLineNumbers"));
        QVERIFY(rendered && source && lineNumbers);
        QTRY_VERIFY(rendered->isVisible());
        action(window, QStringLiteral("options_configure"))->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewFormatted"))->setChecked(false);
        dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewLineNumbers"))->setChecked(false);
        dialog->button(QDialogButtonBox::Apply)->click();
        QVERIFY(source->isVisible());
        QVERIFY(lineNumbers->isHidden());
        QCOMPARE(source->toPlainText(), QStringLiteral("<h1>Hello</h1>"));
        dialog->close();
    }

    void sidebarDefaultsToSmallSymbolicIconsBesideTitles()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        QCOMPARE(dialog.findChild<KPageWidget *>()->faceType(), KPageView::FlatList);
        QVERIFY(sidebarOf(dialog));
        QCOMPARE(sidebarOf(dialog)->iconSize(), QSize(16, 16));
        QCOMPARE(firstPageIconName(dialog), QStringLiteral("configure-symbolic"));
    }

    void sidebarStyleChangesOnApply()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"))->setCurrentIndex(Settings::EnumSidebarTitlePosition::Below);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"))->setValue(32);
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(dialog.findChild<KPageWidget *>()->faceType(), KPageView::List);
        QVERIFY(sidebarOf(dialog));
        QCOMPARE(sidebarOf(dialog)->iconSize(), QSize(32, 32));
        QCOMPARE(firstPageIconName(dialog), QStringLiteral("configure"));
    }

    void editorSidebarUsesNotepadIcon()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const QAbstractItemModel *pages = dialog.findChild<KPageWidget *>()->model();
        const auto matches = pages->match(pages->index(0, 0), Qt::DisplayRole, QStringLiteral("Editor"), 1, Qt::MatchExactly);
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.first().data(Qt::DecorationRole).value<QIcon>().name(), QStringLiteral("view-pim-notes-symbolic"));

        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"))->setValue(32);
        QCOMPARE(matches.first().data(Qt::DecorationRole).value<QIcon>().name(), QStringLiteral("view-pim-notes"));
    }

    void m04AppearanceAndArchiveSettingsApply()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FavoritesVisible"))->setChecked(false);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_PanelPosition"))->setCurrentIndex(Settings::EnumPanelPosition::right);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TabIcons"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ExpandTabs"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTabColors"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ArchiveOtherPaneByDefault"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_InlineFolderExpansion"))->setChecked(false);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DetailsColumnWidth"))->setCurrentIndex(Settings::EnumDetailsColumnWidth::Automatic);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DefaultSelectionMarkers"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DefaultSelectionMode"))->setChecked(true);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultArchiveFormat"))->setCurrentIndex(static_cast<int>(ArchiveWriter::Format::TarXz));
        dialog.button(QDialogButtonBox::Apply)->click();

        QVERIFY(!Settings::favoritesVisible());
        QCOMPARE(Settings::panelPosition(), Settings::EnumPanelPosition::right);
        QCOMPARE(Settings::defaultArchiveFormat(), static_cast<int>(ArchiveWriter::Format::TarXz));
        QVERIFY(Settings::archiveOtherPaneByDefault());
        QVERIFY(!Settings::showTabColors());
        auto *tree = qobject_cast<QTreeView *>(window.activePane()->view()->currentView());
        QVERIFY(tree);
        QVERIFY(!tree->itemsExpandable());
        QCOMPARE(tree->header()->sectionResizeMode(KDirModel::Size), QHeaderView::ResizeToContents);
        QVERIFY(window.activePane()->view()->selectionMarkersVisible());
        QVERIFY(window.activePane()->view()->selectionMode());
        QVERIFY(action(window, QStringLiteral("selection_markers"))->isChecked());
        QVERIFY(action(window, QStringLiteral("selection_mode"))->isChecked());
        window.activePane()->view()->setSelectionMode(false);
        window.applySettings();
        QVERIFY(!window.activePane()->view()->selectionMode());
        const auto config = KSharedConfig::openConfig();
        QCOMPARE(config->group(QStringLiteral("PanelSettings")).readEntry("Position", QString()), QStringLiteral("right"));
        QCOMPARE(config->group(QStringLiteral("Archives")).readEntry("DefaultFormat", -1), static_cast<int>(ArchiveWriter::Format::TarXz));
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("combinedDock"));
        QVERIFY(dock);
        QCOMPARE(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        auto *bar = window.activePane()->findChild<QTabBar *>();
        QVERIFY(bar);
        QVERIFY(!bar->expanding());
        QVERIFY(bar->tabIcon(0).isNull());
        CompressDialog archive(QStringLiteral("test"), false, &window, static_cast<ArchiveWriter::Format>(Settings::defaultArchiveFormat()));
        QCOMPARE(archive.format(), ArchiveWriter::Format::TarXz);
        CompressDialog otherArchive(QStringLiteral("test"), true, &window, ArchiveWriter::Format::Zip, Settings::archiveOtherPaneByDefault());
        QVERIFY(otherArchive.toOtherPane());
        CompressDialog unavailable(QStringLiteral("test"), false, &window, ArchiveWriter::Format::Zip, Settings::archiveOtherPaneByDefault());
        QVERIFY(!unavailable.toOtherPane());

        QVERIFY(dialog.button(QDialogButtonBox::RestoreDefaults)->isEnabled());
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FavoritesVisible"))->isChecked());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::favoritesVisible());
        QCOMPARE(Settings::panelPosition(), Settings::EnumPanelPosition::left);
        QCOMPARE(Settings::defaultArchiveFormat(), static_cast<int>(ArchiveWriter::Format::Zip));
        QVERIFY(!Settings::archiveOtherPaneByDefault());
        QVERIFY(Settings::showTabColors());
        QVERIFY(tree->itemsExpandable());
        QVERIFY(!window.activePane()->view()->selectionMarkersVisible());
        QCOMPARE(tree->header()->sectionResizeMode(KDirModel::Name), QHeaderView::Stretch);
        QVERIFY(bar->expanding());
    }

    void compactViewSettingsApply()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CompactIconSize"))->setValue(32);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CompactItemSpacing"))->setValue(5);
        auto *information = dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_CompactInformation"));
        QVERIFY(information);
        information->setInformation({QStringLiteral("size"), QStringLiteral("owner")});
        dialog.button(QDialogButtonBox::Apply)->click();
        FolderView *view = window.activePane()->view();
        view->setViewMode(FolderView::ViewMode::Compact);
        auto *compact = qobject_cast<QListView *>(view->currentView());
        QVERIFY(compact);
        QCOMPARE(compact->iconSize(), QSize(32, 32));
        QCOMPARE(compact->spacing(), 5);
        auto *delegate = qobject_cast<KFileItemDelegate *>(compact->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), (KFileItemDelegate::InformationList{KFileItemDelegate::Size, KFileItemDelegate::Owner}));
    }

    void cardViewSettingsApply()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setCardPreviewSize(96);
            Settings::self()->setCardColumns(0);
            Settings::self()->setCardsDensity(Settings::EnumCardsDensity::Normal);
            Settings::self()->setCardInformation({QStringLiteral("size"),
                                                  QStringLiteral("modificationTime"),
                                                  QStringLiteral("friendlyMimeType"),
                                                  QStringLiteral("tags"),
                                                  QStringLiteral("rating"),
                                                  QStringLiteral("label")});
            Settings::self()->save();
        });
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CardPreviewSize"))->setValue(128);
        auto *columns = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CardColumns"));
        QVERIFY(columns);
        QCOMPARE(columns->specialValueText(), QStringLiteral("Automatic"));
        auto *density = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_CardsDensity"));
        QVERIFY(density);
        density->setCurrentIndex(int(Density::Items::VeryCompact));
        Q_EMIT density->activated(density->currentIndex());
        auto *information = dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_CardInformation"));
        QVERIFY(information);
        QStringList offered;
        for (int row = 0; row < information->count(); ++row) {
            offered.append(information->item(row)->data(Qt::UserRole).toString());
        }
        QVERIFY(offered.contains(QStringLiteral("tags")));
        QVERIFY(offered.contains(QStringLiteral("rating")));
        QVERIFY(offered.contains(QStringLiteral("label")));
        QVERIFY(!dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_IconInformation"))->findItems(QStringLiteral("Tags"), Qt::MatchExactly).size());
        information->setInformation({QStringLiteral("rating"), QStringLiteral("size")});
        Q_EMIT information->informationChanged();
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::cardPreviewSize(), 128);
        FolderView *view = window.activePane()->view();
        view->setViewMode(FolderView::ViewMode::Cards);
        auto *cards = qobject_cast<QListView *>(view->currentView());
        QVERIFY(cards);
        QCOMPARE(cards->iconSize(), QSize(128, 128));
        auto *delegate = qobject_cast<CardDelegate *>(cards->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->previewSize(), 128);
        QCOMPARE(delegate->itemSpace(), 0);
        QCOMPARE(delegate->information(), (QStringList{QStringLiteral("rating"), QStringLiteral("size")}));
    }

    void selectionFiltersCanBeManagedInSettings()
    {
        KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("SelectionFilters"));
        group.writeEntry("Patterns", QStringList{QStringLiteral("*.txt")});
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        auto *editor = dialog.findChild<RuleListEditor *>(QStringLiteral("selectionFilterRules"));
        QVERIFY(editor);
        QCOMPARE(editor->rules(), QStringList{QStringLiteral("*.txt")});
        editor->setRules({QStringLiteral("*.jpg"), QStringLiteral("*.cpp")});
        QCOMPARE(group.readEntry("Patterns", QStringList()), QStringList{QStringLiteral("*.txt")});
        QVERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), (QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.cpp")}));
        QAction *select = action(window, QStringLiteral("select_menu"));
        QMenu *saved = nullptr;
        for (QAction *entry : select->menu()->actions()) {
            if (entry->menu() && entry->text() == QLatin1String("Saved Selection Filters")) {
                saved = entry->menu();
            }
        }
        QVERIFY(saved);
        QMetaObject::invokeMethod(saved, "aboutToShow");
        QCOMPARE(saved->actions().at(0)->text(), QStringLiteral("*.jpg"));
        QCOMPARE(saved->actions().at(1)->text(), QStringLiteral("*.cpp"));
        editor->setRules({QStringLiteral("*.jpg")});
        group.writeEntry("Patterns", QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.cpp"), QStringLiteral("*.png")});
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), (QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.png")}));
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(group.readEntry("Patterns", QStringList()).isEmpty());
        editor->setRules({QStringLiteral("*.jpg"), QStringLiteral("*.png")});
        dialog.button(QDialogButtonBox::Apply)->click();
        group.writeEntry("Patterns", QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.png"), QStringLiteral("*.gif")});
        auto *showLabels = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowFileLabels"));
        QVERIFY(showLabels);
        showLabels->setChecked(!showLabels->isChecked());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), (QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.png"), QStringLiteral("*.gif")}));
        showLabels->setChecked(!showLabels->isChecked());
        dialog.button(QDialogButtonBox::Apply)->click();
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), QStringList{QStringLiteral("*.gif")});
        group.deleteEntry("Patterns");
    }

    void folderSizeDefaultsApplyAndMenuStaysInSync()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_AutomaticLocalFolderSizes"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_AutomaticRemoteFolderSizes"))->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(window.activePane()->view()->automaticFolderSizes());
        QVERIFY(action(window, QStringLiteral("automatic_local_folder_sizes"))->isChecked());
        QVERIFY(!action(window, QStringLiteral("automatic_remote_folder_sizes"))->isChecked());
        action(window, QStringLiteral("automatic_local_folder_sizes"))->trigger();
        QVERIFY(!Settings::automaticLocalFolderSizes());
        QVERIFY(!window.activePane()->view()->automaticFolderSizes());
        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("FolderSizes")).readEntry("AutomaticLocal", false), false);
    }

    void inheritedFormatSourceIsAccessibleFromViewMenu()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("child")));
        FolderMetadata::Format format{QStringLiteral("list"), {}, QStringLiteral("name"), true, QStringLiteral("none"), true};
        QVERIFY(FolderMetadata::setFormat(root.path(), format));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("child")))});
        QAction *openSource = action(window, QStringLiteral("open_folder_format_source"));
        QAction *clear = action(window, QStringLiteral("clear_folder_format"));
        QVERIFY(openSource->isEnabled());
        QVERIFY(!clear->isEnabled());
        openSource->trigger();
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(root.path()));
        QVERIFY(clear->isEnabled());
        action(window, QStringLiteral("lock_folder_format"))->trigger();
        QVERIFY(!clear->isEnabled());
    }

    void hiddenPanelSectionsKeepTheirCollapsedState()
    {
        MainWindow window;
        auto *panel = window.findChild<CombinedPanel *>();
        QVERIFY(panel);
        panel->setCollapsedSections({QStringLiteral("tree")});
        panel->setSectionVisibility(false, false, false);
        QCOMPARE(panel->collapsedSections(), QStringList({QStringLiteral("tree")}));
        panel->setSectionVisibility(true, true, true);
        QCOMPARE(panel->collapsedSections(), QStringList({QStringLiteral("tree")}));
    }

    void listingDefaultsApplyAndPreserveFolderGrouping()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GroupBy"))->setCurrentIndex(Settings::EnumGroupBy::Type);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SizeFormat"))->setCurrentIndex(Settings::EnumSizeFormat::bytes);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DateFormat"))->setCurrentIndex(Settings::EnumDateFormat::iso);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowSeconds"))->setChecked(true);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_IconNameLines"))->setValue(4);
        auto *information = dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_IconInformation"));
        QVERIFY(information);
        information->setInformation({QStringLiteral("size")});
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("View")).readEntry("GroupBy", QString()), QStringLiteral("type"));
        QCOMPARE(Settings::iconInformation(), QStringList({QStringLiteral("size")}));
        FolderView *view = window.activePane()->view();
        QCOMPARE(view->groupBy(), QStringLiteral("type"));
        QTRY_COMPARE(view->itemCount(), 1);
        const QAbstractItemModel *model = view->currentView()->model();
        QCOMPARE(model->index(0, KDirModel::Size).data().toString(), QStringLiteral("5 B"));
        QVERIFY(model->index(0, KDirModel::ModifiedTime)
                    .data()
                    .toString()
                    .contains(QRegularExpression(QStringLiteral("^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}$"))));
        view->setViewMode(FolderView::ViewMode::Icons);
        auto *icons = qobject_cast<QListView *>(view->currentView());
        QVERIFY(icons);
        auto *delegate = qobject_cast<KFileItemDelegate *>(icons->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), KFileItemDelegate::InformationList({KFileItemDelegate::Size}));
        view->setGroupBy(QStringLiteral("name"));
        window.applySettings();
        QCOMPARE(view->groupBy(), QStringLiteral("name"));
    }

    void fileRulesApplyToExistingListing()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *colors = dialog.findChild<RuleListEditor *>(QStringLiteral("kcfg_ExtensionColors"));
        auto *ignored = dialog.findChild<RuleListEditor *>(QStringLiteral("kcfg_IgnoreList"));
        QVERIFY(colors && ignored);
        colors->setRules({QStringLiteral("txt=#123456")});
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(view->currentView()->model()->index(0, 0).data(Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#123456")));
        ignored->setRules({QStringLiteral("*.txt")});
        dialog.button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(view->itemCount(), 0);
    }

    void kioConfirmationsAreSharedWithDolphin()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *trash = dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmTrash"));
        auto *remove = dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmDelete"));
        QVERIFY(trash && remove);
        trash->setChecked(true);
        remove->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();

        const KConfigGroup kio = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("Confirmations"));
        QVERIFY(kio.readEntry("ConfirmTrash", false));
        QVERIFY(!kio.readEntry("ConfirmDelete", true));
    }

    void permanentDeletionHonorsSharedConfirmation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("delete.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        KConfigGroup confirmations = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("Confirmations"));
        confirmations.writeEntry("ConfirmDelete", true);
        confirmations.sync();
        bool prompted = false;
        QTimer prompt;
        connect(&prompt, &QTimer::timeout, &window, [&prompted] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                prompted = true;
                dialog->reject();
            }
        });
        prompt.start(10);
        FileOperations::deletePermanently({QUrl::fromLocalFile(path)}, &window);
        QTRY_VERIFY(prompted);
        QVERIFY(QFile::exists(path));
        prompted = false;
        confirmations.writeEntry("ConfirmDelete", false);
        confirmations.sync();
        FileOperations::deletePermanently({QUrl::fromLocalFile(path)}, &window);
        QTRY_VERIFY(!QFile::exists(path));
        QVERIFY(!prompted);
        prompt.stop();
    }

    void closingWithSeveralTabsAsks()
    {
        Settings::setConfirmClosingMultipleTabs(true);
        MainWindow window;
        window.openUrls({m_a});
        window.activePane()->openInNewTab(m_b);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        // Accepts or rejects the confirmation once it is shown.
        const auto answer = [&window](bool accept) {
            auto *poll = new QTimer(&window);
            QObject::connect(poll, &QTimer::timeout, poll, [poll, accept] {
                const auto boxes =
                    QApplication::activeModalWidget() ? QApplication::activeModalWidget()->findChildren<QDialogButtonBox *>() : QList<QDialogButtonBox *>();
                for (QDialogButtonBox *box : boxes) {
                    const auto buttons = box->buttons();
                    for (QAbstractButton *button : buttons) {
                        const auto role = box->buttonRole(button);
                        const bool accepting = role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole;
                        const bool rejecting = role == QDialogButtonBox::RejectRole || role == QDialogButtonBox::NoRole;
                        if (accept ? accepting : rejecting) {
                            poll->deleteLater();
                            button->click();
                            return;
                        }
                    }
                }
            });
            poll->start(20);
        };
        answer(false);
        QVERIFY(!window.close());
        answer(true);
        QVERIFY(window.close());
    }

    void dropWithoutModifierCopiesOrMoves()
    {
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("drop")));
        QFile copyMe(m_root.filePath(QStringLiteral("copy-me.txt")));
        QVERIFY(copyMe.open(QIODevice::WriteOnly));
        copyMe.close();
        QFile moveMe(m_root.filePath(QStringLiteral("move-me.txt")));
        QVERIFY(moveMe.open(QIODevice::WriteOnly));
        moveMe.close();

        FolderView view;
        view.setUrl(url(QStringLiteral("drop")));
        view.resize(400, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QWidget *viewport = view.currentView()->viewport();

        const auto drop = [viewport](const QUrl &source) {
            QMimeData mime;
            mime.setUrls({source});
            const QPoint position(5, 200);
            QDragEnterEvent enter(position, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &enter);
            QDragMoveEvent move(position, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &move);
            QDropEvent dropEvent(position, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &dropEvent);
        };

        Settings::setDropAction(Settings::EnumDropAction::Copy);
        drop(url(QStringLiteral("copy-me.txt")));
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("drop/copy-me.txt"))));
        QVERIFY(QFile::exists(m_root.filePath(QStringLiteral("copy-me.txt"))));

        Settings::setDropAction(Settings::EnumDropAction::Move);
        drop(url(QStringLiteral("move-me.txt")));
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("drop/move-me.txt"))));
        QTRY_VERIFY(!QFile::exists(m_root.filePath(QStringLiteral("move-me.txt"))));
    }

    void panelDropsHonorTheDefaultAction()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        const QUrl destination = QUrl::fromLocalFile(root.filePath(QStringLiteral("destination")));
        MainWindow window;
        auto *combined = window.findChild<CombinedPanel *>();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("foldersDock"));
        QVERIFY(combined && dock);
        auto *tree = dock->findChild<FolderTree *>();
        QVERIFY(tree);
        const auto drop = [&root, &destination](auto *panel, const QString &name, Qt::KeyboardModifiers modifiers) {
            QFile file(root.filePath(name));
            if (!file.open(QIODevice::WriteOnly)) {
                return false;
            }
            file.close();
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(root.filePath(name))});
            QDropEvent event(QPointF(0, 0), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, modifiers);
            Q_EMIT panel->urlsDropped(destination, &event);
            return true;
        };
        Settings::setDropAction(Settings::EnumDropAction::Copy);
        QVERIFY(drop(tree, QStringLiteral("tree-copy.txt"), Qt::NoModifier));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/tree-copy.txt"))));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("tree-copy.txt"))));
        Settings::setDropAction(Settings::EnumDropAction::Move);
        QVERIFY(drop(combined, QStringLiteral("combined-move.txt"), Qt::NoModifier));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/combined-move.txt"))));
        QTRY_VERIFY(!QFile::exists(root.filePath(QStringLiteral("combined-move.txt"))));
        QVERIFY(drop(combined, QStringLiteral("modifier-copy.txt"), Qt::ControlModifier));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/modifier-copy.txt"))));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("modifier-copy.txt"))));
    }

    void pressingBesideTheNameDrawsASelectionRectangle()
    {
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("rows")));
        for (const char *name : {"r1.txt", "r2.txt", "r3.txt"}) {
            QFile file(m_root.filePath(QStringLiteral("rows/") + QLatin1String(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FolderView view;
        view.setDragFromAnywhere(false);
        view.setUrl(url(QStringLiteral("rows")));
        view.resize(700, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_COMPARE(view.itemCount(), 3);

        auto *details = view.currentView();
        QWidget *viewport = details->viewport();
        const QAbstractItemModel *model = details->model();
        const QPoint from = details->visualRect(model->index(0, KDirModel::ModifiedTime)).center();
        const QPoint to = details->visualRect(model->index(2, KDirModel::ModifiedTime)).center();
        const auto send = [viewport](QEvent::Type type, const QPoint &pos, Qt::MouseButtons buttons) {
            QMouseEvent event(type, pos, viewport->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
            QApplication::sendEvent(viewport, &event);
        };
        send(QEvent::MouseButtonPress, from, Qt::LeftButton);
        send(QEvent::MouseMove, to, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, to, Qt::NoButton);
        QCOMPARE(view.selectedItems().size(), 3);

        send(QEvent::MouseButtonPress, to, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, to, Qt::NoButton);
        QCOMPARE(view.selectedItems().size(), 1);
    }

    void itemDensityChangesRowHeightAndIconSpacing()
    {
        FolderView view;
        view.setUrl(m_a);
        view.resize(600, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_VERIFY(view.itemCount() > 0);
        auto *details = view.currentView();
        const auto rowHeight = [details] {
            details->doItemsLayout();
            return details->visualRect(details->model()->index(0, 0)).height();
        };

        view.setItemSpacing(2, 2);
        const int normal = rowHeight();
        view.setItemSpacing(0, 0);
        QCOMPARE(rowHeight(), normal - 4);
        view.setItemSpacing(10, 10);
        QCOMPARE(rowHeight(), normal + 16);

        view.setViewMode(FolderView::ViewMode::Icons);
        QCOMPARE(qobject_cast<QListView *>(view.currentView())->spacing(), 20);
    }

    void barDensityTightensBars()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto barsHeight = [&window] {
            return window.toolBar()->sizeHint().height() + window.statusBar()->sizeHint().height();
        };
        const int normal = barsHeight();

        Settings::setBarDensity(Settings::EnumBarDensity::VeryCompact);
        window.applySettings();
        QVERIFY(barsHeight() < normal);

        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        window.applySettings();
        QCOMPARE(barsHeight(), normal);
    }

    void customBarSpacingAppliesAndRestores()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *density = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_BarDensity"));
        auto *spacing = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_BarCustomSpacing"));
        QVERIFY(density && spacing);
        QVERIFY(!spacing->isEnabled());
        density->setCurrentIndex(Settings::EnumBarDensity::Custom);
        QVERIFY(spacing->isEnabled());
        spacing->setValue(11);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::barDensity(), Settings::EnumBarDensity::Custom);
        QCOMPARE(Settings::barCustomSpacing(), 11);
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        window.applySettings();
        QLayout *toolbar = window.toolBar()->layout();
        QLayout *navigation = window.activePane()->navigationBar()->layout();
        const QMargins initialToolbar = toolbar->contentsMargins();
        const QMargins initialNavigation = navigation->contentsMargins();
        Settings::setBarDensity(Settings::EnumBarDensity::Custom);
        Settings::setBarCustomSpacing(11);
        window.applySettings();
        QCOMPARE(toolbar->contentsMargins().top(), 11);
        QCOMPARE(navigation->contentsMargins().top(), 11);
        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        window.applySettings();
        QCOMPARE(toolbar->contentsMargins(), initialToolbar);
        QCOMPARE(navigation->contentsMargins(), initialNavigation);
    }

    void customFontsApplyToListsTreesTabsAndStatusBars()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QFont plasma = QApplication::font();
        const QList<FolderTree *> trees = window.findChildren<FolderTree *>();
        QVERIFY(!trees.isEmpty());
        const auto fontsAre = [&](const QFont &list, const QFont &tree, const QFont &tabs, const QFont &status) {
            for (Pane *pane : {window.activePane(), window.inactivePane()}) {
                if (pane->view()->currentView()->font() != list || pane->findChild<QTabBar *>()->font() != tabs || pane->statusBarWidget()->font() != status) {
                    return false;
                }
            }
            for (FolderTree *folderTree : trees) {
                if (folderTree->font() != tree) {
                    return false;
                }
            }
            return window.statusBar()->font() == status;
        };
        QVERIFY(fontsAre(plasma, plasma, plasma, plasma));

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        const QStringList entries{QStringLiteral("FileListFont"), QStringLiteral("FolderTreeFont"), QStringLiteral("TabFont"), QStringLiteral("StatusBarFont")};
        QList<FontOverrideChooser *> choosers;
        for (const QString &entry : entries) {
            choosers.append(dialog.findChild<FontOverrideChooser *>(QStringLiteral("kcfg_") + entry));
            QVERIFY2(choosers.last(), qPrintable(entry));
        }
        QFont tabs(plasma.family(), 14);
        tabs.setItalic(true);
        const QList<QFont> custom{QFont(plasma.family(), 17), QFont(plasma.family(), 15), tabs, QFont(plasma.family(), 7)};
        for (int index = 0; index < choosers.size(); ++index) {
            choosers[index]->setFontSetting(AppFonts::toSetting(custom[index]));
        }
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(AppFonts::fromSetting(Settings::fileListFont()).pointSize(), 17);
        QVERIFY(AppFonts::fromSetting(Settings::tabFont()).italic());
        const auto applied = [&plasma](const QString &setting) {
            return AppFonts::fromSetting(setting).resolve(plasma);
        };
        QVERIFY(
            fontsAre(applied(Settings::fileListFont()), applied(Settings::folderTreeFont()), applied(Settings::tabFont()), applied(Settings::statusBarFont())));

        for (FontOverrideChooser *chooser : std::as_const(choosers)) {
            chooser->setFontSetting(QString());
        }
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::fileListFont().isEmpty());
        QVERIFY(Settings::statusBarFont().isEmpty());
        QVERIFY(fontsAre(plasma, plasma, plasma, plasma));
    }

    void spacingScaleAppliesToBarsTabsAndPanelsButNotDialogs()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tabBar = window.activePane()->findChild<QTabBar *>();
        QVERIFY(tabBar && tabBar->count() > 0);
        QLayout *navigation = window.activePane()->navigationBar()->layout();
        QLayout *paneStatus = window.activePane()->statusBarWidget()->layout();
        const QSize tab = tabBar->tabRect(0).size();
        const QMargins navigationMargins = navigation->contentsMargins();
        const QMargins paneStatusMargins = paneStatus->contentsMargins();
        QDialog dialog;
        const auto spacing = [](const QWidget *widget) {
            return QApplication::style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, widget);
        };
        const int windowSpacing = spacing(window.centralWidget());
        const int dialogSpacing = spacing(&dialog);
        QVERIFY(windowSpacing > 0);

        Settings::setSpacingScale(200);
        window.applySettings();
        QVERIFY(tabBar->tabRect(0).height() > tab.height());
        QCOMPARE(spacing(window.centralWidget()), 2 * windowSpacing);
        QCOMPARE(spacing(&dialog), dialogSpacing);
        QCOMPARE(navigation->contentsMargins(), navigationMargins * 2);
        QCOMPARE(paneStatus->contentsMargins(), paneStatusMargins * 2);

        // Explicit bar densities keep their spacing.
        Settings::setBarDensity(Settings::EnumBarDensity::VeryCompact);
        window.applySettings();
        QCOMPARE(navigation->contentsMargins(), QMargins());

        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        Settings::setSpacingScale(100);
        window.applySettings();
        QCOMPARE(tabBar->tabRect(0).size(), tab);
        QCOMPARE(spacing(window.centralWidget()), windowSpacing);
        QCOMPARE(navigation->contentsMargins(), navigationMargins);
        QCOMPARE(paneStatus->contentsMargins(), paneStatusMargins);
    }

    void tabShapesColoursAndStatusBarStylesApply()
    {
        const QString originalName = QApplication::style()->name();
        MainWindow window;
        auto *colors = window.findChild<TabColors *>();
        QVERIFY(colors);
        auto restore = qScopeGuard([&] {
            colors->setColor(m_a, LabelColor::None);
            Settings::self()->setDefaults();
            Settings::self()->save();
            if (AppearanceStyle::installed()) {
                QApplication::setStyle(QStyleFactory::create(originalName));
            }
        });
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QColor red = LabelColors::rgb(LabelColor::Red);
        colors->setColor(m_a, LabelColor::Red);
        auto *bar = window.activePane()->findChild<QTabBar *>();
        QVERIFY(bar && bar->count() > 0);
        const int current = bar->currentIndex();
        QTRY_COMPARE(bar->tabTextColor(current), red);
        QVERIFY(!bar->tabData(current).isValid());
        const QImage plainStatus = window.statusBar()->grab().toImage();

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        const auto choose = [&dialog](const QString &entry, int index) {
            auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_") + entry);
            QVERIFY2(combo, qPrintable(entry));
            combo->setCurrentIndex(index);
            Q_EMIT combo->activated(index);
        };
        auto *indicator = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TabIndicator"));
        QVERIFY(indicator && !indicator->isEnabled());
        choose(QStringLiteral("TabShape"), Settings::EnumTabShape::Flat);
        QVERIFY(indicator->isEnabled());
        choose(QStringLiteral("TabIndicator"), Settings::EnumTabIndicator::None);
        choose(QStringLiteral("TabColorStrength"), Settings::EnumTabColorStrength::Strong);
        choose(QStringLiteral("StatusBarStyle"), Settings::EnumStatusBarStyle::Shaded);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(int(Settings::tabShape()), int(Settings::EnumTabShape::Flat));
        AppearanceStyle *style = AppearanceStyle::installed();
        QVERIFY(style);

        // Strong tab colours fill the tab, and its text stays readable on the fill.
        const QColor windowColor = bar->palette().color(QPalette::Window);
        const QColor fill = AppearanceStyle::tabColorFill(windowColor, red, AppearanceStyle::TabColorStrength::Strong, true);
        QCOMPARE(bar->tabData(current).value<QColor>(), red);
        QVERIFY(ColorSchemes::contrastRatio(bar->tabTextColor(current), fill) >= 4.5);

        const QPalette palette = QApplication::palette();
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
        QStyleOptionTab tab;
        tab.palette = palette;
        tab.shape = QTabBar::RoundedNorth;
        tab.state = QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected;
        const auto drawTab = [&](int tabIndex) {
            QImage image(40, 20, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            tab.rect = image.rect();
            tab.tabIndex = tabIndex;
            style->drawControl(QStyle::CE_TabBarTabShape, &tab, &painter, bar);
            painter.end();
            return image;
        };
        // Flat tabs have no fill but keep an indicator, even when indicators are off.
        QImage image = drawTab(-1);
        QCOMPARE(qAlpha(image.pixel(20, 8)), 0);
        QCOMPARE(image.pixelColor(20, 18), highlight);
        image = drawTab(current);
        QCOMPARE(image.pixelColor(20, 8).rgb(), fill.rgb());

        // Square tabs fill their corners; a thick indicator is twice as tall.
        Settings::setTabShape(Settings::EnumTabShape::Square);
        Settings::setTabIndicator(Settings::EnumTabIndicator::ThickLine);
        window.applySettings();
        image = drawTab(-1);
        QVERIFY(qAlpha(image.pixel(1, 1)) > 0);
        QCOMPARE(image.pixelColor(20, 16), highlight);
        Settings::setTabIndicator(Settings::EnumTabIndicator::None);
        window.applySettings();
        QVERIFY(drawTab(-1).pixelColor(20, 18) != highlight);

        // Coloured text leaves the tab unfilled.
        Settings::setTabColorStrength(Settings::EnumTabColorStrength::Text);
        window.applySettings();
        QVERIFY(!bar->tabData(current).isValid());
        QCOMPARE(bar->tabTextColor(current), red);

        // Shaded status bars get a background and a divider.
        const QImage shadedStatus = window.statusBar()->grab().toImage();
        QCOMPARE(shadedStatus.size(), plainStatus.size());
        QVERIFY(shadedStatus.pixelColor(shadedStatus.width() / 2, 0) != plainStatus.pixelColor(plainStatus.width() / 2, 0));
        QVERIFY(shadedStatus.pixelColor(1, shadedStatus.height() / 2) != plainStatus.pixelColor(1, plainStatus.height() / 2));
        Settings::setStatusBarStyle(Settings::EnumStatusBarStyle::FollowStyle);
        window.applySettings();
        const QImage restoredStatus = window.statusBar()->grab().toImage();
        QCOMPARE(restoredStatus.pixelColor(restoredStatus.width() / 2, 0), plainStatus.pixelColor(plainStatus.width() / 2, 0));
        QCOMPARE(restoredStatus.pixelColor(1, restoredStatus.height() / 2), plainStatus.pixelColor(1, plainStatus.height() / 2));
    }

    void animationsFollowPlasmaReduceOrTurnOff()
    {
        const QString originalName = QApplication::style()->name();
        auto restore = qScopeGuard([originalName] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            if (AppearanceStyle::installed()) {
                QApplication::setStyle(QStyleFactory::create(originalName));
            }
        });
        MainWindow window;
        const auto duration = [&window] {
            return window.style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, &window);
        };
        const int plasma = duration();
        QVERIFY(plasma > 0);
        QVERIFY(window.dockOptions().testFlag(QMainWindow::AnimatedDocks));

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *animations = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_Animations"));
        QVERIFY(animations);
        animations->setCurrentIndex(Settings::EnumAnimations::Reduced);
        Q_EMIT animations->activated(Settings::EnumAnimations::Reduced);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(duration(), plasma / 2);
        QVERIFY(window.style()->styleHint(QStyle::SH_Widget_Animate, nullptr, &window));
        QVERIFY(!window.dockOptions().testFlag(QMainWindow::AnimatedDocks));

        Settings::setAnimations(Settings::EnumAnimations::Off);
        window.applySettings();
        QCOMPARE(duration(), 0);
        QVERIFY(!window.style()->styleHint(QStyle::SH_Widget_Animate, nullptr, &window));
        QVERIFY(!window.dockOptions().testFlag(QMainWindow::AnimatedDocks));

        Settings::setAnimations(Settings::EnumAnimations::FollowPlasma);
        window.applySettings();
        QCOMPARE(duration(), plasma);
        QVERIFY(window.dockOptions().testFlag(QMainWindow::AnimatedDocks));
    }

    void toolbarAppearanceSettingsApplyToEveryToolbar()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *style = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_ToolbarButtonStyle"));
        auto *iconSize = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_ToolbarIconSize"));
        QVERIFY(style && iconSize);
        QCOMPARE(iconSize->specialValueText(), QStringLiteral("Default"));

        style->setCurrentIndex(Settings::EnumToolbarButtonStyle::TextUnderIcon);
        iconSize->setValue(32);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::toolbarButtonStyle(), Settings::EnumToolbarButtonStyle::TextUnderIcon);
        QCOMPARE(Settings::toolbarIconSize(), 32);
        window.applySettings();

        QCOMPARE(window.toolBar()->toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
        QCOMPARE(window.toolBar()->iconSize(), QSize(32, 32));
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            QCOMPARE(pane->toolBar()->toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
            QCOMPARE(pane->toolBar()->iconSize(), QSize(32, 32));
        }

        QAction *entry = nullptr;
        for (QAction *action : window.toolBar()->actions()) {
            if (qobject_cast<QToolButton *>(window.toolBar()->widgetForAction(action))) {
                entry = action;
                break;
            }
        }
        QVERIFY(entry);
        KConfigGroup entryConfig =
            KSharedConfig::openConfig()->group(QStringLiteral("ToolbarCustomization")).group(QStringLiteral("Entries")).group(entry->objectName());
        entryConfig.writeEntry(QStringLiteral("ButtonStyle"), static_cast<int>(Qt::ToolButtonTextOnly));
        entryConfig.sync();
        window.applySettings();
        QCOMPARE(qobject_cast<QToolButton *>(window.toolBar()->widgetForAction(entry))->toolButtonStyle(), Qt::ToolButtonTextOnly);

        QToolBar *paneToolbar = window.activePane()->toolBar();
        QAction *paneEntry = nullptr;
        for (QAction *action : paneToolbar->actions()) {
            if (qobject_cast<QToolButton *>(paneToolbar->widgetForAction(action))) {
                paneEntry = action;
                break;
            }
        }
        QVERIFY(paneEntry);
        KConfigGroup paneEntryConfig =
            KSharedConfig::openConfig()->group(QStringLiteral("ToolbarCustomization")).group(QStringLiteral("Entries")).group(paneEntry->objectName());
        paneEntryConfig.writeEntry(QStringLiteral("ButtonStyle"), static_cast<int>(Qt::ToolButtonTextBesideIcon));
        paneEntryConfig.sync();
        window.applySettings();
        QCOMPARE(qobject_cast<QToolButton *>(paneToolbar->widgetForAction(paneEntry))->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);

        entryConfig.deleteEntry(QStringLiteral("ButtonStyle"));
        paneEntryConfig.deleteEntry(QStringLiteral("ButtonStyle"));
        Settings::setToolbarIconSize(0);
        Settings::setToolbarButtonStyle(Settings::EnumToolbarButtonStyle::IconOnly);
        window.applySettings();
        QCOMPARE(window.toolBar()->toolButtonStyle(), Qt::ToolButtonIconOnly);
        QCOMPARE(window.activePane()->toolBar()->iconSize(), QSize(16, 16));
    }

    void lockedToolbarsDisableEditingAndCanBeUnlocked()
    {
        Settings::setLockToolbars(false);
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *lock = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_LockToolbars"));
        auto *mainEditor = dialog.findChild<QPushButton *>(QStringLiteral("configureMainToolbarEntries"));
        auto *paneEditor = dialog.findChild<QPushButton *>(QStringLiteral("configureFirstPaneToolbarEntries"));
        QAction *mainCustomize = window.actionCollection()->action(QStringLiteral("customize_main_toolbar"));
        QAction *paneCustomize = window.activePane()->toolbarAction(QStringLiteral("paneCustomizeToolbar"));
        QVERIFY(lock && mainEditor && paneEditor && mainCustomize && paneCustomize);
        QVERIFY(mainCustomize->isEnabled() && paneCustomize->isEnabled());

        mainCustomize->trigger();
        QVERIFY(mainCustomize->isChecked());
        lock->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        window.applySettings();
        QVERIFY(Settings::lockToolbars());
        QVERIFY(!window.toolBar()->isMovable());
        QVERIFY(!mainCustomize->isEnabled() && !paneCustomize->isEnabled());
        QVERIFY(!mainCustomize->isChecked());
        QVERIFY(!mainEditor->isEnabled() && !paneEditor->isEnabled());

        lock->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        window.applySettings();
        QVERIFY(!Settings::lockToolbars());
        QVERIFY(window.toolBar()->isMovable());
        QVERIFY(mainCustomize->isEnabled() && paneCustomize->isEnabled());
        QVERIFY(mainEditor->isEnabled() && paneEditor->isEnabled());
    }

    void inactivePaneOpacitySettingsApply()
    {
        const bool previousEnabled = Settings::dimInactivePane();
        const int previousOpacity = Settings::inactivePaneOpacity();
        const auto restore = qScopeGuard([&] {
            Settings::setDimInactivePane(previousEnabled);
            Settings::setInactivePaneOpacity(previousOpacity);
        });

        Settings::setDimInactivePane(true);
        Settings::setInactivePaneOpacity(38);
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *dim = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DimInactivePane"));
        auto *opacity = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_InactivePaneOpacity"));
        QVERIFY(dim && opacity);
        QVERIFY(dim->isChecked() && opacity->isEnabled());
        QCOMPARE(opacity->value(), 38);
        dim->setChecked(false);
        QVERIFY(!opacity->isEnabled());
        dim->setChecked(true);
        QVERIFY(opacity->isEnabled());
        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(window.inactivePane()->graphicsEffect());
        QVERIFY(effect);
        QCOMPARE(effect->opacity(), 0.38);

        opacity->setValue(72);
        dialog.button(QDialogButtonBox::Apply)->click();
        window.applySettings();
        QCOMPARE(Settings::inactivePaneOpacity(), 72);
        QCOMPARE(effect->opacity(), 0.72);
    }

    void contextListsSaveUnavailableEntries()
    {
        MainWindow window;
        Settings::setHiddenServiceActions({QStringLiteral("not-installed")});
        SettingsDialog dialog(placesOf(window), &window);
        auto *actions = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_HiddenContextActions"));
        auto *services = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_HiddenServiceActions"));
        auto *copy = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_CopyToSubmenu"));
        auto *move = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_MoveToSubmenu"));
        QVERIFY(actions && services && copy && move);
        QListWidgetItem *rename = nullptr;
        QListWidgetItem *service = nullptr;
        for (int row = 0; row < actions->count(); ++row) {
            if (actions->item(row)->data(Qt::UserRole).toString() == QLatin1String("rename")) {
                rename = actions->item(row);
            }
        }
        for (int row = 0; row < services->count(); ++row) {
            if (services->item(row)->data(Qt::UserRole).toString() == QLatin1String("opusfmSettingsTest")) {
                service = services->item(row);
            }
        }
        QVERIFY(rename && service);
        rename->setCheckState(Qt::Unchecked);
        service->setCheckState(Qt::Unchecked);
        copy->setChecked(true);
        move->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::copyToSubmenu());
        QVERIFY(Settings::moveToSubmenu());
        QVERIFY(Settings::hiddenContextActions().contains(QStringLiteral("rename")));
        QVERIFY(Settings::hiddenServiceActions().contains(QStringLiteral("opusfmSettingsTest")));
        QVERIFY(Settings::hiddenServiceActions().contains(QStringLiteral("not-installed")));
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::hiddenContextActions().isEmpty());
        QVERIFY(Settings::hiddenServiceActions().isEmpty());
        QVERIFY(!Settings::copyToSubmenu());
        QVERIFY(!Settings::moveToSubmenu());
    }

    void panelNavigationCanFocusTheActivePane()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *panel = window.findChild<CombinedPanel *>();
        QVERIFY(panel);
        auto *tree = panel->folderTree()->findChild<QTreeView *>();
        QVERIFY(tree);
        tree->setFocus();
        QTRY_VERIFY(tree->hasFocus());
        Settings::setAutoFocusPaneAfterPanelNavigation(false);
        Q_EMIT panel->urlActivated(m_b);
        QCOMPARE(window.activePane()->url(), m_b);
        QVERIFY(tree->hasFocus());
        Settings::setAutoFocusPaneAfterPanelNavigation(true);
        Q_EMIT panel->urlActivated(m_a);
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());

        window.setPanelLayout(MainWindow::PanelLayout::Separate);
        auto *places = window.findChild<KFilePlacesView *>();
        QVERIFY(places);
        Settings::setAutoFocusPaneAfterPanelNavigation(false);
        places->setFocus();
        QVERIFY(QMetaObject::invokeMethod(places, "urlChanged", Qt::DirectConnection, Q_ARG(QUrl, m_b)));
        QCOMPARE(window.activePane()->url(), m_b);
        QVERIFY(places->hasFocus());
        Settings::setAutoFocusPaneAfterPanelNavigation(true);
        QVERIFY(QMetaObject::invokeMethod(places, "urlChanged", Qt::DirectConnection, Q_ARG(QUrl, m_a)));
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());
        auto *foldersDock = window.findChild<QDockWidget *>(QStringLiteral("foldersDock"));
        QVERIFY(foldersDock);
        auto *folderTree = foldersDock->findChild<FolderTree *>();
        QVERIFY(folderTree);
        Q_EMIT folderTree->urlActivated(m_b);
        QCOMPARE(window.activePane()->url(), m_b);
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());
    }

    void contextMenuFollowsSettings()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();

        // Opens the context menu for folder b and returns its entries, then closes it.
        const auto menuTexts = [view, this] {
            QStringList texts;
            auto *poll = new QTimer;
            QObject::connect(poll, &QTimer::timeout, poll, [poll, &texts] {
                auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                if (!menu) {
                    return;
                }
                const auto actions = menu->actions();
                for (const QAction *action : actions) {
                    texts.append(action->text().remove(u'&'));
                }
                poll->deleteLater();
                menu->close();
            });
            poll->start(20);
            Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), {KFileItem(m_b)});
            return texts;
        };

        const QStringList defaults = menuTexts();
        QVERIFY(defaults.contains(QStringLiteral("Open in New Tab")));
        QVERIFY(defaults.contains(QStringLiteral("Copy to Other Pane")));
        QVERIFY(defaults.contains(QStringLiteral("Delete Permanently")));
        QVERIFY(!defaults.contains(QStringLiteral("Copy To")));

        Settings::setShowOpenInNewTab(false);
        Settings::setShowCopyToOtherPane(false);
        Settings::setShowDelete(false);
        Settings::setShowCopyMoveToMenus(true);
        const QStringList changed = menuTexts();
        QVERIFY(!changed.contains(QStringLiteral("Open in New Tab")));
        QVERIFY(!changed.contains(QStringLiteral("Copy to Other Pane")));
        QVERIFY(!changed.contains(QStringLiteral("Delete Permanently")));
        QVERIFY(changed.contains(QStringLiteral("Copy To")));
        QVERIFY(changed.contains(QStringLiteral("Move To")));
    }

    void contextListsAndTransferMenusApply()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("source")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        QFile file(root.filePath(QStringLiteral("source/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QUrl source = QUrl::fromLocalFile(root.filePath(QStringLiteral("source")));
        const QUrl destination = QUrl::fromLocalFile(root.filePath(QStringLiteral("destination")));
        MainWindow window;
        window.openUrls({source, destination});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectAll();
        const KFileItemList items = view->selectedItems();
        QCOMPARE(items.size(), 1);
        QStringList defaultActions;
        QTimer::singleShot(0, [&defaultActions] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            const auto collect = [&defaultActions](auto &&self, QMenu *current) -> void {
                for (QAction *action : current->actions()) {
                    defaultActions.append(action->text().remove(u'&'));
                    if (action->menu()) {
                        self(self, action->menu());
                    }
                }
            };
            collect(collect, menu);
            menu->close();
        });
        Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), items);
        QVERIFY(defaultActions.contains(QStringLiteral("Settings Test Action")));
        Settings::setCopyToSubmenu(true);
        Settings::setMoveToSubmenu(true);
        Settings::setHiddenContextActions({QStringLiteral("open"), QStringLiteral("rename")});
        Settings::setHiddenServiceActions({QStringLiteral("opusfmSettingsTest")});
        QStringList actions;
        QTimer::singleShot(0, [&actions] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            const auto collect = [&actions](auto &&self, QMenu *current) -> void {
                for (QAction *action : current->actions()) {
                    actions.append(action->text().remove(u'&'));
                    if (action->menu()) {
                        self(self, action->menu());
                    }
                }
            };
            collect(collect, menu);
            auto *copy = menu->findChild<QMenu *>(QStringLiteral("copyToMenu"));
            auto *move = menu->findChild<QMenu *>(QStringLiteral("moveToMenu"));
            if (copy && move) {
                actions.append(copy->objectName());
                actions.append(move->objectName());
                copy->actions().first()->trigger();
            }
            menu->close();
        });
        Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), items);
        QVERIFY(actions.contains(QStringLiteral("copyToMenu")));
        QVERIFY(actions.contains(QStringLiteral("moveToMenu")));
        QVERIFY(!actions.contains(QStringLiteral("Open")));
        QVERIFY(!actions.contains(QStringLiteral("Rename")));
        QVERIFY(!actions.contains(QStringLiteral("Settings Test Action")));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/file.txt"))));

        QTimer::singleShot(0, [] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (menu) {
                if (auto *move = menu->findChild<QMenu *>(QStringLiteral("moveToMenu"))) {
                    move->actions().first()->trigger();
                }
                menu->close();
            }
        });
        QFile::remove(root.filePath(QStringLiteral("destination/file.txt")));
        Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), items);
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/file.txt"))));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("source/file.txt"))));
    }

    void serviceMenusAreSharedWithDolphin()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const auto checks = dialog.findChildren<QCheckBox *>();
        QCheckBox *service = nullptr;
        for (QCheckBox *check : checks) {
            if (check->objectName().startsWith(QLatin1String("serviceMenu_"))) {
                service = check;
                break;
            }
        }
        QVERIFY(service);
        QCOMPARE(tabTitleOf(service), QStringLiteral("Service Menus"));
        const QString key = service->objectName().mid(QStringLiteral("serviceMenu_").size());
        service->setChecked(!service->isChecked());
        const bool wanted = service->isChecked();
        dialog.button(QDialogButtonBox::Apply)->click();

        const KConfig config(QStringLiteral("kservicemenurc"), KConfig::NoGlobals);
        QCOMPARE(config.group(QStringLiteral("Show")).readEntry(key, !wanted), wanted);
    }

    void runningAnExecutableRunsIt()
    {
        const QString marker = m_root.filePath(QStringLiteral("ran"));
        const QString script = m_root.filePath(QStringLiteral("touch-marker.sh"));
        QFile file(script);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\ntouch '" + marker.toUtf8() + "'\n");
        file.close();
        QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));

        Settings::setExecutableAction(Settings::EnumExecutableAction::Run);
        MainWindow window;
        FileOperations::open(KFileItem(QUrl::fromLocalFile(script)), &window);
        QTRY_VERIFY(QFile::exists(marker));
    }

    void startupHomeFolderIgnoresSavedTabs()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.close();
        }
        Settings::setStartupMode(Settings::EnumStartupMode::HomeFolder);
        Settings::setHomeUrl(url(QStringLiteral("c")));

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->tabs()->urls(), QList<QUrl>{url(QStringLiteral("c"))});
        QCOMPARE(window.inactivePane()->tabs()->urls(), QList<QUrl>{url(QStringLiteral("c"))});
    }

    void startupFixedFolders()
    {
        Settings::setStartupMode(Settings::EnumStartupMode::FixedFolders);
        Settings::setLeftStartUrl(m_a);
        Settings::setRightStartUrl(m_b);

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
    }

    void singlePaneLayoutIsRememberedAsLastUsed()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            action(window, QStringLiteral("dual_layout"))->trigger();
            QVERIFY(!window.isDualLayout());
            window.close();
        }
        MainWindow restored;
        restored.openUrls({});
        QVERIFY(!restored.isDualLayout());
    }

    void dualPaneAlwaysOverridesLastUsed()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            action(window, QStringLiteral("dual_layout"))->trigger();
            window.close();
        }
        Settings::setDualPaneAtStartup(Settings::EnumDualPaneAtStartup::Always);
        MainWindow restored;
        restored.openUrls({});
        QVERIFY(restored.isDualLayout());
    }

    void alwaysShowTabBar()
    {
        Settings::setAlwaysShowTabBar(false);
        MainWindow window;
        window.openUrls({});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTabBar *tabBar = window.activePane()->findChild<QTabBar *>();
        QVERIFY(!tabBar->isVisible());

        Settings::setAlwaysShowTabBar(true);
        window.applySettings();
        QVERIFY(tabBar->isVisible());
    }

    void hideNavigationBar()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *navigator = window.activePane()->findChild<KUrlNavigator *>();
        QVERIFY(navigator->isVisible());

        Settings::setShowNavigationBar(false);
        window.applySettings();
        QVERIFY(!window.activePane()->isNavigationBarVisible());
        QVERIFY(!navigator->isVisible());
    }

    void newTabsAfterCurrent()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        pane->openInNewTab(m_b, false);
        pane->tabs()->setCurrentIndex(0);

        Settings::setNewTabsAfterCurrent(true);
        window.applySettings();
        pane->openInNewTab(url(QStringLiteral("c")), true);
        QCOMPARE(pane->tabs()->currentIndex(), 1);
        QCOMPARE(pane->tabs()->urls().last(), m_b);
    }

    void followSymlinks()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        const QUrl link = url(QStringLiteral("link"));

        pane->setUrl(link);
        QCOMPARE(pane->url(), link);

        Settings::setFollowSymlinks(true);
        window.applySettings();
        pane->setUrl(m_b);
        pane->setUrl(link);
        QCOMPARE(pane->url(), QUrl::fromLocalFile(QDir(m_a.toLocalFile()).canonicalPath()));
    }

    void tabSwitchesPanes()
    {
        MainWindow window;
        QAction *switchPane = action(window, QStringLiteral("switch_pane"));
        QVERIFY(switchPane->shortcuts().contains(QKeySequence(Qt::Key_Tab)));

        Settings::setTabSwitchesPanes(false);
        window.applySettings();
        QVERIFY(!switchPane->shortcuts().contains(QKeySequence(Qt::Key_Tab)));
    }

    void viewDefaultsApplyToNewWindow()
    {
        Settings::setLinkPanesAtStartup(false);
        Settings::setHomeUrl(m_b);
        Settings::setDefaultViewMode(Settings::EnumDefaultViewMode::Icons);
        Settings::setSortColumn(Settings::EnumSortColumn::Size);
        Settings::setSortDescending(true);
        Settings::setShowHiddenFiles(true);
        Settings::setShowPreviews(false);

        MainWindow window;
        window.openUrls({m_a});
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            FolderView *view = pane->view();
            QCOMPARE(view->viewMode(), FolderView::ViewMode::Icons);
            QCOMPARE(view->sortColumn(), int(KDirModel::Size));
            QCOMPARE(view->sortOrder(), Qt::DescendingOrder);
            QVERIFY(view->showHiddenFiles());
            QVERIFY(!view->previewsEnabled());
        }
        QVERIFY(!action(window, QStringLiteral("show_previews"))->isChecked());
    }

    void liveViewSettings()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(!view->isColumnVisible(KDirModel::Owner));

        Settings::setDetailsIconSize(32);
        Settings::setIconsIconSize(128);
        Settings::setShowOwnerColumn(true);
        Settings::setShowSizeColumn(false);
        Settings::setFoldersFirst(false);
        Settings::setShowToolTips(false);
        window.applySettings();

        QCOMPARE(view->currentView()->iconSize(), QSize(32, 32));
        view->setViewMode(FolderView::ViewMode::Icons);
        QCOMPARE(view->currentView()->iconSize(), QSize(128, 128));
        view->setPreviewsEnabled(false);
        QCOMPARE(view->currentView()->iconSize(), QSize(128, 128));
        view->setPreviewsEnabled(true);
        QVERIFY(view->previewsShown());
        QCOMPARE(view->currentView()->iconSize(), QSize(128, 128));
        QVERIFY(view->isColumnVisible(KDirModel::Owner));
        QVERIFY(!view->isColumnVisible(KDirModel::Size));
        QVERIFY(view->isColumnVisible(KDirModel::Name));
        QVERIFY(!view->sortFoldersFirst());
        QVERIFY(!view->showToolTips());

        window.activePane()->setUrl(m_b);
        QVERIFY(view->isColumnVisible(KDirModel::Owner));
        QVERIFY(!view->isColumnVisible(KDirModel::Size));
    }

    void applyingSettingsPreservesFolderRatingColumn()
    {
        FolderMetadata::Format format;
        format.view = QStringLiteral("list");
        format.columns = {QStringLiteral("rating")};
        format.sortKey = QStringLiteral("name");
        format.group = QStringLiteral("none");
        QVERIFY(FolderMetadata::setFormat(m_a.toLocalFile(), format));

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
        window.applySettings();
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
    }

    void legacyRatingDefaultsSurviveMigrationAndNavigation()
    {
        KConfigGroup legacy = KSharedConfig::openConfig()->group(QStringLiteral("View"));
        legacy.writeEntry("VisibleColumns", QStringList{QStringLiteral("size"), QStringLiteral("rating")});
        legacy.sync();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(Settings::showRatingColumn());
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
        window.activePane()->setUrl(m_b);
        window.applySettings();
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
        QVERIFY(!legacy.hasKey("VisibleColumns"));
    }

    void previewSettings()
    {
        Settings::setShowPreviews(true);
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(view->previewsShown());

        Settings::setPreviewPlugins({QStringLiteral("imagethumbnail")});
        window.applySettings();
        QCOMPARE(view->previewPlugins(), QStringList{QStringLiteral("imagethumbnail")});

        window.activePane()->setUrl(QUrl(QStringLiteral("trash:/")));
        QVERIFY(view->previewsEnabled());
        QVERIFY(!view->previewsShown());
        Settings::setShowRemotePreviews(true);
        window.applySettings();
        QVERIFY(view->previewsShown());
    }

    void previewSizeSpinBoxMapsZeroToNoLimit()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *size = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_MaximumPreviewSize"));
        QVERIFY(size);
        size->setValue(10);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::maximumPreviewSize(), 10ULL * 1024 * 1024);

        size->setValue(0);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::maximumPreviewSize(), std::numeric_limits<qulonglong>::max());
    }

    void previewPluginListWritesCheckedPlugins()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("kcfg_PreviewPlugins"));
        QVERIFY(list);
        if (list->count() < 2) {
            QSKIP("needs at least two KIO thumbnailer plugins installed");
        }
        for (int row = 0; row < list->count(); ++row) {
            list->item(row)->setCheckState(row == 0 ? Qt::Checked : Qt::Unchecked);
        }
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::previewPlugins(), QStringList{list->item(0)->data(Qt::UserRole).toString()});
    }

    void placesPanelVisibility()
    {
        Settings::setShowPlaces(true);
        MainWindow window;
        window.setPanelLayout(MainWindow::PanelLayout::Separate);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
        QVERIFY(dock->isVisible());

        Settings::setShowPlaces(false);
        window.applySettings();
        QVERIFY(!dock->isVisible());

        action(window, QStringLiteral("show_places"))->trigger();
        QVERIFY(dock->isVisible());
        QVERIFY(Settings::showPlaces());

        // Closing the window hides the dock without turning the panel off.
        window.close();
        QVERIFY(Settings::showPlaces());
    }

    void closingPlacesPanelIsRemembered()
    {
        MainWindow window;
        window.setPanelLayout(MainWindow::PanelLayout::Separate);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QDockWidget *>(QStringLiteral("placesDock"))->close();
        QVERIFY(!Settings::showPlaces());
    }

    void placesSectionsAreHiddenThroughTheDialog()
    {
        MainWindow window;
        KFilePlacesModel *places = placesOf(window);
        QVERIFY(places);
        places->setGroupHidden(KFilePlacesModel::RemoteType, false);

        SettingsDialog dialog(placesOf(window), &window);
        auto *remote = dialog.findChild<QCheckBox *>(QStringLiteral("placesGroup_RemoteType"));
        QVERIFY(remote);
        QVERIFY(remote->isChecked());
        remote->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(places->isGroupHidden(KFilePlacesModel::RemoteType));

        places->setGroupHidden(KFilePlacesModel::RemoteType, false);
    }

    void statusBarContents()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *items = window.findChild<QLabel *>(QStringLiteral("itemsLabel"));
        auto *freeSpace = window.findChild<QLabel *>(QStringLiteral("freeSpaceLabel"));
        view->selectAll();
        QVERIFY(items->text().contains(QLatin1Char('(')));

        Settings::setShowSelectionSize(false);
        Settings::setShowFreeSpace(false);
        window.applySettings();
        QVERIFY(!items->text().contains(QLatin1Char('(')));
        QVERIFY(freeSpace->isHidden());

        Settings::setShowItemCounts(false);
        window.applySettings();
        QVERIFY(items->isHidden());
    }

    void statusBarIndicatorsCanBeShownPerPane()
    {
        const auto previousLayout = Settings::statusBarLayout();
        const auto previousOrder = Settings::statusBarIndicatorOrder();
        const bool previousCounts = Settings::showItemCounts();
        const bool previousSize = Settings::showSelectionSize();
        const bool previousFreeSpace = Settings::showFreeSpace();
        const auto restore = qScopeGuard([=] {
            Settings::setStatusBarLayout(previousLayout);
            Settings::setStatusBarIndicatorOrder(previousOrder);
            Settings::setShowItemCounts(previousCounts);
            Settings::setShowSelectionSize(previousSize);
            Settings::setShowFreeSpace(previousFreeSpace);
            Settings::self()->save();
        });
        Settings::setStatusBarLayout(Settings::EnumStatusBarLayout::Shared);
        Settings::setStatusBarIndicatorOrder(Settings::EnumStatusBarIndicatorOrder::ItemsFirst);
        Settings::setShowItemCounts(true);
        Settings::setShowSelectionSize(true);
        Settings::setShowFreeSpace(true);

        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        Pane *left = nullptr;
        Pane *right = nullptr;
        for (Pane *pane : window.findChildren<Pane *>()) {
            if (pane->url() == m_a) {
                left = pane;
            } else if (pane->url() == m_b) {
                right = pane;
            }
        }
        QVERIFY(left);
        QVERIFY(right);
        QTRY_COMPARE(left->view()->itemCount(), 1);
        QTRY_COMPARE(right->view()->itemCount(), 0);

        auto *leftItems = window.findChild<QLabel *>(QStringLiteral("leftItemsLabel"));
        auto *rightItems = window.findChild<QLabel *>(QStringLiteral("rightItemsLabel"));
        auto *sharedItems = window.findChild<QLabel *>(QStringLiteral("itemsLabel"));
        SettingsDialog preferences(placesOf(window), &window);
        auto *layout = preferences.findChild<QComboBox *>(QStringLiteral("kcfg_StatusBarLayout"));
        QVERIFY(leftItems && rightItems && sharedItems && layout);
        QCOMPARE(layout->count(), 2);
        QVERIFY(sharedItems->isVisible());
        QVERIFY(left->statusBarWidget()->isHidden());
        auto *sharedFreeSpace = window.findChild<QLabel *>(QStringLiteral("freeSpaceLabel"));
        QVERIFY(sharedItems->geometry().x() < sharedFreeSpace->geometry().x());
        QVERIFY(sharedItems->parentWidget()->layout()->contentsMargins().left() >= 8);
        QVERIFY(sharedItems->parentWidget()->layout()->contentsMargins().right() >= 8);

        Settings::setStatusBarIndicatorOrder(Settings::EnumStatusBarIndicatorOrder::FreeSpaceFirst);
        window.applySettings();
        QTRY_VERIFY(sharedFreeSpace->geometry().x() < sharedItems->geometry().x());

        Settings::setStatusBarLayout(Settings::EnumStatusBarLayout::PerPane);
        window.applySettings();
        QVERIFY(sharedItems->isHidden());
        QVERIFY(left->statusBarWidget()->isVisible());
        QVERIFY(right->statusBarWidget()->isVisible());
        auto *leftStatusLayout = left->statusBarWidget()->layout();
        QVERIFY(leftStatusLayout->contentsMargins().left() >= 8);
        QVERIFY(leftStatusLayout->contentsMargins().right() >= 8);
        QTRY_VERIFY(leftStatusLayout->indexOf(window.findChild<QLabel *>(QStringLiteral("leftFreeSpaceLabel"))) < leftStatusLayout->indexOf(leftItems));
        auto *rightStatusLayout = right->statusBarWidget()->layout();
        QVERIFY(rightStatusLayout->contentsMargins().left() >= 8);
        QVERIFY(rightStatusLayout->contentsMargins().right() >= 8);
        QTRY_VERIFY(rightStatusLayout->indexOf(window.findChild<QLabel *>(QStringLiteral("rightFreeSpaceLabel"))) < rightStatusLayout->indexOf(rightItems));
        QVERIFY(leftItems->text().contains(QStringLiteral("1 item")));
        QVERIFY(rightItems->text().contains(QStringLiteral("0 items")));

        left->view()->selectAll();
        QTRY_VERIFY(leftItems->text().contains(QStringLiteral("1 of 1 selected")));
        QVERIFY(!rightItems->text().contains(QStringLiteral("selected")));
        right->statusMessage(QStringLiteral("Operation failed"));
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Operation failed"));

        window.statusBar()->clearMessage();
        Settings::setStatusBarLayout(Settings::EnumStatusBarLayout::Shared);
        Settings::setStatusBarIndicatorOrder(Settings::EnumStatusBarIndicatorOrder::ItemsFirst);
        window.applySettings();
        QVERIFY(sharedItems->isVisible());
        QVERIFY(left->statusBarWidget()->isHidden());
    }

    void statusBarMessagesHaveHorizontalPadding()
    {
        MainWindow window;
        window.show();
        auto *bar = window.statusBar();
        bar->setSizeGripEnabled(false);
        for (const Qt::LayoutDirection direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            bar->setLayoutDirection(direction);
            bar->showMessage(QStringLiteral(" "));
            const QImage empty = bar->grab().toImage();
            const QString message(500, QLatin1Char('M'));
            bar->showMessage(message);
            QCOMPARE(bar->currentMessage(), message);
            const QImage text = bar->grab().toImage();
            int changed = 0;
            for (int y = 0; y < text.height(); ++y) {
                for (int x = 0; x < text.width(); ++x) {
                    if (text.pixelColor(x, y) != empty.pixelColor(x, y)) {
                        QVERIFY(x >= 8);
                        QVERIFY(x < text.width() - 8);
                        ++changed;
                    }
                }
            }
            QVERIFY(changed > 0);
        }
        bar->clearMessage();
        QVERIFY(bar->currentMessage().isEmpty());

        auto *permanent = new QLabel(QStringLiteral("Permanent"), bar);
        bar->addPermanentWidget(permanent);
        bar->setFixedWidth(160);
        bar->setSizeGripEnabled(true);
        auto *grip = bar->findChild<QSizeGrip *>();
        QVERIFY(grip);
        QTRY_VERIFY(grip->isVisible());
        for (const Qt::LayoutDirection direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            bar->setLayoutDirection(direction);
            bar->showMessage(QStringLiteral(" "));
            QVERIFY(permanent->isVisible());
            const QImage empty = bar->grab().toImage();
            bar->showMessage(QString(500, QLatin1Char('M')));
            const QImage text = bar->grab().toImage();
            const QRect permanentRect = permanent->geometry().intersected(text.rect());
            QVERIFY(permanentRect.isValid());
            for (int y = permanentRect.top(); y <= permanentRect.bottom(); ++y) {
                for (int x = permanentRect.left(); x <= permanentRect.right(); ++x) {
                    QCOMPARE(text.pixelColor(x, y), empty.pixelColor(x, y));
                }
            }
            const QRect gripRect = grip->geometry().intersected(text.rect());
            QVERIFY(gripRect.isValid());
            for (int y = gripRect.top(); y <= gripRect.bottom(); ++y) {
                for (int x = gripRect.left(); x <= gripRect.right(); ++x) {
                    QCOMPARE(text.pixelColor(x, y), empty.pixelColor(x, y));
                }
            }
        }
        bar->showMessage(QStringLiteral("Temporary"), 20);
        QTRY_VERIFY(bar->currentMessage().isEmpty());
        QVERIFY(permanent->isVisible());
    }

    void embeddedPanelTitlesKeepHorizontalPaddingWithCompactStyles()
    {
        PreviewPanel preview;
        EditorPanel editor(nullptr, m_root.filePath(QStringLiteral("padding-drafts")));
        for (QWidget *panel : {static_cast<QWidget *>(&preview), static_cast<QWidget *>(&editor)}) {
            auto *style = new ZeroHorizontalMarginStyle;
            style->setParent(panel);
            panel->setStyle(style);
            const QMargins margins = panel->layout()->contentsMargins();
            QVERIFY(margins.left() >= 8);
            QVERIFY(margins.right() >= 8);
        }
    }

    void goHomeUsesHomeSetting()
    {
        Settings::setHomeUrl(m_b);
        MainWindow window;
        window.openUrls({m_a});
        action(window, QStringLiteral("go_home"))->trigger();
        QCOMPARE(window.activePane()->url(), m_b);
    }
};

QTEST_MAIN(SettingsTest)

#include "settingstest.moc"
