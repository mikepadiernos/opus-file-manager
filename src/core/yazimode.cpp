// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfigGroup>
#include <KIO/Global>
#include <KLocalizedString>
#include <KProtocolInfo>
#include <KProtocolManager>

#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QMimeDatabase>
#include <QVarLengthArray>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr auto SessionKey = "YaziMode";
constexpr auto WorkspaceKey = "yaziMode";
constexpr auto SessionPreviewKey = "YaziPreview";
constexpr auto WorkspacePreviewKey = "yaziPreview";
constexpr auto SessionPreviewContentKey = "YaziPreviewContent";
constexpr auto WorkspacePreviewContentKey = "yaziPreviewContent";
constexpr auto SessionParentColumnKey = "YaziParentColumn";
constexpr auto WorkspaceParentColumnKey = "yaziParentColumn";
constexpr auto SessionRatiosKey = "YaziColumnRatios";
constexpr auto WorkspaceRatiosKey = "yaziColumnRatios";

/// The PreviewContent value @p name stands for, compared case-insensitively, or -1.
int previewContentFromName(const QString &name)
{
    if (name.compare(QLatin1String("preview"), Qt::CaseInsensitive) == 0) {
        return int(YaziMode::PreviewContent::Preview);
    }
    if (name.compare(QLatin1String("information"), Qt::CaseInsensitive) == 0) {
        return int(YaziMode::PreviewContent::Information);
    }
    return -1;
}

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

bool isArchiveScheme(const QString &scheme)
{
    static const QStringList schemes{QStringLiteral("zip"),
                                     QStringLiteral("tar"),
                                     QStringLiteral("ar"),
                                     QStringLiteral("sevenz"),
                                     QStringLiteral("krarc"),
                                     QStringLiteral("iso"),
                                     QStringLiteral("rar")};
    return schemes.contains(scheme) || !KProtocolInfo::archiveMimetypes(scheme).isEmpty();
}

/// Whether @p name is an archive file's name, judged by its extension alone.
bool isArchiveName(const QString &name)
{
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension);
    if (!mime.isValid() || mime.isDefault()) {
        return false;
    }
    if (!KProtocolManager::protocolForArchiveMimetype(mime.name()).isEmpty()) {
        return true;
    }
    static const QStringList archives{QStringLiteral("application/zip"),
                                      QStringLiteral("application/x-tar"),
                                      QStringLiteral("application/x-compressed-tar"),
                                      QStringLiteral("application/x-bzip-compressed-tar"),
                                      QStringLiteral("application/x-bzip2-compressed-tar"),
                                      QStringLiteral("application/x-xz-compressed-tar"),
                                      QStringLiteral("application/x-zstd-compressed-tar"),
                                      QStringLiteral("application/x-lzma-compressed-tar"),
                                      QStringLiteral("application/x-tarz"),
                                      QStringLiteral("application/x-archive"),
                                      QStringLiteral("application/x-7z-compressed"),
                                      QStringLiteral("application/x-cd-image"),
                                      QStringLiteral("application/vnd.rar")};
    return std::any_of(archives.cbegin(), archives.cend(), [&mime](const QString &archive) {
        return mime.inherits(archive);
    });
}

