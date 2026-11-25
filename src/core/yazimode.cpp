// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfigGroup>
#include <KIO/Global>

#include <QJsonObject>
#include <QKeySequence>
#include <QVarLengthArray>

#include <algorithm>

namespace
{
constexpr auto SessionKey = "YaziMode";
constexpr auto WorkspaceKey = "yaziMode";
constexpr auto SessionPreviewKey = "YaziPreview";
constexpr auto WorkspacePreviewKey = "yaziPreview";

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

/// The digit @p key types without modifiers, or -1.
int digitOf(QKeyCombination key)
{
    if (key.keyboardModifiers() != Qt::NoModifier || key.key() < Qt::Key_0 || key.key() > Qt::Key_9) {
        return -1;
    }
    return int(key.key()) - int(Qt::Key_0);
}

/// Whether @p sequence equals @p longer or begins it.
bool startsWith(const YaziMode::KeySequence &longer, const YaziMode::KeySequence &sequence)
{
    return sequence.size() <= longer.size() && std::equal(sequence.cbegin(), sequence.cend(), longer.cbegin());
}

YaziMode::Binding bind(std::initializer_list<QKeyCombination> keys, YaziMode::Command command)
{
    return {YaziMode::KeySequence(keys), command};
}
}

bool YaziMode::readSession(const KConfigGroup &group)
{
    return group.readEntry(SessionKey, false);
}

void YaziMode::writeSession(KConfigGroup &group, bool enabled)
{
    group.writeEntry(SessionKey, enabled);
}

int YaziMode::readSessionPreview(const KConfigGroup &group)
{
    if (!group.hasKey(SessionPreviewKey)) {
        return -1;
    }
    return group.readEntry(SessionPreviewKey, true) ? 1 : 0;
}

void YaziMode::writeSessionPreview(KConfigGroup &group, int preview)
{
    if (preview < 0) {
        group.deleteEntry(SessionPreviewKey);
    } else {
        group.writeEntry(SessionPreviewKey, preview != 0);
    }
}

void YaziMode::writeWorkspacePreview(QJsonObject &object, int preview)
{
    if (preview >= 0) {
        object.insert(QLatin1String(WorkspacePreviewKey), preview != 0);
    }
}

