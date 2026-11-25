// SPDX-License-Identifier: GPL-3.0-or-later

#include "startuppresentation.h"

#include "appearancestyle.h"
#include "appidentity.h"
#include "colorschemes.h"
#include "settings.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QBuffer>
#include <QEvent>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QtMath>

#include <algorithm>

static void initBrandingResources()
{
    Q_INIT_RESOURCE(branding);
}

namespace
{
// The logo's natural size; the card shows it at half size on wide windows.
constexpr QSize LogoSize(768, 160);
constexpr int PreferredLogoWidth = 384;

void setTextColor(QWidget *widget, const QColor &color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, color);
    palette.setColor(QPalette::Text, color);
    widget->setPalette(palette);
}

int cardMargin(const QStyle *style)
{
    return style->pixelMetric(QStyle::PM_LayoutLeftMargin) * 2;
}

void paintCard(QPainter &painter, const QRectF &rect, const Branding::PresentationColors &colors, bool highContrast, const QStyle *style)
{
    painter.setRenderHint(QPainter::Antialiasing);
    const int border = highContrast ? 2 : 1;
    QColor outline = colors.text;
    if (!highContrast) {
        outline.setAlphaF(0.2);
    }
    painter.setPen(QPen(outline, border));
    painter.setBrush(colors.background);
    const qreal inset = border / 2.0;
    const int radius = AppearanceStyle::cornerRadius(style->pixelMetric(QStyle::PM_LayoutLeftMargin));
    painter.drawRoundedRect(rect.adjusted(inset, inset, -inset, -inset), radius, radius);
}
}

StartupPresentation *StartupPresentation::presentIfEnabled(QWidget *window)
{
    if (!Settings::startupPresentation()) {
        return nullptr;
    }
    auto *presentation =
        new StartupPresentation(window, static_cast<Branding::LogoVariant>(int(Settings::logoVariant())), ColorSchemes::highContrastPreferred());
    // Start once the event loop runs, so the window appears first.
    QTimer::singleShot(0, presentation, &StartupPresentation::start);
    return presentation;
}

StartupPresentation::StartupPresentation(QWidget *window, Branding::LogoVariant variant, bool highContrast)
    : QFrame(window)
    , m_window(window)
    , m_colors(Branding::presentationColors(variant, window->palette(), highContrast))
    , m_highContrast(highContrast)
    , m_logo(new QLabel(this))
    , m_holdTimer(new QTimer(this))
{
    initBrandingResources();
    setObjectName(QStringLiteral("startupPresentation"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(false);
    hide();

    const QString version = KAboutData::applicationData().version();
    setAccessibleName(version.isEmpty() ? AppIdentity::displayName()
                                        : i18nc("@info accessible name: application name and version", "%1 %2", AppIdentity::displayName(), version));
    setAccessibleDescription(AppIdentity::summary());

    m_logo->setAlignment(Qt::AlignCenter);
    m_logo->setAccessibleName(AppIdentity::displayName());

    auto *versionLabel = new QLabel(version.isEmpty() ? QString() : i18nc("@info", "Version %1", version), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setVisible(!version.isEmpty());
    auto *summaryLabel = new QLabel(AppIdentity::summary(), this);
    summaryLabel->setAlignment(Qt::AlignCenter);
    summaryLabel->setWordWrap(true);
    for (QLabel *label : {versionLabel, summaryLabel}) {
        setTextColor(label, m_colors.text);
    }

    auto *layout = new QVBoxLayout(this);
    const int margin = cardMargin(style());
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->addWidget(m_logo);
    layout->addWidget(versionLabel);
    layout->addWidget(summaryLabel);

    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(Branding::holdDuration());
    connect(m_holdTimer, &QTimer::timeout, this, &StartupPresentation::dismiss);

    window->installEventFilter(this);
    qApp->installEventFilter(this);
    updateLogo();
}

Branding::PresentationColors StartupPresentation::colors() const
{
    return m_colors;
}

QPixmap StartupPresentation::logo() const
{
    return m_logo->pixmap();
}

void StartupPresentation::setHoldDuration(int milliseconds)
{
    m_holdTimer->setInterval(milliseconds);
}

void StartupPresentation::start()
{
    if (m_dismissing || !m_window) {
        return;
    }
    reposition();
    show();
    raise();
    m_holdTimer->start();
}

void StartupPresentation::dismiss()
{
    if (m_dismissing) {
        return;
    }
    m_dismissing = true;
    m_holdTimer->stop();
    qApp->removeEventFilter(this);

    const int duration = Branding::fadeDuration(style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, this));
    if (duration == 0 || !isVisible()) {
        hide();
        deleteLater();
        return;
    }
    auto *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    m_fade = new QVariantAnimation(this);
    m_fade->setStartValue(1.0);
    m_fade->setEndValue(0.0);
    m_fade->setDuration(duration);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fade, &QVariantAnimation::valueChanged, effect, [effect](const QVariant &value) {
        effect->setOpacity(value.toReal());
    });
    connect(m_fade, &QVariantAnimation::finished, this, [this] {
        hide();
        deleteLater();
    });
    m_fade->start();
}

bool StartupPresentation::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::Resize:
            updateLogo();
            reposition();
            break;
        case QEvent::Hide:
            dismiss();
            break;
        default:
            break;
        }
    }
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        // Observe only: the input still reaches the window.
        if (!m_dismissing) {
            dismiss();
        }
        break;
    default:
        break;
    }
    return false;
}