/// The local path of the archive file at the root of the archive folder @p url, or empty when @p url is inside it or isn't an archive folder.
QString archiveRootFile(const QUrl &url)
{
    if (url.isLocalFile() || !isArchiveScheme(url.scheme())) {
        return {};
    }
    const QString path = url.adjusted(QUrl::StripTrailingSlash).path();
    const QString name = path.section(QLatin1Char('/'), -1);
    return path.size() > 1 && isArchiveName(name) ? path : QString();
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

bool YaziMode::startupMode(const Activation &activation, std::optional<bool> saved)
{
    return activation.restore && saved ? *saved : activation.defaultMode;
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

int YaziMode::readSessionPreviewContent(const KConfigGroup &group)
{
    return previewContentFromName(group.readEntry(SessionPreviewContentKey, QString()));
}

void YaziMode::writeSessionPreviewContent(KConfigGroup &group, int content)
{
    if (content < 0) {
        group.deleteEntry(SessionPreviewContentKey);
    } else {
        group.writeEntry(SessionPreviewContentKey, content == int(PreviewContent::Information) ? QStringLiteral("Information") : QStringLiteral("Preview"));
    }
}

void YaziMode::writeWorkspacePreviewContent(QJsonObject &object, int content)
{
    if (content >= 0) {
        object.insert(QLatin1String(WorkspacePreviewContentKey),
                      content == int(PreviewContent::Information) ? QStringLiteral("information") : QStringLiteral("preview"));
    }
}

bool YaziMode::readWorkspacePreviewContent(const QJsonObject &object, int &content)
{
    const auto value = object.value(QLatin1String(WorkspacePreviewContentKey));
    if (value.isUndefined()) {
        content = -1;
        return true;
    }
    const int parsed = value.isString() ? previewContentFromName(value.toString()) : -1;
    if (parsed < 0) {
        return false;
    }
    content = parsed;
    return true;
}

void YaziMode::writeWorkspace(QJsonObject &object, bool enabled)
{
    object.insert(QLatin1String(WorkspaceKey), enabled);
}

int YaziMode::readSessionParentColumn(const KConfigGroup &group)
{
    if (!group.hasKey(SessionParentColumnKey)) {
        return -1;
    }
    return group.readEntry(SessionParentColumnKey, true) ? 1 : 0;
}

void YaziMode::writeSessionParentColumn(KConfigGroup &group, int parent)
{
    if (parent < 0) {
        group.deleteEntry(SessionParentColumnKey);
    } else {
        group.writeEntry(SessionParentColumnKey, parent != 0);
    }
}

void YaziMode::writeWorkspaceParentColumn(QJsonObject &object, int parent)
{
    if (parent >= 0) {
        object.insert(QLatin1String(WorkspaceParentColumnKey), parent != 0);
    }
}

bool YaziMode::readWorkspaceParentColumn(const QJsonObject &object, int &parent)
{
    const auto value = object.value(QLatin1String(WorkspaceParentColumnKey));
    if (value.isUndefined()) {
        parent = -1;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    parent = value.toBool() ? 1 : 0;
    return true;
}

YaziMode::Ratios YaziMode::clampedRatios(const Ratios &ratios)
{
    Ratios result{qMax(1, ratios.parent), qMax(1, ratios.current), qMax(1, ratios.preview)};
    const int largest = std::max({result.parent, result.current, result.preview});
    if (largest > MaximumRatio) {
        const auto scaled = [largest](int value) {
            return int(qMax<qint64>(1, qint64(value) * MaximumRatio / largest));
        };
        result = {scaled(result.parent), scaled(result.current), scaled(result.preview)};
    }
    return result;
}

QList<int> YaziMode::ratiosToList(const Ratios &ratios)
{
    return {ratios.parent, ratios.current, ratios.preview};
}

std::optional<YaziMode::Ratios> YaziMode::ratiosFromList(const QList<int> &list)
{
    if (list.size() != 3 || std::any_of(list.cbegin(), list.cend(), [](int value) {
            return value < 1;
        })) {
        return std::nullopt;
    }
    return clampedRatios({list.at(0), list.at(1), list.at(2)});
}

QList<int> YaziMode::readSessionRatios(const KConfigGroup &group)
{
    const std::optional<Ratios> ratios = ratiosFromList(group.readEntry(SessionRatiosKey, QList<int>()));
    return ratios ? ratiosToList(*ratios) : QList<int>();
}

void YaziMode::writeSessionRatios(KConfigGroup &group, const QList<int> &ratios)
{
    if (ratios.isEmpty()) {
        group.deleteEntry(SessionRatiosKey);
    } else {
        group.writeEntry(SessionRatiosKey, ratios);
    }
}

void YaziMode::writeWorkspaceRatios(QJsonObject &object, const QList<int> &ratios)
{
    if (ratios.isEmpty()) {
        return;
    }
    QJsonArray array;
    for (int ratio : ratios) {
        array.append(ratio);
    }
    object.insert(QLatin1String(WorkspaceRatiosKey), array);
}

bool YaziMode::readWorkspaceRatios(const QJsonObject &object, QList<int> &ratios)
{
    const auto value = object.value(QLatin1String(WorkspaceRatiosKey));
    if (value.isUndefined()) {
        ratios.clear();
        return true;
    }
    if (!value.isArray()) {
        return false;
    }
    QList<int> list;
    for (const QJsonValue &entry : value.toArray()) {
        const double number = entry.toDouble(-1);
        if (!entry.isDouble() || number != std::floor(number) || number < 1 || number > std::numeric_limits<int>::max()) {
            return false;
        }
        list.append(int(number));
    }
    const std::optional<Ratios> parsed = ratiosFromList(list);
    if (!parsed) {
        return false;
    }
    ratios = ratiosToList(*parsed);
    return true;
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
    // KIO's archive workers can't list the folder around the archive; the local folder that holds it can.
    if (const QString archive = archiveRootFile(current); !archive.isEmpty()) {
        return QUrl::fromLocalFile(archive).adjusted(QUrl::RemoveFilename);
    }
    const QUrl parent = KIO::upUrl(current);
    if (!parent.isValid() || parent.isEmpty() || parent.adjusted(QUrl::StripTrailingSlash) == current.adjusted(QUrl::StripTrailingSlash)) {
        return {};
    }
    return parent;
}

QUrl YaziMode::parentItem(const QUrl &current)
{
    if (const QString archive = archiveRootFile(current); !archive.isEmpty()) {
        return QUrl::fromLocalFile(archive);
    }
    return current;
}

YaziMode::Support YaziMode::support(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        return {SupportLevel::Unsupported, i18nc("@info:status why Yazi mode isn't available", "there is no folder to show.")};
    }
    if (url.isLocalFile()) {
        return {};
    }
    const QString scheme = url.scheme().toLower();
    const auto is = [&scheme](std::initializer_list<const char *> schemes) {
        return std::any_of(schemes.begin(), schemes.end(), [&scheme](const char *each) {
            return scheme == QLatin1String(each);
        });
    };
    if (is({"baloosearch", "filenamesearch", "search"})) {
        return {SupportLevel::Unsupported,
                i18nc("@info:status why Yazi mode isn't available", "search results come from many folders, so there is no parent folder to show.")};
    }
    if (is({"recentlyused", "recentdocuments", "timeline", "activities"})) {
        return {SupportLevel::Unsupported,
                i18nc("@info:status why Yazi mode isn't available", "recent items come from many folders, so there is no parent folder to show.")};
    }
    if (is({"tags"})) {
        return {SupportLevel::Unsupported,
                i18nc("@info:status why Yazi mode isn't available", "tagged items come from many folders, so there is no parent folder to show.")};
    }
    if (is({"remote"})) {
        return {SupportLevel::Unsupported,
                i18nc("@info:status why Yazi mode isn't available", "this lists shortcuts to network places; open one to browse it in columns.")};
    }
    if (is({"applications", "programs", "settings"})) {
        return {SupportLevel::Unsupported, i18nc("@info:status why Yazi mode isn't available", "this location lists applications rather than files.")};
    }
    if (is({"trash"})) {
        return {
            SupportLevel::Limited,
            i18nc("@info:status why Yazi mode is limited", "items in the trash can only be restored or deleted, and the parent column stays in the trash.")};
    }
    if (isArchiveScheme(scheme)) {
        return {SupportLevel::Limited, i18nc("@info:status why Yazi mode is limited", "archives are read-only here, and previews extract each file first.")};
    }
    if (is({"desktop"})) {
        return {SupportLevel::Limited, i18nc("@info:status why Yazi mode is limited", "the parent column stops at the desktop folder.")};
    }
    return {SupportLevel::Limited,
            i18nc("@info:status why Yazi mode is limited", "the parent column lists in the background, and previews download each file first.")};
}

YaziMode::ColumnWidths YaziMode::columnWidths(int available, bool preview, const Ratios &ratios, int handleWidth, bool parent)
{
    ColumnWidths widths;
    if (available <= 0) {
        return widths;
    }
    const int handle = qMax(0, handleWidth);
    const bool showPreview = preview && available >= (parent ? ThreeColumnWidth + 2 * handle : MinimumCurrentWidth + MinimumPreviewWidth + handle);
    const bool showParent = parent && (showPreview || available >= TwoColumnWidth + handle);
    if (!showParent && !showPreview) {
        widths.current = available;
        return widths;
    }

    struct Column {
        int *width;
        qint64 weight;
        int minimum;
    };
    QVarLengthArray<Column, 3> pending;
    if (showParent) {
        pending.append({&widths.parent, qMax(1, ratios.parent), MinimumParentWidth});
    }
    pending.append({&widths.current, qMax(1, ratios.current), MinimumCurrentWidth});
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
    case Command::TogglePreviewContent:
        return QStringLiteral("toggle-preview-content");
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
    case Command::Filter:
        return QStringLiteral("filter");
    case Command::ClearFilter:
        return QStringLiteral("clear-filter");
    case Command::SearchNames:
        return QStringLiteral("search-names");
    case Command::SearchContents:
        return QStringLiteral("search-contents");
    case Command::SortByName:
        return QStringLiteral("sort-by-name");
    case Command::SortBySize:
        return QStringLiteral("sort-by-size");
    case Command::SortByModified:
        return QStringLiteral("sort-by-modified");
    case Command::SortByType:
        return QStringLiteral("sort-by-type");
    case Command::ReverseSort:
        return QStringLiteral("reverse-sort");
    case Command::NewTab:
        return QStringLiteral("new-tab");
    case Command::PreviousTab:
        return QStringLiteral("previous-tab");
    case Command::NextTab:
        return QStringLiteral("next-tab");
    case Command::CloseTab:
        return QStringLiteral("close-tab");
    }
    return {};
}

QString YaziMode::commandText(Command command)
{
    switch (command) {
    case Command::MoveUp:
        return i18nc("@item Yazi mode command", "Move up");
    case Command::MoveDown:
        return i18nc("@item Yazi mode command", "Move down");
    case Command::MoveFirst:
        return i18nc("@item Yazi mode command", "Move to the first item");
    case Command::MoveLast:
        return i18nc("@item Yazi mode command", "Move to the last item");
    case Command::PageUp:
        return i18nc("@item Yazi mode command", "Move a page up");
    case Command::PageDown:
        return i18nc("@item Yazi mode command", "Move a page down");
    case Command::HalfPageUp:
        return i18nc("@item Yazi mode command", "Move half a page up");
    case Command::HalfPageDown:
        return i18nc("@item Yazi mode command", "Move half a page down");
    case Command::Parent:
        return i18nc("@item Yazi mode command", "Open the parent folder");
    case Command::Open:
        return i18nc("@item Yazi mode command", "Open the focused folder or file");
    case Command::Back:
        return i18nc("@item Yazi mode command", "Go back");
    case Command::Forward:
        return i18nc("@item Yazi mode command", "Go forward");
    case Command::Home:
        return i18nc("@item Yazi mode command", "Open the home folder");
    case Command::FocusParent:
        return i18nc("@item Yazi mode command", "Focus the parent column");
    case Command::FocusCurrent:
        return i18nc("@item Yazi mode command", "Focus the current column");
    case Command::FocusPreview:
        return i18nc("@item Yazi mode command", "Focus the preview column");
    case Command::TogglePreview:
        return i18nc("@item Yazi mode command", "Show or hide the preview column");
    case Command::TogglePreviewContent:
        return i18nc("@item Yazi mode command", "Switch the preview column to file information, or back");
    case Command::Leave:
        return i18nc("@item Yazi mode command", "Leave Yazi mode");
    case Command::ToggleSelection:
        return i18nc("@item Yazi mode command", "Select or deselect the focused item");
    case Command::ToggleSelectionDown:
        return i18nc("@item Yazi mode command", "Select or deselect the focused item, then move down");
    case Command::VisualSelection:
        return i18nc("@item Yazi mode command", "Start or end range selection");
    case Command::SelectAll:
        return i18nc("@item Yazi mode command", "Select all");
    case Command::Copy:
        return i18nc("@item Yazi mode command", "Copy the items to the clipboard");
    case Command::Cut:
        return i18nc("@item Yazi mode command", "Cut the items to the clipboard");
    case Command::Paste:
        return i18nc("@item Yazi mode command", "Paste into the current folder");
    case Command::Rename:
        return i18nc("@item Yazi mode command", "Rename the items");
    case Command::Trash:
        return i18nc("@item Yazi mode command", "Move the items to the Trash");
    case Command::Create:
        return i18nc("@item Yazi mode command", "Create a new folder or file");
    case Command::OpenWith:
        return i18nc("@item Yazi mode command", "Open the items with another application");
    case Command::CopyToOtherPane:
        return i18nc("@item Yazi mode command", "Copy the items to the other pane");
    case Command::MoveToOtherPane:
        return i18nc("@item Yazi mode command", "Move the items to the other pane");
    case Command::Undo:
        return i18nc("@item Yazi mode command", "Undo the last file operation");
    case Command::ToggleHidden:
        return i18nc("@item Yazi mode command", "Show or hide hidden files");
    case Command::Filter:
        return i18nc("@item Yazi mode command", "Open the pane's filter bar");
    case Command::ClearFilter:
        return i18nc("@item Yazi mode command", "Clear the pane's filter");
    case Command::SearchNames:
        return i18nc("@item Yazi mode command", "Search file names in the current folder");
    case Command::SearchContents:
        return i18nc("@item Yazi mode command", "Search file contents in the current folder");
    case Command::SortByName:
        return i18nc("@item Yazi mode command", "Sort by name");
    case Command::SortBySize:
        return i18nc("@item Yazi mode command", "Sort by size");
    case Command::SortByModified:
        return i18nc("@item Yazi mode command", "Sort by modification date");
    case Command::SortByType:
        return i18nc("@item Yazi mode command", "Sort by type");
    case Command::ReverseSort:
        return i18nc("@item Yazi mode command", "Reverse the sort order");
    case Command::NewTab:
        return i18nc("@item Yazi mode command", "Open the focused folder in a new tab");
    case Command::PreviousTab:
        return i18nc("@item Yazi mode command", "Go to the previous tab");
    case Command::NextTab:
        return i18nc("@item Yazi mode command", "Go to the next tab");
    case Command::CloseTab:
        return i18nc("@item Yazi mode command", "Close the current tab");
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
            bind({Qt::Key_I}, Command::TogglePreviewContent),
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
            bind({Qt::Key_Slash}, Command::Filter),
            bind({Qt::Key_Backslash}, Command::ClearFilter),
            bind({Qt::Key_S}, Command::SearchNames),
            bind({Qt::ShiftModifier | Qt::Key_S}, Command::SearchContents),
            bind({Qt::Key_Comma, Qt::Key_N}, Command::SortByName),
            bind({Qt::Key_Comma, Qt::Key_S}, Command::SortBySize),
            bind({Qt::Key_Comma, Qt::Key_M}, Command::SortByModified),
            bind({Qt::Key_Comma, Qt::Key_E}, Command::SortByType),
            bind({Qt::Key_Comma, Qt::Key_R}, Command::ReverseSort),
            bind({Qt::Key_T}, Command::NewTab),
            bind({Qt::Key_BracketLeft}, Command::PreviousTab),
            bind({Qt::Key_BracketRight}, Command::NextTab),
            bind({Qt::Key_G, Qt::ShiftModifier | Qt::Key_T}, Command::PreviousTab),
            bind({Qt::Key_G, Qt::Key_T}, Command::NextTab),
            bind({Qt::Key_G, Qt::Key_W}, Command::CloseTab),
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

QList<YaziMode::ShadowedShortcut> YaziMode::shadowedShortcuts(const Keymap &keymap, const QList<ActionShortcuts> &actions)
{
    const KeyMatcher matcher(keymap);
    QList<ShadowedShortcut> found;
    for (const ActionShortcuts &action : actions) {
        for (const QKeySequence &shortcut : action.shortcuts) {
            if (shortcut.isEmpty()) {
                continue;
            }
            const QKeyCombination key = normalizedKey(shortcut[0]);
            if (matcher.test(key) == KeyMatcher::Result::NoMatch) {
                continue;
            }
            QList<int> bindings;
            for (int i = 0; i < keymap.bindings.size(); ++i) {
                const KeySequence &keys = keymap.bindings.at(i).keys;
                if (!keys.isEmpty() && keys.first() == key) {
                    bindings.append(i);
                }
            }
            found.append({action.name, action.text, shortcut, bindings});
        }
    }
    return found;
}

QKeyCombination YaziMode::normalizedKey(QKeyCombination key)
{
    Qt::KeyboardModifiers modifiers = key.keyboardModifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
    const int code = int(key.key());
    const bool punctuation = (code >= Qt::Key_Exclam && code <= Qt::Key_Slash) || (code >= Qt::Key_Colon && code <= Qt::Key_At)
        || (code >= Qt::Key_BracketLeft && code <= Qt::Key_QuoteLeft) || (code >= Qt::Key_BraceLeft && code <= Qt::Key_AsciiTilde);
    if (punctuation) {
        modifiers &= ~Qt::ShiftModifier;
    }
    return QKeyCombination(modifiers, key.key());
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
