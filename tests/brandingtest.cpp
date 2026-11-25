// SPDX-License-Identifier: GPL-3.0-or-later

#include "branding.h"

#include <QFile>
#include <QPalette>
#include <QTest>

class BrandingTest : public QObject
{
    Q_OBJECT

private:
    static QPalette palette()
    {
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(10, 20, 30));
        palette.setColor(QPalette::WindowText, QColor(240, 230, 220));
        palette.setColor(QPalette::Accent, QColor(200, 40, 50));
        return palette;
    }

private Q_SLOTS:
    void automaticFollowsPalette()
    {
        const auto colors = Branding::presentationColors(Branding::LogoVariant::Automatic, palette(), false);
        QCOMPARE(colors.background, QColor(10, 20, 30));
        QCOMPARE(colors.text, QColor(240, 230, 220));
        QCOMPARE(colors.accent, QColor(200, 40, 50));
    }

    void fixedVariantsKeepAccent()
    {
        const auto darkText = Branding::presentationColors(Branding::LogoVariant::DarkText, palette(), false);
        QVERIFY(darkText.background.lightnessF() > 0.9);
        QVERIFY(darkText.text.lightnessF() < 0.2);
        QCOMPARE(darkText.accent, QColor(200, 40, 50));

        const auto lightText = Branding::presentationColors(Branding::LogoVariant::LightText, palette(), false);
        QVERIFY(lightText.background.lightnessF() < 0.2);
        QVERIFY(lightText.text.lightnessF() > 0.9);
        QCOMPARE(lightText.accent, QColor(200, 40, 50));
    }

    void highContrastUsesPalette()
    {
        const auto automatic = Branding::presentationColors(Branding::LogoVariant::Automatic, palette(), false);
        for (auto variant : {Branding::LogoVariant::Automatic, Branding::LogoVariant::DarkText, Branding::LogoVariant::LightText}) {
            QCOMPARE(Branding::presentationColors(variant, palette(), true), automatic);
        }
    }

    void recolorsColorSchemeClasses()
    {
        const QByteArray svg = R"(<svg><style id="current-color-scheme">
      .ColorScheme-Accent { color: #3daee9; }
      .ColorScheme-Text{color:#232629}
      .Other { color: #123456; }
    </style></svg>)";
        const QByteArray result = Branding::recolor(svg, QColor(1, 2, 3), QColor(250, 251, 252));
        QVERIFY(result.contains(".ColorScheme-Accent { color: #fafbfc; }"));
        QVERIFY(result.contains(".ColorScheme-Text{color:#010203}"));
        QVERIFY(result.contains(".Other { color: #123456; }"));

        const QByteArray plain = "<svg><path fill=\"#232629\"/></svg>";
        QCOMPARE(Branding::recolor(plain, Qt::red, Qt::green), plain);
    }

    void recolorsLogo()
    {
        QFile file(QStringLiteral(OPUSFM_LOGO_FILE));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray logo = file.readAll();
        QVERIFY(logo.contains("#232629"));
        const QByteArray result = Branding::recolor(logo, QColor(0xfc, 0xfc, 0xfc), QColor(200, 40, 50));
        QVERIFY(!result.contains("#232629"));
        QVERIFY(!result.contains("#3daee9"));
        QVERIFY(result.contains("color: #fcfcfc"));
        QVERIFY(result.contains("color: #c82832"));
    }

    void timing()
    {
        QVERIFY(Branding::holdDuration() > 0);
        QVERIFY(Branding::holdDuration() <= 3000);
        QCOMPARE(Branding::fadeDuration(0), 0);
        QCOMPARE(Branding::fadeDuration(-1), 0);
        QCOMPARE(Branding::fadeDuration(20), 150);
        QCOMPARE(Branding::fadeDuration(200), 400);
        QCOMPARE(Branding::fadeDuration(5000), 500);
    }
};

QTEST_MAIN(BrandingTest)

#include "brandingtest.moc"
