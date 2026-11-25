// SPDX-License-Identifier: GPL-3.0-or-later

#include "startuppresentation.h"
#include "appearancestyle.h"
#include "appidentity.h"
#include "settings.h"

#include <KAboutData>

#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTest>
#include <QVBoxLayout>

class StartupPresentationTest : public QObject
{
    Q_OBJECT

private:
    struct Window {
        QWidget widget;
        QLineEdit *edit = new QLineEdit(&widget);
        QPushButton *button = new QPushButton(QStringLiteral("Under the card"), &widget);

        Window()
        {
            auto *layout = new QVBoxLayout(&widget);
            layout->addWidget(edit);
            layout->addWidget(button, 1);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            widget.resize(800, 600);
            widget.show();
            edit->setFocus();
        }
    };

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KAboutData::setApplicationData(AppIdentity::aboutData(QStringLiteral("1.2.3")));
    }

    void init()
    {
        Settings::self()->setDefaults();
    }

    void offByDefault()
    {
        QVERIFY(!Settings::startupPresentation());
        QCOMPARE(int(Settings::logoVariant()), int(Branding::LogoVariant::Automatic));
        Window window;
        QVERIFY(!StartupPresentation::presentIfEnabled(&window.widget));
    }

    void presentsWhenEnabled()
    {
        Settings::setStartupPresentation(true);
        Window window;
        QVERIFY(QTest::qWaitForWindowExposed(&window.widget));
        QPointer<StartupPresentation> presentation = StartupPresentation::presentIfEnabled(&window.widget);
        QVERIFY(presentation);
        QTRY_VERIFY(presentation->isVisible());
        QVERIFY(presentation->accessibleName().contains(AppIdentity::displayName()));
        QVERIFY(presentation->accessibleName().contains(QStringLiteral("1.2.3")));
        QCOMPARE(presentation->accessibleDescription(), AppIdentity::summary());
        QVERIFY(qAbs(presentation->geometry().center().x() - window.widget.rect().center().x()) <= 1);
        QVERIFY(qAbs(presentation->geometry().center().y() - window.widget.rect().center().y()) <= 1);
        QVERIFY(window.widget.rect().contains(presentation->geometry()));
        QCOMPARE(presentation->logo().deviceIndependentSize().toSize(), QSize(384, 80));
    }

    void neverBlocksInput()
    {
        Window window;
        QVERIFY(QTest::qWaitForWindowExposed(&window.widget));
        QPointer<StartupPresentation> presentation = new StartupPresentation(&window.widget, Branding::LogoVariant::Automatic, false);
        presentation->start();
        QVERIFY(presentation->isVisible());

        // Pointer input goes to the widget underneath and focus stays put.
        QVERIFY(presentation->testAttribute(Qt::WA_TransparentForMouseEvents));
        QCOMPARE(presentation->focusPolicy(), Qt::NoFocus);
        const QPoint centre = presentation->geometry().center();
        QCOMPARE(window.widget.childAt(centre), window.button);
        QCOMPARE(QApplication::focusWidget(), window.edit);

        // Typing reaches the focused widget and dismisses the presentation.
        QTest::keyClicks(window.edit, QStringLiteral("ab"));
        QCOMPARE(window.edit->text(), QStringLiteral("ab"));
        QTRY_VERIFY(presentation.isNull());
    }

    void clickDismissesAndReachesWindow()
    {
        Window window;
        QVERIFY(QTest::qWaitForWindowExposed(&window.widget));
        QPointer<StartupPresentation> presentation = new StartupPresentation(&window.widget, Branding::LogoVariant::Automatic, false);
        presentation->start();
        bool clicked = false;
        connect(window.button, &QPushButton::clicked, this, [&clicked] {
            clicked = true;
        });
        QTest::mouseClick(window.button, Qt::LeftButton);
        QVERIFY(clicked);
        QTRY_VERIFY(presentation.isNull());
    }

    void hidesItself()
    {
        Window window;
        QPointer<StartupPresentation> presentation = new StartupPresentation(&window.widget, Branding::LogoVariant::Automatic, false);
        presentation->setHoldDuration(10);
        presentation->start();
        QVERIFY(presentation->isVisible());
        QTRY_VERIFY(presentation.isNull());
    }

    void fadesOnlyWithAnimations()
    {
        Window window;
        QPointer<StartupPresentation> presentation = new StartupPresentation(&window.widget, Branding::LogoVariant::Automatic, false);
        presentation->start();
        presentation->dismiss();
        QVERIFY(presentation->isVisible());
        QVERIFY(presentation->graphicsEffect());
        QTRY_VERIFY(presentation.isNull());

        AppearanceStyle::Options options;
        options.animations = AppearanceStyle::Animations::Off;
        AppearanceStyle::apply(options);
        presentation = new StartupPresentation(&window.widget, Branding::LogoVariant::Automatic, false);
        presentation->start();
        presentation->dismiss();
        QVERIFY(!presentation->isVisible());
        QVERIFY(!presentation->graphicsEffect());
        QTRY_VERIFY(presentation.isNull());
        AppearanceStyle::apply(AppearanceStyle::Options());
    }

    void logoFollowsVariant_data()
    {
        QTest::addColumn<int>("variant");
        QTest::addColumn<bool>("lightText");
        QTest::newRow("dark text") << int(Branding::LogoVariant::DarkText) << false;
        QTest::newRow("light text") << int(Branding::LogoVariant::LightText) << true;
    }

    void logoFollowsVariant()
    {
        QFETCH(int, variant);
        QFETCH(bool, lightText);
        Window window;
        StartupPresentation presentation(&window.widget, Branding::LogoVariant(variant), false);
        QCOMPARE(presentation.colors().text.lightnessF() > 0.5, lightText);
        QCOMPARE(presentation.colors().background.lightnessF() > 0.5, !lightText);

        // The wordmark "Plasma" uses the text colour.
        const QImage logo = presentation.logo().toImage();
        QVERIFY(!logo.isNull());
        const qreal scale = logo.width() / 768.0;
        qreal lightness = 0;
        int count = 0;
        for (int y = int(30 * scale); y < int(120 * scale); ++y) {
            for (int x = int(180 * scale); x < int(495 * scale); ++x) {
                const QColor c = logo.pixelColor(x, y);
                if (c.alpha() == 255) {
                    lightness += c.lightnessF();
                    ++count;
                }
            }
        }
        QVERIFY(count > 100);
        QCOMPARE(lightness / count > 0.5, lightText);
    }

    void highContrastUsesPalette()
    {
        Window window;
        QPalette palette = window.widget.palette();
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::yellow);
        window.widget.setPalette(palette);
        StartupPresentation presentation(&window.widget, Branding::LogoVariant::DarkText, true);
        QCOMPARE(presentation.colors().background, QColor(Qt::black));
        QCOMPARE(presentation.colors().text, QColor(Qt::yellow));
    }

    void narrowWindowShrinksLogo()
    {
        Window window;
        window.widget.resize(300, 400);
        StartupPresentation presentation(&window.widget, Branding::LogoVariant::Automatic, false);
        presentation.start();
        QVERIFY(presentation.logo().deviceIndependentSize().width() < 384);
        QVERIFY(presentation.logo().deviceIndependentSize().width() >= 192);
    }
};

QTEST_MAIN(StartupPresentationTest)

#include "startuppresentationtest.moc"