bool YaziMode::readWorkspacePreview(const QJsonObject &object, int &preview)
{
    const auto value = object.value(QLatin1String(WorkspacePreviewKey));
    if (value.isUndefined()) {
        preview = -1;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    preview = value.toBool() ? 1 : 0;
    return true;
}

void YaziMode::writeWorkspace(QJsonObject &object, bool enabled)
{
    object.insert(QLatin1String(WorkspaceKey), enabled);
}

bool YaziMode::readWorkspace(const QJsonObject &object, bool &enabled)
{
    const auto value = object.value(QLatin1String(WorkspaceKey));
    if (value.isUndefined()) {
        enabled = false;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    enabled = value.toBool();
    return true;
}

QUrl YaziMode::parentFolder(const QUrl &current)
{
    if (!current.isValid() || current.isEmpty()) {
        return {};
    }
    const QUrl parent = KIO::upUrl(current);
    if (!parent.isValid() || parent.isEmpty() || parent.adjusted(QUrl::StripTrailingSlash) == current.adjusted(QUrl::StripTrailingSlash)) {
        return {};
    }
    return parent;
}

YaziMode::ColumnWidths YaziMode::columnWidths(int available, bool preview, const Ratios &ratios, int handleWidth)
{
    ColumnWidths widths;
    if (available <= 0) {
        return widths;
    }
    const int handle = qMax(0, handleWidth);
    const bool showPreview = preview && available >= ThreeColumnWidth + 2 * handle;
    const bool showParent = showPreview || available >= TwoColumnWidth + handle;
    if (!showParent) {
        widths.current = available;
        return widths;
    }

    struct Column {
        int *width;
        qint64 weight;
        int minimum;
    };
    QVarLengthArray<Column, 3> pending{{&widths.parent, qMax(1, ratios.parent), MinimumParentWidth},
                                       {&widths.current, qMax(1, ratios.current), MinimumCurrentWidth}};
    if (showPreview) {
        pending.append({&widths.preview, qMax(1, ratios.preview), MinimumPreviewWidth});
    }
    int remaining = available - handle * int(pending.size() - 1);
    // Columns whose share is below their minimum get the minimum, and the
    // others share what is left. The thresholds above leave room for every
    // minimum, so at least one column always keeps its proportional share.
    for (bool clamped = true; clamped;) {
        clamped = false;
        qint64 total = 0;
        for (const Column &column : pending) {
            total += column.weight;
        }
        for (auto it = pending.begin(); it != pending.end(); ++it) {
            if (qint64(remaining) * it->weight / total < it->minimum) {
                *it->width = it->minimum;
                remaining -= it->minimum;
                pending.erase(it);
                clamped = true;
                break;
            }
        }
    }
    qint64 total = 0;
    for (const Column &column : pending) {
        total += column.weight;
    }
    int assigned = 0;
    for (const Column &column : pending) {
        *column.width = int(qint64(remaining) * column.weight / total);
        assigned += *column.width;
    }
    // Rounding leftovers go to the current column when it isn't at its minimum.
    const bool currentPending = std::any_of(pending.cbegin(), pending.cend(), [&widths](const Column &column) {
        return column.width == &widths.current;
    });
    *(currentPending ? &widths.current : pending.last().width) += remaining - assigned;
    return widths;
}

QString YaziMode::commandId(Command command)
{
    switch (command) {
    case Command::MoveUp:
        return QStringLiteral("move-up");
    case Command::MoveDown:
        return QStringLiteral("move-down");
    case Command::MoveFirst:
        return QStringLiteral("move-first");
    case Command::MoveLast:
        return QStringLiteral("move-last");
    case Command::PageUp:
        return QStringLiteral("page-up");
    case Command::PageDown:
        return QStringLiteral("page-down");
    case Command::HalfPageUp:
        return QStringLiteral("half-page-up");
    case Command::HalfPageDown:
        return QStringLiteral("half-page-down");
    case Command::Parent:
        return QStringLiteral("parent");
    case Command::Open:
        return QStringLiteral("open");
    case Command::Back:
        return QStringLiteral("back");
    case Command::Forward:
        return QStringLiteral("forward");
    case Command::Home:
        return QStringLiteral("home");
    case Command::FocusParent:
        return QStringLiteral("focus-parent");
    case Command::FocusCurrent:
        return QStringLiteral("focus-current");
    case Command::FocusPreview:
        return QStringLiteral("focus-preview");
    case Command::TogglePreview:
        return QStringLiteral("toggle-preview");
    case Command::Leave:
        return QStringLiteral("leave");
    case Command::ToggleSelection:
        return QStringLiteral("toggle-selection");
    case Command::ToggleSelectionDown:
        return QStringLiteral("toggle-selection-down");
    case Command::VisualSelection:
        return QStringLiteral("visual-selection");
    case Command::SelectAll:
        return QStringLiteral("select-all");
    case Command::Copy:
        return QStringLiteral("copy");
    case Command::Cut:
        return QStringLiteral("cut");
    case Command::Paste:
        return QStringLiteral("paste");
    case Command::Rename:
        return QStringLiteral("rename");
    case Command::Trash:
        return QStringLiteral("trash");
    case Command::Create:
        return QStringLiteral("create");
    case Command::OpenWith:
        return QStringLiteral("open-with");
    case Command::CopyToOtherPane:
        return QStringLiteral("copy-to-other-pane");
    case Command::MoveToOtherPane:
        return QStringLiteral("move-to-other-pane");
    case Command::Undo:
        return QStringLiteral("undo");
    case Command::ToggleHidden:
        return QStringLiteral("toggle-hidden");
    }
    return {};
}

bool YaziMode::commandTakesCount(Command command)
{
    switch (command) {
    case Command::MoveUp:
    case Command::MoveDown:
    case Command::PageUp:
    case Command::PageDown:
    case Command::HalfPageUp:
    case Command::HalfPageDown:
    case Command::ToggleSelectionDown:
        return true;
    default:
        return false;
    }
}

bool YaziMode::isSelectionCommand(Command command)
{
    switch (command) {
    case Command::ToggleSelection:
    case Command::ToggleSelectionDown:
    case Command::VisualSelection:
    case Command::SelectAll:
        return true;
    default:
        return false;
    }
}

bool YaziMode::isFileCommand(Command command)
{
    switch (command) {
    case Command::Copy:
    case Command::Cut:
    case Command::Paste:
    case Command::Rename:
    case Command::Trash:
    case Command::Create:
    case Command::OpenWith:
    case Command::CopyToOtherPane:
    case Command::MoveToOtherPane:
    case Command::Undo:
    case Command::ToggleHidden:
        return true;
    default:
        return false;
    }
}

bool YaziMode::commandUsesSelection(Command command)
{
    switch (command) {
    case Command::Copy:
    case Command::Cut:
    case Command::Rename:
    case Command::Trash:
    case Command::OpenWith:
    case Command::CopyToOtherPane:
    case Command::MoveToOtherPane:
        return true;
    default:
        return false;
    }
}

QString YaziMode::commandAction(Command command)
{
    switch (command) {
    case Command::Copy:
        return QStringLiteral("edit_copy");
    case Command::Cut:
        return QStringLiteral("edit_cut");
    case Command::Paste:
        return QStringLiteral("edit_paste");
    case Command::Rename:
        return QStringLiteral("rename");
    case Command::Trash:
        return QStringLiteral("move_to_trash");
    case Command::Create:
        return QStringLiteral("new_menu");
    case Command::CopyToOtherPane:
        return QStringLiteral("copy_to_other_pane");
    case Command::MoveToOtherPane:
        return QStringLiteral("move_to_other_pane");
    case Command::Undo:
        return QStringLiteral("edit_undo");
    case Command::ToggleHidden:
        return QStringLiteral("show_hidden_files");
    default:
        // Open With shows the applications menu of the context menu; it has no window action.
        return {};
    }
}

QStringList YaziMode::permanentDeletionActions()
{
    return {QStringLiteral("delete")};
}

YaziMode::Keymap YaziMode::presetKeymap(Preset preset)
{
    Keymap keymap;
    keymap.bindings = {
        bind({Qt::Key_Up}, Command::MoveUp),
        bind({Qt::Key_Down}, Command::MoveDown),
        bind({Qt::Key_Home}, Command::MoveFirst),
        bind({Qt::Key_End}, Command::MoveLast),
        bind({Qt::Key_PageUp}, Command::PageUp),
        bind({Qt::Key_PageDown}, Command::PageDown),
        bind({Qt::Key_Left}, Command::Parent),
        bind({Qt::Key_Right}, Command::Open),
        bind({Qt::Key_Return}, Command::Open),
        bind({Qt::Key_Enter}, Command::Open),
        // Space alone stays the window's Open Gallery shortcut, and the
        // window's file shortcuts, such as Ctrl+C and Delete, pass through.
        bind({Qt::ControlModifier | Qt::Key_Space}, Command::ToggleSelection),
    };
    if (preset == Preset::Vim) {
        keymap.counts = true;
        keymap.bindings += {
            bind({Qt::Key_K}, Command::MoveUp),
            bind({Qt::Key_J}, Command::MoveDown),
            bind({Qt::Key_G, Qt::Key_G}, Command::MoveFirst),
            bind({Qt::ShiftModifier | Qt::Key_G}, Command::MoveLast),
            bind({Qt::ControlModifier | Qt::Key_U}, Command::HalfPageUp),
            bind({Qt::ControlModifier | Qt::Key_D}, Command::HalfPageDown),
            bind({Qt::Key_H}, Command::Parent),
            bind({Qt::Key_L}, Command::Open),
            bind({Qt::ShiftModifier | Qt::Key_H}, Command::Back),
            bind({Qt::ShiftModifier | Qt::Key_L}, Command::Forward),
            bind({Qt::Key_G, Qt::Key_H}, Command::Home),
            bind({Qt::Key_G, Qt::Key_P}, Command::FocusParent),
            bind({Qt::Key_G, Qt::Key_C}, Command::FocusCurrent),
            bind({Qt::Key_G, Qt::Key_V}, Command::FocusPreview),
            bind({Qt::Key_Z, Qt::Key_P}, Command::TogglePreview),
            bind({Qt::Key_Q}, Command::Leave),
            bind({Qt::Key_Space}, Command::ToggleSelectionDown),
            bind({Qt::Key_V}, Command::VisualSelection),
            bind({Qt::ShiftModifier | Qt::Key_V}, Command::SelectAll),
            bind({Qt::ControlModifier | Qt::Key_A}, Command::SelectAll),
            bind({Qt::Key_Y, Qt::Key_Y}, Command::Copy),
            bind({Qt::Key_X}, Command::Cut),
            bind({Qt::Key_P}, Command::Paste),
            bind({Qt::Key_R}, Command::Rename),
            // Never permanent deletion: D is deliberately left unbound.
            bind({Qt::Key_D, Qt::Key_D}, Command::Trash),
            bind({Qt::Key_A}, Command::Create),
            bind({Qt::Key_O}, Command::OpenWith),
            bind({Qt::Key_C}, Command::CopyToOtherPane),
            bind({Qt::Key_M}, Command::MoveToOtherPane),
            bind({Qt::Key_U}, Command::Undo),
            bind({Qt::Key_Period}, Command::ToggleHidden),
        };
    }
    return keymap;
}

QList<YaziMode::Conflict> YaziMode::conflicts(const Keymap &keymap)
{
    QList<Conflict> found;
    const QList<Binding> &bindings = keymap.bindings;
    for (int i = 0; i < bindings.size(); ++i) {
        const KeySequence &keys = bindings.at(i).keys;
        const bool unusable = keys.isEmpty() || std::any_of(keys.cbegin(), keys.cend(), [](QKeyCombination key) {
                                  return key.key() == Qt::Key_Escape || isModifierKey(key.key());
                              });
        if (unusable || (keymap.counts && digitOf(keys.first()) > 0)) {
            found.append({i, -1});
            continue;
        }
        for (int j = i + 1; j < bindings.size(); ++j) {
            const KeySequence &other = bindings.at(j).keys;
            if (!other.isEmpty() && (startsWith(keys, other) || startsWith(other, keys))) {
                found.append({i, j});
            }
        }
    }
    return found;
}

QKeyCombination YaziMode::normalizedKey(QKeyCombination key)
{
    return QKeyCombination(key.keyboardModifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier), key.key());
}

QString YaziMode::keyText(QKeyCombination key)
{
    const Qt::KeyboardModifiers modifiers = key.keyboardModifiers();
    if (key.key() >= Qt::Key_A && key.key() <= Qt::Key_Z && (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)) {
        const QChar letter(char16_t(u'a' + (int(key.key()) - int(Qt::Key_A))));
        return modifiers == Qt::ShiftModifier ? QString(letter.toUpper()) : QString(letter);
    }
    return QKeySequence(key).toString(QKeySequence::NativeText);
}

QString YaziMode::sequenceText(const KeySequence &keys, int count)
{
    QStringList parts;
    bool singleCharacters = true;
    for (QKeyCombination key : keys) {
        parts.append(keyText(key));
        singleCharacters = singleCharacters && parts.last().size() == 1;
    }
    if (count > 0) {
        parts.prepend(QString::number(count));
    }
    return parts.join(singleCharacters ? QString() : QStringLiteral(" "));
}

YaziMode::KeyMatcher::KeyMatcher(const Keymap &keymap)
    : m_keymap(keymap)
{
}

void YaziMode::KeyMatcher::setKeymap(const Keymap &keymap)
{
    m_keymap = keymap;
    reset();
}

const YaziMode::Keymap &YaziMode::KeyMatcher::keymap() const
{
    return m_keymap;
}

YaziMode::KeyMatcher::Result YaziMode::KeyMatcher::feed(QKeyCombination key)
{
    key = normalizedKey(key);
    if (isModifierKey(key.key())) {
        return isPending() ? Result::Pending : Result::NoMatch;
    }
    if (key.key() == Qt::Key_Escape) {
        if (!isPending()) {
            return Result::NoMatch;
        }
        reset();
        return Result::Cancelled;
    }
    if (isPending()) {
        const Result result = advance(key);
        if (result != Result::NoMatch) {
            return result;
        }
        reset();
    }
    return advance(key);
}

YaziMode::KeyMatcher::Result YaziMode::KeyMatcher::test(QKeyCombination key) const
{
    KeyMatcher copy = *this;
    return copy.feed(key);
}

YaziMode::KeyMatcher::Result YaziMode::KeyMatcher::advance(QKeyCombination key)
{
    const int digit = m_keymap.counts && m_pending.isEmpty() ? digitOf(key) : -1;
    if (digit > 0 || (digit == 0 && m_pendingCount > 0)) {
        m_pendingCount = qMin(MaximumCount, m_pendingCount * 10 + digit);
        return Result::Pending;
    }
    KeySequence typed = m_pending;
    typed.append(key);
    bool longer = false;
    for (const Binding &binding : std::as_const(m_keymap.bindings)) {
        if (!startsWith(binding.keys, typed)) {
            continue;
        }
        if (binding.keys.size() == typed.size()) {
            m_command = binding.command;
            m_count = qMax(1, m_pendingCount);
            m_pending.clear();
            m_pendingCount = 0;
            return Result::Matched;
        }
        longer = true;
    }
    if (!longer) {
        return Result::NoMatch;
    }
    m_pending = typed;
    return Result::Pending;
}

YaziMode::Command YaziMode::KeyMatcher::command() const
{
    return m_command;
}

int YaziMode::KeyMatcher::count() const
{
    return m_count;
}

bool YaziMode::KeyMatcher::isPending() const
{
    return !m_pending.isEmpty() || m_pendingCount > 0;
}

YaziMode::KeySequence YaziMode::KeyMatcher::pendingKeys() const
{
    return m_pending;
}

int YaziMode::KeyMatcher::pendingCount() const
{
    return m_pendingCount;
}

QString YaziMode::KeyMatcher::pendingText() const
{
    return isPending() ? sequenceText(m_pending, m_pendingCount) : QString();
}

void YaziMode::KeyMatcher::reset()
{
    m_pending.clear();
    m_pendingCount = 0;
}
