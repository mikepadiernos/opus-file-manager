// SPDX-License-Identifier: GPL-3.0-or-later

#include "carddelegate.h"

#include "appearancestyle.h"
#include "colorschemes.h"
#include "labelcolor.h"
#include "librarymovetracker.h"

#include <KDirModel>
#include <KFileItem>
#include <KLocalizedString>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QLocale>
#include <QMimeDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QTextLayout>

namespace
{
constexpr int NameGap = 6;
constexpr int InformationGap = 2;
constexpr int MarkerSize = 16;

QColor mixed(const QColor &first, const QColor &second, qreal secondAmount)
{
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * secondAmount,
                            first.greenF() + (second.greenF() - first.greenF()) * secondAmount,
                            first.blueF() + (second.blueF() - first.blueF()) * secondAmount);
}

QFont informationFont(const QFont &font)
{
    QFont small(font);
    small.setBold(false);
    if (small.pointSizeF() > 0) {
        small.setPointSizeF(qMax(6.0, small.pointSizeF() * 0.9));
    } else if (small.pixelSize() > 0) {
        small.setPixelSize(qMax(8, qRound(small.pixelSize() * 0.9)));
    }
    return small;
}

QString labelText(LabelColor color)
{
    switch (color) {
    case LabelColor::Red:
        return i18nc("@item colour label", "Red");
    case LabelColor::Orange:
        return i18nc("@item colour label", "Orange");
    case LabelColor::Yellow:
        return i18nc("@item colour label", "Yellow");
    case LabelColor::Green:
        return i18nc("@item colour label", "Green");
    case LabelColor::Blue:
        return i18nc("@item colour label", "Blue");
    case LabelColor::Purple:
        return i18nc("@item colour label", "Purple");
    case LabelColor::Pink:
        return i18nc("@item colour label", "Pink");
    case LabelColor::Gray:
        return i18nc("@item colour label", "Gray");
    case LabelColor::None:
        break;
    }
    return {};
}

QString dateText(const QDateTime &time)
{
    return time.isValid() ? QLocale().toString(time, QLocale::ShortFormat) : QString();
}

/// Wraps @p name onto at most @p lines lines, eliding the middle of the last one.
QStringList nameLines(const QString &name, const QFont &font, int width, int lines)
{
    QStringList result;
    if (width <= 0) {
        return result;
    }
    const QFontMetrics metrics(font);
    QTextLayout layout(name, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    for (int i = 0; i < lines; ++i) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        const bool last = i == lines - 1;
        if (last && line.textStart() + line.textLength() < name.size()) {
            result.append(metrics.elidedText(name.mid(line.textStart()), Qt::ElideMiddle, width));
        } else {
            result.append(name.mid(line.textStart(), line.textLength()).trimmed());
        }
    }
    layout.endLayout();
    return result;
}
}

CardDelegate::CardDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

int CardDelegate::previewSize() const
{
    return m_previewSize;
}

void CardDelegate::setPreviewSize(int size)
{
    m_previewSize = qMax(16, size);
}

void CardDelegate::setItemSpace(int space)
{
    m_space = qMax(0, space);
}

int CardDelegate::itemSpace() const
{
    return m_space;
}

void CardDelegate::setInformation(const QStringList &keys)
{
    m_information = keys;
}

QStringList CardDelegate::information() const
{
    return m_information;
}

void CardDelegate::setCellWidth(int width)
{
    m_cellWidth = qMax(0, width);
}

void CardDelegate::setSelectionMarkersVisible(std::function<bool()> visible)
{
    m_markersVisible = std::move(visible);
}

int CardDelegate::minimumCellWidth() const
{
    return 2 * (Padding + m_space) + qMin(m_previewSize, 48);
}