void StartupPresentation::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    paintCard(painter, rect(), m_colors, m_highContrast, style());
}

QPixmap StartupPresentation::renderLogo(const Branding::PresentationColors &colors, int logicalWidth, qreal devicePixelRatio)
{
    initBrandingResources();
    QFile file(QStringLiteral(":/branding/plasma-opus-logo.svg"));
    if (logicalWidth <= 0 || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svg = Branding::recolor(file.readAll(), colors.text, colors.accent);
    QBuffer buffer(&svg);
    QImageReader reader(&buffer, "svg");
    const QSize logical(logicalWidth, logicalWidth * LogoSize.height() / LogoSize.width());
    reader.setScaledSize(logical * devicePixelRatio);
    QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }
    image.setDevicePixelRatio(devicePixelRatio);
    return QPixmap::fromImage(image);
}

QPixmap StartupPresentation::preview(Branding::LogoVariant variant, const QPalette &palette, bool highContrast, int logicalWidth, qreal devicePixelRatio)
{
    const QStyle *style = QApplication::style();
    const int margin = cardMargin(style);
    const QPixmap logo = renderLogo(Branding::presentationColors(variant, palette, highContrast), logicalWidth - 2 * margin, devicePixelRatio);
    if (logo.isNull()) {
        return {};
    }
    const QSizeF logoSize = logo.deviceIndependentSize();
    const QSize card(logicalWidth, qCeil(logoSize.height()) + 2 * margin);
    QPixmap pixmap(card * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paintCard(painter, QRectF(QPointF(), card), Branding::presentationColors(variant, palette, highContrast), highContrast, style);
    painter.drawPixmap(QPointF(margin, margin), logo);
    return pixmap;
}

void StartupPresentation::updateLogo()
{
    if (!m_window) {
        return;
    }
    const QMargins margins = contentsMargins() + layout()->contentsMargins();
    const int available = m_window->width() - 2 * (margins.left() + margins.right());
    const int width = std::clamp(available, PreferredLogoWidth / 2, PreferredLogoWidth);
    if (!m_logo->pixmap().isNull() && qRound(m_logo->pixmap().deviceIndependentSize().width()) == width) {
        return;
    }
    const QPixmap logo = renderLogo(m_colors, width, m_window->devicePixelRatioF());
    if (!logo.isNull()) {
        m_logo->setPixmap(logo);
    }
}

void StartupPresentation::reposition()
{
    if (!m_window) {
        return;
    }
    adjustSize();
    QRect bounds(QPoint(), sizeHint().boundedTo(m_window->size()));
    bounds.moveCenter(m_window->rect().center());
    setGeometry(bounds);
}
