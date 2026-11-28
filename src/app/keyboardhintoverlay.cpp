// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardhintoverlay.h"

#include "folderview.h"
#include "pane.h"
#include "yazicolumns.h"

#include <KFilePlacesView>
#include <KLocalizedString>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QScrollBar>
#include <QTabBar>
#include <QTreeView>

#include <algorithm>

using KeyboardHints::Kind;

namespace
{

// More than any window shows; it bounds the work for a very tall view.
constexpr int MaximumItems = 2000;

bool isModifierKey(int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_Mode_switch:
        return true;
    default:
        return false;
    }
}

QString kindText(Kind kind)
{
    switch (kind) {
    case Kind::Location:
        return i18nc("@info accessible description of a keyboard hint", "Location segment");
    case Kind::Tab:
        return i18nc("@info accessible description of a keyboard hint", "Tab");
    case Kind::Place:
        return i18nc("@info accessible description of a keyboard hint", "Place");
    case Kind::Item:
        break;
    }
    return i18nc("@info accessible description of a keyboard hint", "File or folder");
}

QColor mixed(const QColor &first, const QColor &second)
{
    return QColor((first.red() + second.red()) / 2, (first.green() + second.green()) / 2, (first.blue() + second.blue()) / 2);
}

// The first row a tree view shows, at any of its columns.
QModelIndex firstVisibleRow(QTreeView *tree)
{
    const QRect visible = tree->viewport()->rect();
    const QHeaderView *header = tree->header();
    QList<int> xs{0, visible.width() / 2, visible.width() - 1};
    for (int section = 0; section < header->count(); ++section) {
        if (!header->isSectionHidden(section)) {
            xs.append(qMax(0, tree->columnViewportPosition(section)));
        }
    }
    for (const int x : std::as_const(xs)) {
        const QModelIndex index = tree->indexAt(QPoint(x, 0));
        if (index.isValid()) {
            return index.siblingAtColumn(0);
        }
    }
    return {};
}

} // namespace

