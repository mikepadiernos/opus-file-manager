// SPDX-License-Identifier: GPL-3.0-or-later

#include "doubleshift.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QKeyEvent>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

#include <memory>

class DoubleShiftTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        m_window = std::make_unique<QWindow>();
        m_window->resize(100, 100);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_detector = std::make_unique<DoubleShiftDetector>([this] {
            return m_window.get();
        });
        m_detector->setInterval(2000);
    }

    void cleanup()
    {
        m_detector.reset();
        m_window.reset();
    }

    void triggersOnTwoTaps()
    {
        QSignalSpy triggered(m_detector.get(), &DoubleShiftDetector::triggered);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 1);
        // A third tap starts a new pair.
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 1);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 2);
    }

    void ignoresShiftUsedAsAModifier()
    {
        QSignalSpy triggered(m_detector.get(), &DoubleShiftDetector::triggered);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QTest::keyPress(m_window.get(), Qt::Key_Shift);
        QTest::keyClick(m_window.get(), Qt::Key_A, Qt::ShiftModifier);
        QTest::keyRelease(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);
        // Typing between taps also breaks the pair.
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QTest::keyClick(m_window.get(), Qt::Key_B);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);
    }

    void ignoresHeldShiftAndMousePresses()
    {
        QSignalSpy triggered(m_detector.get(), &DoubleShiftDetector::triggered);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QTest::keyPress(m_window.get(), Qt::Key_Shift);
        QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier, QString(), true);
        QCoreApplication::sendEvent(m_window.get(), &repeat);
        QTest::keyRelease(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);

        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, QPoint(10, 10));
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);
    }

    void ignoresSlowTapsAndOtherWindows()
    {
        QSignalSpy triggered(m_detector.get(), &DoubleShiftDetector::triggered);
        m_detector->setInterval(50);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QTest::qWait(120);
        QTest::keyClick(m_window.get(), Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);

        m_detector->setInterval(2000);
        QWindow other;
        other.resize(100, 100);
        other.show();
        QVERIFY(QTest::qWaitForWindowExposed(&other));
        QTest::keyClick(&other, Qt::Key_Shift);
        QTest::keyClick(&other, Qt::Key_Shift);
        QCOMPARE(triggered.size(), 0);
    }

    void reportsStickyKeys()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto kaccessrc = KSharedConfig::openConfig(dir.filePath(QStringLiteral("kaccessrc")), KConfig::SimpleConfig);
        QVERIFY(DoubleShiftDetector::unavailableReason(kaccessrc).isEmpty());
        kaccessrc->group(QStringLiteral("Keyboard")).writeEntry("StickyKeys", true);
        QVERIFY(!DoubleShiftDetector::unavailableReason(kaccessrc).isEmpty());
    }

private:
    std::unique_ptr<QWindow> m_window;
    std::unique_ptr<DoubleShiftDetector> m_detector;
};

QTEST_MAIN(DoubleShiftTest)

#include "doubleshifttest.moc"
