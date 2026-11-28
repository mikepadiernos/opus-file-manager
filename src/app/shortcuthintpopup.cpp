// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcuthintpopup.h"

#include "appearancestyle.h"
#include "yazicolumns.h"

#include <KLocalizedString>

#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace
{
/// The space between the popup and the edges of its area.
constexpr int Margin = 6;
/// Longer descriptions are elided, so that more columns fit.
constexpr int MaximumDescriptionCharacters = 36;
}

ShortcutHintPopup::ShortcutHintPopup(QWidget *parent)
    : QFrame(parent)
    , m_title(new QLabel(this))
    , m_grid(new QWidget(this))
    , m_gridLayout(new QGridLayout(m_grid))
    , m_footer(new QLabel(this))
    , m_slide(new QPropertyAnimation(this, "pos", this))
{
    setObjectName(QStringLiteral("shortcutHintPopup"));
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);
    setAccessibleName(i18nc("@info:accessibility", "Shortcut hints"));
    auto *layout = new QVBoxLayout(this);
    m_title->setObjectName(QStringLiteral("shortcutHintTitle"));
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setVerticalSpacing(0);
    layout->addWidget(m_grid);
    m_footer->setObjectName(QStringLiteral("shortcutHintFooter"));
    m_footer->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_footer);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    parent->installEventFilter(this);
    hide();
}

void ShortcutHintPopup::showHints(Source source, const QString &title, const QList<ShortcutHints::Hint> &hints, const QString &note, QWidget *area)
{
    const bool shown = isShowing(source);
    const bool changed = !shown || title != m_title->text() || hints != m_hints;
    if (m_area != area) {
        if (m_area) {
            m_area->removeEventFilter(this);
        }
        m_area = area;
        if (m_area) {
            m_area->installEventFilter(this);
        }
    }
    if (!shown || hints != m_hints) {
        m_page = 0;
    }
    m_source = source;
    m_note = note;
    m_hints = hints;
    m_title->setText(title);
    layOut();
    place(!shown && !isVisible());
    if (changed) {
        YaziColumns::announce(this, spokenText());
    }
}

void ShortcutHintPopup::dismiss(Source source)
{
    if (m_source != source) {
        return;
    }
    m_slide->stop();
    hide();
}

bool ShortcutHintPopup::isShowing(Source source) const
{
    return isVisible() && m_source == source;
}

QString ShortcutHintPopup::title() const
{
    return m_title->text();
}

QList<ShortcutHints::Hint> ShortcutHintPopup::hints() const
{
    return m_hints;
}

void ShortcutHintPopup::turnPage(int delta)
{
    const int page = qBound(0, m_page + delta, qMax(0, int(m_pages.size()) - 1));
    if (page == m_page) {
        return;
    }
    m_page = page;
    showPage();
    place(false);
    YaziColumns::announce(this, spokenText());
}

int ShortcutHintPopup::page() const
{
    return m_page;
}

int ShortcutHintPopup::pageCount() const
{
    return int(m_pages.size());
}

QList<ShortcutHints::Hint> ShortcutHintPopup::pageHints() const
{
    QList<ShortcutHints::Hint> hints;
    for (const QList<ShortcutHints::Row> &column : m_pages.value(m_page)) {
        for (const ShortcutHints::Row &row : column) {
            if (!row.isHeading()) {
                hints.append(row.hint);
            }
        }
    }
    return hints;
}

int ShortcutHintPopup::pageDelta(QKeyCombination key)
{
    if ((key.keyboardModifiers() & ~Qt::KeypadModifier) != Qt::NoModifier) {
        return 0;
    }
    switch (key.key()) {
    case Qt::Key_PageUp:
    case Qt::Key_Up:
    case Qt::Key_Left:
        return -1;
    case Qt::Key_PageDown:
    case Qt::Key_Down:
    case Qt::Key_Right:
        return 1;
    default:
        return 0;
    }
}

bool ShortcutHintPopup::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if ((watched == parentWidget() || watched == m_area) && isVisible() && (type == QEvent::Resize || type == QEvent::Move)) {
        layOut();
        place(false);
    }
    return QFrame::eventFilter(watched, event);
}

void ShortcutHintPopup::wheelEvent(QWheelEvent *event)
{
    m_wheelDelta += event->angleDelta().y();
    while (qAbs(m_wheelDelta) >= QWheelEvent::DefaultDeltasPerStep) {
        const int step = m_wheelDelta > 0 ? 1 : -1;
        m_wheelDelta -= step * QWheelEvent::DefaultDeltasPerStep;
        turnPage(-step);
    }
    event->accept();
}