QSize CardDelegate::cellSize(const QFont &font) const
{
    const QFontMetrics metrics(font);
    const QFontMetrics small(informationFont(font));
    const int naturalWidth = qMax(m_previewSize, MinimumTextWidth) + 2 * (Padding + m_space);
    const int width = m_cellWidth > 0 ? qMax(m_cellWidth, minimumCellWidth()) : naturalWidth;
    int height = 2 * (Padding + m_space) + m_previewSize + NameGap + NameLines * metrics.lineSpacing();
    if (!m_information.isEmpty()) {
        height += InformationGap + m_information.size() * small.lineSpacing();
    }
    return {width, height};
}

QRect CardDelegate::cardRect(const QRect &cell, int space)
{
    return cell.adjusted(space, space, -space, -space);
}

QRect CardDelegate::markerRect(const QRect &card, Qt::LayoutDirection direction)
{
    const int x = direction == Qt::RightToLeft ? card.right() - 4 - MarkerSize + 1 : card.left() + 4;
    return {x, card.top() + 4, MarkerSize, MarkerSize};
}

QRect CardDelegate::previewRect(const QRect &card) const
{
    return {card.left() + Padding, card.top() + Padding, qMax(0, card.width() - 2 * Padding), m_previewSize};
}

QRect CardDelegate::nameRect(const QRect &card, const QFontMetrics &metrics) const
{
    const QRect preview = previewRect(card);
    return {preview.left(), preview.bottom() + 1 + NameGap, preview.width(), NameLines * metrics.lineSpacing()};
}

QString CardDelegate::informationText(const QModelIndex &index, const QString &key)
{
    const KFileItem item = index.data(KDirModel::FileItemRole).value<KFileItem>();
    if (item.isNull()) {
        return {};
    }
    if (key == QLatin1String("size")) {
        return index.siblingAtColumn(KDirModel::Size).data(Qt::DisplayRole).toString();
    }
    if (key == QLatin1String("modificationTime")) {
        return index.siblingAtColumn(KDirModel::ModifiedTime).data(Qt::DisplayRole).toString();
    }
    if (key == QLatin1String("creationTime")) {
        return dateText(item.time(KFileItem::CreationTime));
    }
    if (key == QLatin1String("accessTime")) {
        return dateText(item.time(KFileItem::AccessTime));
    }
    if (key == QLatin1String("permissions")) {
        return item.permissionsString();
    }
    if (key == QLatin1String("octalPermissions")) {
        return QLatin1Char('0') + QString::number(item.permissions() & 07777, 8);
    }
    if (key == QLatin1String("owner")) {
        return item.user();
    }
    if (key == QLatin1String("ownerAndGroup")) {
        return i18nc("@item owner and group of a file", "%1:%2", item.user(), item.group());
    }
    if (key == QLatin1String("mimeType")) {
        return item.mimetype();
    }
    if (key == QLatin1String("friendlyMimeType")) {
        return item.mimeComment();
    }
    if (key == QLatin1String("linkDest")) {
        return item.isLink() ? item.linkDest() : QString();
    }
    if (key == QLatin1String("localPathOrUrl")) {
        return item.url().toDisplayString(QUrl::PreferLocalFile);
    }
    if (key == QLatin1String("comment")) {
        return item.comment();
    }
    if (key == QLatin1String("tags")) {
        return QLocale().createSeparatedList(index.data(CardRoles::Tags).toStringList());
    }
    if (key == QLatin1String("rating")) {
        const int rating = qBound(0, index.data(CardRoles::Rating).toInt(), 5);
        return rating > 0 ? QString(rating, QChar(0x2605)) + QString(5 - rating, QChar(0x2606)) : QString();
    }
    if (key == QLatin1String("label")) {
        return labelText(LabelColors::fromKey(index.data(CardRoles::Label).toString()));
    }
    return {};
}

QStringList CardDelegate::shownInformation(const QModelIndex &index) const
{
    QStringList lines;
    for (const QString &key : m_information) {
        lines.append(informationText(index, key));
    }
    return lines;
}

QSize CardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return cellSize(option.font);
}

void CardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QRect card = cardRect(opt.rect, m_space);
    if (!card.isValid()) {
        return;
    }
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = AppearanceStyle::hoverHighlight() && (opt.state & QStyle::State_MouseOver);
    const bool focused = opt.state & QStyle::State_HasFocus;
    const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
        : (opt.state & QStyle::State_Active)                                ? QPalette::Active
                                                                            : QPalette::Inactive;
    const QPalette &palette = opt.palette;
    const QColor base = palette.color(group, QPalette::Base);
    const QColor text = palette.color(group, QPalette::Text);
    const QColor highlight = palette.color(group, QPalette::Highlight);
    QColor background = mixed(base, text, 0.04);
    QColor border = mixed(base, text, 0.16);
    const AppearanceStyle::Selection selection = AppearanceStyle::selectionStyle();
    // A light selection keeps the normal text colour when it stays readable.
    const QColor lightSelection = mixed(background, highlight, 0.2);
    const bool light = AppearanceStyle::isLightSelection(selection) && ColorSchemes::contrastRatio(text, lightSelection) >= 4.5;
    const QColor highlightedText = light ? text : palette.color(group, QPalette::HighlightedText);
    const QColor accent = ColorSchemes::readable(highlight, base);
    if (selected) {
        background = light ? lightSelection : highlight;
        border = light ? accent : highlight;
    } else if (hovered) {
        background = mixed(background, highlight, light ? 0.08 : 0.15);
        border = mixed(border, highlight, 0.6);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal radius = qMin<qreal>(AppearanceStyle::cornerRadius(6), qMin(card.width(), card.height()) / 4.0);
    const QRectF shape = QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(shape, radius, radius);
    painter->fillPath(path, background);
    if (selected && selection == AppearanceStyle::Selection::AccentBar && light) {
        painter->save();
        painter->setClipPath(path);
        constexpr int bar = 4;
        painter->fillRect(opt.direction == Qt::RightToLeft ? QRect(card.right() - bar + 1, card.top(), bar, card.height())
                                                           : QRect(card.left(), card.top(), bar, card.height()),
                          accent);
        painter->restore();
    }
    painter->setPen(QPen(border, selected && selection == AppearanceStyle::Selection::Outlined && light ? 2 : 1));
    painter->drawPath(path);
    if (focused) {
        // An inner ring keeps the current card visible with or without
        // selection; a dashed ring stays distinct from a light selection.
        QPen ring(selected && !light ? highlightedText : highlight, 2);
        if (selected && light) {
            ring.setStyle(Qt::DashLine);
        }
        painter->setPen(ring);
        const qreal inner = qMax<qreal>(0, radius - 2);
        painter->drawRoundedRect(shape.adjusted(2.5, 2.5, -2.5, -2.5), inner, inner);
    }
    painter->setRenderHint(QPainter::Antialiasing, false);

    const QIcon::Mode mode = !(opt.state & QStyle::State_Enabled) ? QIcon::Disabled : selected && !light ? QIcon::Selected : QIcon::Normal;
    opt.icon.paint(painter, previewRect(card), Qt::AlignCenter, mode);

    const QFontMetrics metrics(opt.font);
    const QRect name = nameRect(card, metrics);
    QColor nameColor = selected ? highlightedText : text;
    if (!selected) {
        if (const QVariant foreground = index.data(Qt::ForegroundRole); foreground.canConvert<QColor>() && foreground.value<QColor>().isValid()) {
            nameColor = foreground.value<QColor>();
        }
    }
    painter->setFont(opt.font);
    painter->setPen(nameColor);
    const QStringList lines = nameLines(opt.text, opt.font, name.width(), NameLines);
    for (int i = 0; i < lines.size(); ++i) {
        const QRect line(name.left(), name.top() + i * metrics.lineSpacing(), name.width(), metrics.lineSpacing());
        painter->drawText(line, Qt::AlignHCenter | Qt::AlignVCenter, lines.at(i));
    }

    const QFont small = informationFont(opt.font);
    const QFontMetrics smallMetrics(small);
    painter->setFont(small);
    const QColor secondary = selected ? highlightedText : mixed(base, text, 0.7);
    int top = name.bottom() + 1 + InformationGap;
    const QStringList information = shownInformation(index);
    for (int i = 0; i < information.size(); ++i) {
        const QString &value = information.at(i);
        QRect line(name.left(), top, name.width(), smallMetrics.lineSpacing());
        top += smallMetrics.lineSpacing();
        if (value.isEmpty()) {
            continue;
        }
        if (m_information.at(i) == QLatin1String("label")) {
            const QColor swatch = LabelColors::rgb(LabelColors::fromKey(index.data(CardRoles::Label).toString()));
            const int size = qMin(smallMetrics.height() - 4, 10);
            const int textWidth = qMin(smallMetrics.horizontalAdvance(value), line.width() - size - 4);
            const int left = line.left() + (line.width() - textWidth - size - 4) / 2;
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(selected ? highlightedText : Qt::NoPen);
            painter->setBrush(swatch);
            painter->drawEllipse(QRect(left, line.center().y() - size / 2, size, size));
            painter->setRenderHint(QPainter::Antialiasing, false);
            line = QRect(left + size + 4, line.top(), textWidth, line.height());
        }
        painter->setPen(secondary);
        painter->drawText(line, Qt::AlignHCenter | Qt::AlignVCenter, smallMetrics.elidedText(value, Qt::ElideMiddle, line.width()));
    }

    if (m_markersVisible && m_markersVisible()) {
        QStyleOptionButton check;
        check.rect = markerRect(card, opt.direction);
        check.state = QStyle::State_Enabled | (selected ? QStyle::State_On : QStyle::State_Off);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_CheckBox, &check, painter, opt.widget);
    }
    painter->restore();
}