KeyboardHintOverlay::KeyboardHintOverlay(QWidget *window)
    : QWidget(window)
{
    setObjectName(QStringLiteral("keyboardHintOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    // Keys are labels, not text to compose.
    setAttribute(Qt::WA_InputMethodEnabled, false);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(i18nc("@info accessible name", "Keyboard hints"));
    hide();
}

KeyboardHintOverlay::~KeyboardHintOverlay() = default;

QList<KeyboardHintOverlay::Target> KeyboardHintOverlay::itemTargets(QAbstractItemView *view,
                                                                    const std::function<void(const QModelIndex &, bool shift)> &activate)
{
    QList<Target> result;
    QAbstractItemModel *model = view ? view->model() : nullptr;
    if (!model || !view->isVisible()) {
        return result;
    }
    QWidget *viewport = view->viewport();
    const QRect visible = viewport->rect();
    const auto add = [&result, &activate, viewport, visible](const QModelIndex &index, const QRect &rect) {
        const QRect shown = rect & visible;
        if (shown.isEmpty()) {
            return;
        }
        const QPersistentModelIndex persistent(index);
        result.append({Kind::Item, viewport, shown, index.data(Qt::DisplayRole).toString(), [persistent, activate](bool shift) {
                           if (persistent.isValid()) {
                               activate(persistent, shift);
                           }
                       }});
    };

    if (auto *tree = qobject_cast<QTreeView *>(view)) {
        for (QModelIndex row = firstVisibleRow(tree); row.isValid() && result.size() < MaximumItems; row = tree->indexBelow(row)) {
            const QRect name = tree->visualRect(row);
            if (name.top() > visible.bottom()) {
                break;
            }
            // Scrolled sideways past the name, the label marks the row.
            add(row, (name & visible).isEmpty() ? QRect(visible.left(), name.top(), visible.width(), name.height()) : name);
        }
        return result;
    }

    auto *list = qobject_cast<QListView *>(view);
    const QModelIndex root = view->rootIndex();
    const int column = list ? list->modelColumn() : 0;
    for (int row = 0, rows = model->rowCount(root); row < rows && result.size() < MaximumItems; ++row) {
        if (list && list->isRowHidden(row)) {
            continue;
        }
        const QModelIndex index = model->index(row, column, root);
        add(index, view->visualRect(index));
    }
    return result;
}

QList<KeyboardHintOverlay::Target> KeyboardHintOverlay::locationTargets(Pane *pane)
{
    QList<Target> result;
    QWidget *bar = pane ? pane->locationBar() : nullptr;
    if (!bar || !bar->isVisible()) {
        return result;
    }
    QList<QAbstractButton *> buttons;
    const QList<QAbstractButton *> children = bar->findChildren<QAbstractButton *>();
    for (QAbstractButton *button : children) {
        const bool segment = button->objectName() == QLatin1String("paneBreadcrumbSegment") || button->inherits("KDEPrivate::KUrlNavigatorButton");
        if (segment && button->isVisible() && button->isEnabled() && !button->rect().isEmpty()) {
            buttons.append(button);
        }
    }
    // In reading order.
    const bool rightToLeft = bar->layoutDirection() == Qt::RightToLeft;
    std::stable_sort(buttons.begin(), buttons.end(), [bar, rightToLeft](const QAbstractButton *left, const QAbstractButton *right) {
        const int leftX = left->mapTo(bar, QPoint()).x();
        const int rightX = right->mapTo(bar, QPoint()).x();
        return rightToLeft ? leftX > rightX : leftX < rightX;
    });
    for (QAbstractButton *button : std::as_const(buttons)) {
        QString name = button->accessibleName();
        if (name.isEmpty()) {
            name = KLocalizedString::removeAcceleratorMarker(button->text());
        }
        const QPointer<QAbstractButton> guard(button);
        const bool navigatorButton = button->inherits("KDEPrivate::KUrlNavigatorButton");
        result.append({Kind::Location, button, button->rect(), name, [guard, navigatorButton](bool) {
                           if (!guard) {
                               return;
                           }
                           if (navigatorButton) {
                               // KUrlNavigator's buttons open their folder on Return, as on a click, without a menu or drag.
                               QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                               QCoreApplication::sendEvent(guard, &press);
                           } else {
                               guard->click();
                           }
                       }});
    }
    return result;
}

QList<KeyboardHintOverlay::Target> KeyboardHintOverlay::tabTargets(Pane *pane)
{
    QList<Target> result;
    QTabBar *bar = pane ? pane->tabBar() : nullptr;
    if (!bar || !bar->isVisible()) {
        return result;
    }
    const QPointer<QTabBar> guard(bar);
    for (int index = 0; index < bar->count(); ++index) {
        const QRect rect = bar->tabRect(index) & bar->rect();
        if (index == bar->currentIndex() || !bar->isTabVisible(index) || !bar->isTabEnabled(index) || rect.isEmpty()) {
            continue;
        }
        result.append({Kind::Tab, bar, rect, KLocalizedString::removeAcceleratorMarker(bar->tabText(index)), [guard, index](bool) {
                           if (guard && index < guard->count()) {
                               guard->setCurrentIndex(index);
                           }
                       }});
    }
    return result;
}

QList<KeyboardHintOverlay::Target> KeyboardHintOverlay::placeTargets(KFilePlacesView *view)
{
    QList<Target> result;
    QAbstractItemModel *model = view ? view->model() : nullptr;
    if (!model || !view->isVisible()) {
        return result;
    }
    QWidget *viewport = view->viewport();
    const QRect visible = viewport->rect();
    const QPointer<KFilePlacesView> guard(view);
    // The first place of a section also draws the section's heading; the label goes by the place.
    const int rowHeight = qMax(view->iconSize().height(), view->fontMetrics().height()) + 8;
    for (int row = 0, rows = model->rowCount(view->rootIndex()); row < rows; ++row) {
        if (view->isRowHidden(row)) {
            continue;
        }
        const QModelIndex index = model->index(row, 0, view->rootIndex());
        QRect rect = view->visualRect(index);
        rect.setTop(qMax(rect.top(), rect.bottom() + 1 - rowHeight));
        rect &= visible;
        if (rect.isEmpty()) {
            continue;
        }
        const QPersistentModelIndex persistent(index);
        result.append({Kind::Place, viewport, rect, index.data(Qt::DisplayRole).toString(), [guard, persistent](bool) {
                           if (!guard || !persistent.isValid()) {
                               return;
                           }
                           // As Return on a place: it sets up a device first, then opens it.
                           guard->setCurrentIndex(persistent);
                           QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                           QCoreApplication::sendEvent(guard, &press);
                       }});
    }
    return result;
}

bool KeyboardHintOverlay::start(Pane *pane, const QList<KFilePlacesView *> &places, const QString &alphabet, ItemAction action)
{
    QList<Target> found;
    QAbstractItemView *view = pane && pane->view() ? pane->view()->currentView() : nullptr;
    const QPointer<Pane> guard(pane);
    found += itemTargets(view, [guard, action](const QModelIndex &index, bool shift) {
        QAbstractItemView *current = guard ? guard->view()->currentView() : nullptr;
        if (!current || index.model() != current->model()) {
            return;
        }
        current->setFocus(Qt::ShortcutFocusReason);
        current->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        current->scrollTo(index);
        if ((action == ItemAction::Open) != shift) {
            guard->view()->activateCurrentItem();
        }
    });
    found += locationTargets(pane);
    found += tabTargets(pane);
    for (KFilePlacesView *placesView : places) {
        found += placeTargets(placesView);
    }
    if (!start(found, alphabet)) {
        return false;
    }

    // What moves or replaces the labelled targets hides the labels.
    const auto cancelLabels = [this] {
        cancel();
    };
    if (view) {
        watchView(view);
    }
    for (KFilePlacesView *placesView : places) {
        if (placesView && placesView->isVisible()) {
            watchView(placesView);
        }
    }
    if (pane) {
        connect(pane, &Pane::urlChanged, m_watchers, cancelLabels);
        connect(pane, &Pane::tabCountChanged, m_watchers, cancelLabels);
        connect(pane->tabBar(), &QTabBar::currentChanged, m_watchers, cancelLabels);
        watchResize(pane->locationBar());
    }
    return true;
}

bool KeyboardHintOverlay::start(const QList<Target> &targets, const QString &alphabet)
{
    if (m_active) {
        cancel();
    }
    QWidget *window = parentWidget();
    QList<Target> shown;
    for (const Target &target : targets) {
        if (target.widget && target.activate && !target.rect.isEmpty() && target.widget->isVisible() && target.widget->window() == window->window()) {
            shown.append(target);
        }
    }
    QList<Kind> kinds;
    for (const Target &target : std::as_const(shown)) {
        kinds.append(target.kind);
    }
    const QStringList labels = KeyboardHints::assign(kinds, alphabet);
    m_targets.clear();
    m_labels.clear();
    for (int i = 0; i < shown.size(); ++i) {
        if (!labels.at(i).isEmpty()) {
            m_targets.append(shown.at(i));
            m_labels.append(labels.at(i));
        }
    }
    if (m_targets.isEmpty()) {
        return false;
    }
    m_matcher = KeyboardHints::Matcher(m_labels);
    m_previousFocus = QApplication::focusWidget();
    setGeometry(window->rect());

    QFont font = this->font();
    font.setBold(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, palette.color(QPalette::ToolTipBase));
    palette.setColor(QPalette::WindowText, palette.color(QPalette::ToolTipText));
    const QRect bounds = rect();
    for (int i = 0; i < m_targets.size(); ++i) {
        const Target &target = m_targets.at(i);
        auto *label = new QLabel(this);
        label->setObjectName(QStringLiteral("keyboardHintLabel"));
        label->setFont(font);
        label->setPalette(palette);
        label->setAutoFillBackground(true);
        label->setFrameShape(QFrame::Box);
        label->setContentsMargins(2, 0, 2, 0);
        label->setTextFormat(Qt::RichText);
        label->setAccessibleName(i18nc("@info accessible name of a keyboard hint, %1 its keys, %2 its target", "%1: %2", m_labels.at(i), target.name));
        label->setAccessibleDescription(kindText(target.kind));
        m_labelWidgets.append(label);

        // At the start of the target, centred on a row or at the top of a tall item.
        label->setText(m_labels.at(i));
        label->adjustSize();
        const QSize size = label->size();
        QRect area(target.widget->mapTo(window, target.rect.topLeft()), target.rect.size());
        area.moveTopLeft(mapFrom(window, area.topLeft()));
        const bool rightToLeft = target.widget->layoutDirection() == Qt::RightToLeft;
        const int x = rightToLeft ? area.right() + 1 - size.width() - 2 : area.left() + 2;
        const int y = area.height() < size.height() * 3 ? area.top() + (area.height() - size.height()) / 2 : area.top() + 2;
        label->move(qBound(bounds.left(), x, qMax(bounds.left(), bounds.right() + 1 - size.width())),
                    qBound(bounds.top(), y, qMax(bounds.top(), bounds.bottom() + 1 - size.height())));
    }
    updateLabels();

    m_active = true;
    show();
    raise();
    setFocus(Qt::ShortcutFocusReason);
    m_watchers = new QObject(this);
    connect(qApp, &QApplication::focusChanged, m_watchers, [this](QWidget *, QWidget *now) {
        if (now != this) {
            finish(false);
        }
    });
    watchResize(window);
    for (const Target &target : std::as_const(m_targets)) {
        watchResize(target.widget);
    }
    YaziColumns::announce(this,
                          i18ncp("@info accessible announcement",
                                 "%1 keyboard hint shown; type its keys to go there, or press Escape to cancel",
                                 "%1 keyboard hints shown; type a label's keys to go there, or press Escape to cancel",
                                 m_targets.size()));
    return true;
}

void KeyboardHintOverlay::cancel()
{
    finish(true);
}

bool KeyboardHintOverlay::isActive() const
{
    return m_active;
}

QList<KeyboardHintOverlay::Target> KeyboardHintOverlay::targets() const
{
    return m_targets;
}

QStringList KeyboardHintOverlay::labels() const
{
    return m_labels;
}

QList<QLabel *> KeyboardHintOverlay::labelWidgets() const
{
    return m_labelWidgets;
}

QString KeyboardHintOverlay::typed() const
{
    return m_matcher.typed();
}

bool KeyboardHintOverlay::event(QEvent *event)
{
    if (m_active && event->type() == QEvent::ShortcutOverride) {
        // Every key is the overlay's while it shows, so none runs a window shortcut.
        if (!isModifierKey(static_cast<QKeyEvent *>(event)->key())) {
            event->accept();
            return true;
        }
    }
    return QWidget::event(event);
}

void KeyboardHintOverlay::keyPressEvent(QKeyEvent *event)
{
    if (!m_active) {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    if (event->isAutoRepeat() || isModifierKey(event->key())) {
        return;
    }
    switch (event->key()) {
    case Qt::Key_Escape:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        cancel();
        return;
    case Qt::Key_Backspace:
        if (m_matcher.backspace()) {
            updateLabels();
        } else {
            cancel();
        }
        return;
    default:
        break;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier | Qt::GroupSwitchModifier);
    const QString text = event->text();
    if (modifiers != Qt::NoModifier || text.size() != 1 || !text.at(0).isPrint() || text.at(0).isSpace()) {
        cancel();
        return;
    }
    switch (m_matcher.feed(text.at(0))) {
    case KeyboardHints::Matcher::Result::NoMatch:
        cancel();
        return;
    case KeyboardHints::Matcher::Result::Pending:
        updateLabels();
        return;
    case KeyboardHints::Matcher::Result::Matched:
        break;
    }
    const Target target = m_targets.value(m_matcher.matched());
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    finish(true);
    if (target.widget && target.activate) {
        target.activate(shift);
    }
}

bool KeyboardHintOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (m_active) {
        const QEvent::Type type = event->type();
        const auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && (type == QEvent::Resize || type == QEvent::Hide || (type == QEvent::Move && !widget->isWindow()) || type == QEvent::LayoutDirectionChange)
            && std::any_of(m_watched.cbegin(), m_watched.cend(), [widget](const QPointer<QWidget> &watchedWidget) {
                   return watchedWidget == widget;
               })) {
            cancel();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void KeyboardHintOverlay::watchResize(QWidget *widget)
{
    if (!widget || std::any_of(m_watched.cbegin(), m_watched.cend(), [widget](const QPointer<QWidget> &watched) {
            return watched == widget;
        })) {
        return;
    }
    widget->installEventFilter(this);
    m_watched.append(widget);
}

void KeyboardHintOverlay::watchView(QAbstractItemView *view)
{
    const auto cancelLabels = [this] {
        cancel();
    };
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, m_watchers, cancelLabels);
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, m_watchers, cancelLabels);
    if (QAbstractItemModel *model = view->model()) {
        connect(model, &QAbstractItemModel::modelReset, m_watchers, cancelLabels);
        connect(model, &QAbstractItemModel::layoutChanged, m_watchers, cancelLabels);
        connect(model, &QAbstractItemModel::rowsInserted, m_watchers, cancelLabels);
        connect(model, &QAbstractItemModel::rowsRemoved, m_watchers, cancelLabels);
        connect(model, &QAbstractItemModel::rowsMoved, m_watchers, cancelLabels);
    }
    watchResize(view->viewport());
}

void KeyboardHintOverlay::updateLabels()
{
    const QString typed = m_matcher.typed().toHtmlEscaped();
    const QPalette palette = m_labelWidgets.isEmpty() ? this->palette() : m_labelWidgets.constFirst()->palette();
    const QString dim = mixed(palette.color(QPalette::WindowText), palette.color(QPalette::Window)).name();
    for (int i = 0; i < m_labelWidgets.size(); ++i) {
        QLabel *label = m_labelWidgets.at(i);
        const bool candidate = m_matcher.isCandidate(i);
        label->setVisible(candidate);
        if (!candidate) {
            continue;
        }
        const QString rest = m_labels.at(i).mid(m_matcher.typed().size()).toHtmlEscaped();
        label->setText(typed.isEmpty() ? rest : QStringLiteral("<span style=\"color:%1\">%2</span>%3").arg(dim, typed, rest));
    }
}

void KeyboardHintOverlay::finish(bool restoreFocus)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (m_watchers) {
        // Deleting the context drops its connections; Qt keeps a running slot alive until it returns.
        delete m_watchers;
        m_watchers = nullptr;
    }
    for (const QPointer<QWidget> &widget : std::as_const(m_watched)) {
        if (widget) {
            widget->removeEventFilter(this);
        }
    }
    m_watched.clear();
    if (restoreFocus && hasFocus() && m_previousFocus) {
        m_previousFocus->setFocus(Qt::OtherFocusReason);
    }
    hide();
    qDeleteAll(m_labelWidgets);
    m_labelWidgets.clear();
    m_targets.clear();
    m_labels.clear();
    m_matcher = KeyboardHints::Matcher();
    Q_EMIT finished();
}