void ShortcutHintPopup::layOut()
{
    const QRect area = m_area ? QRect(m_area->mapTo(parentWidget(), QPoint()), m_area->size()) : parentWidget()->rect();
    const QFontMetrics metrics(font());
    QFont bold = font();
    bold.setBold(true);
    const QFontMetrics boldMetrics(bold);
    int keyWidth = 0;
    int descriptionWidth = 0;
    for (const ShortcutHints::Hint &hint : std::as_const(m_hints)) {
        keyWidth = qMax(keyWidth, boldMetrics.horizontalAdvance(hint.keys));
        descriptionWidth = qMax(descriptionWidth, metrics.horizontalAdvance(hint.description));
    }
    descriptionWidth = qMin(descriptionWidth, metrics.averageCharWidth() * MaximumDescriptionCharacters);
    const int spacing = metrics.averageCharWidth() * 2;
    const int gap = metrics.averageCharWidth() * 4;
    m_gridLayout->setHorizontalSpacing(spacing);
    const int columnWidth = keyWidth + spacing + descriptionWidth + gap;
    const QMargins margins = contentsMargins() + layout()->contentsMargins();
    const int available = area.width() - 2 * Margin - margins.left() - margins.right();
    const int columns = qMax(1, (available + gap) / qMax(1, columnWidth));
    const int rowHeight = qMax(metrics.height(), boldMetrics.height());
    const int chrome = m_title->sizeHint().height() + m_footer->sizeHint().height() + 2 * layout()->spacing() + margins.top() + margins.bottom();
    int rows = qMax(2, (area.height() * 2 / 5 - chrome) / qMax(1, rowHeight));
    m_pages = ShortcutHints::pages(m_hints, rows, columns);
    // Balance the columns of a single page.
    while (rows > 2 && m_pages.size() == 1) {
        const QList<ShortcutHints::Page> fewer = ShortcutHints::pages(m_hints, rows - 1, columns);
        if (fewer.size() != 1) {
            break;
        }
        m_pages = fewer;
        --rows;
    }
    m_page = qBound(0, m_page, qMax(0, int(m_pages.size()) - 1));
    m_descriptionWidth = descriptionWidth;
    m_columnGap = gap;
    showPage();
}

void ShortcutHintPopup::showPage()
{
    // A new grid, rather than emptying the old one, as QGridLayout keeps its columns' settings.
    auto *grid = new QWidget(this);
    auto *gridLayout = new QGridLayout(grid);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setVerticalSpacing(0);
    gridLayout->setHorizontalSpacing(m_gridLayout->horizontalSpacing());
    delete layout()->replaceWidget(m_grid, grid);
    delete m_grid;
    m_grid = grid;
    m_gridLayout = gridLayout;

    const QFontMetrics metrics(font());
    const ShortcutHints::Page page = m_pages.value(m_page);
    for (int c = 0; c < page.size(); ++c) {
        const QList<ShortcutHints::Row> &column = page.at(c);
        for (int r = 0; r < column.size(); ++r) {
            const ShortcutHints::Row &row = column.at(r);
            if (row.isHeading()) {
                auto *heading = new QLabel(row.heading, grid);
                QFont font = heading->font();
                font.setBold(true);
                font.setItalic(true);
                heading->setFont(font);
                gridLayout->addWidget(heading, r, c * 3, 1, 2);
                continue;
            }
            auto *keys = new QLabel(row.hint.keys, grid);
            QFont font = keys->font();
            font.setBold(true);
            keys->setFont(font);
            keys->setForegroundRole(QPalette::Link);
            auto *description = new QLabel(metrics.elidedText(row.hint.description, Qt::ElideRight, m_descriptionWidth), grid);
            if (description->text() != row.hint.description) {
                description->setToolTip(row.hint.description);
            }
            gridLayout->addWidget(keys, r, c * 3);
            gridLayout->addWidget(description, r, c * 3 + 1);
        }
        if (c + 1 < page.size()) {
            gridLayout->setColumnMinimumWidth(c * 3 + 2, m_columnGap);
        }
    }
    gridLayout->setColumnStretch(int(page.size()) * 3, 1);
    gridLayout->setRowStretch(gridLayout->rowCount(), 1);

    if (m_pages.size() > 1) {
        m_footer->setText(i18nc("@info shortcut hints footer; %1 is the page, %2 the number of pages, %3 how to turn pages and leave",
                                "%1 of %2 · %3",
                                m_page + 1,
                                m_pages.size(),
                                m_note));
    } else {
        m_footer->setText(m_note);
    }
    m_footer->setVisible(!m_footer->text().isEmpty());
    setAccessibleDescription(spokenText());
}

void ShortcutHintPopup::place(bool slide)
{
    const QRect area = m_area ? QRect(m_area->mapTo(parentWidget(), QPoint()), m_area->size()) : parentWidget()->rect();
    const int width = qMax(1, area.width() - 2 * Margin);
    layout()->activate();
    const int height = qBound(1, sizeHint().height(), qMax(1, area.height() - 2 * Margin));
    const QPoint target(area.left() + Margin, area.top() + area.height() - Margin - height);
    resize(width, height);
    raise();
    const int duration = AppearanceStyle::panelAnimationsEnabled(this) ? style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, this) : 0;
    if (slide && duration > 0) {
        m_slide->stop();
        m_slide->setDuration(duration);
        m_slide->setStartValue(QPoint(target.x(), area.top() + area.height()));
        m_slide->setEndValue(target);
        move(m_slide->startValue().toPoint());
        show();
        m_slide->start();
        return;
    }
    m_slide->stop();
    move(target);
    show();
}

QString ShortcutHintPopup::spokenText() const
{
    QStringList items;
    for (const ShortcutHints::Hint &hint : pageHints()) {
        items.append(i18nc("@info:accessibility a shortcut hint; %1 is the keys, %2 what they do", "%1, %2", hint.keys, hint.description));
    }
    QString text = i18nc("@info:accessibility %1 is the popup's title, %2 its hints", "%1: %2", m_title->text(), items.join(QStringLiteral("; ")));
    if (m_pages.size() > 1) {
        text = i18nc("@info:accessibility %1 is the hints, %2 the page, %3 the number of pages", "%1. Page %2 of %3", text, m_page + 1, m_pages.size());
    }
    return text;
}