QWidget *CardDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    auto *editor = new QLineEdit(parent);
    editor->setObjectName(QStringLiteral("cardNameEditor"));
    editor->setAlignment(Qt::AlignHCenter);
    editor->setAccessibleName(i18nc("@label:textbox", "New name"));
    return editor;
}

void CardDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *line = qobject_cast<QLineEdit *>(editor);
    if (!line) {
        return;
    }
    const KFileItem item = index.data(KDirModel::FileItemRole).value<KFileItem>();
    const QString name = item.isNull() ? index.data(Qt::DisplayRole).toString() : item.name();
    line->setText(name);
    // Select the name without its extension, as the Rename dialog does. The
    // view selects all text after setEditorData, so apply this afterwards.
    const QString suffix = item.isDir() ? QString() : QMimeDatabase().suffixForFileName(name);
    const int length = suffix.isEmpty() ? name.size() : name.size() - suffix.size() - 1;
    line->setSelection(0, qMax(0, length));
    QMetaObject::invokeMethod(
        line,
        [line, length] {
            line->setSelection(0, qMax(0, length));
        },
        Qt::QueuedConnection);
}

void CardDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto *line = qobject_cast<QLineEdit *>(editor);
    if (!line) {
        return;
    }
    const QString name = line->text();
    const KFileItem item = index.data(KDirModel::FileItemRole).value<KFileItem>();
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QLatin1String(".") || name == QLatin1String("..")
        || (!item.isNull() && name == item.name())) {
        return;
    }
    if (!item.isNull() && item.url().isLocalFile()) {
        const QString from = item.url().toLocalFile();
        LibraryMoveTracker::instance()->expectRename(from, QFileInfo(from).dir().filePath(name));
    }
    model->setData(index, name, Qt::EditRole);
}

void CardDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    const QRect card = cardRect(option.rect, m_space);
    const QRect name = nameRect(card, QFontMetrics(option.font));
    const int height = qMax(editor->sizeHint().height(), QFontMetrics(option.font).lineSpacing());
    editor->setGeometry(name.left() - 2, name.top(), name.width() + 4, height);
}
