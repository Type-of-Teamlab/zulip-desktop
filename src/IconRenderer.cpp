#include "IconRenderer.h"

#include "ZulipApplication.h"

#include <QPixmapCache>
#include <QPainter>
#include <QSvgRenderer>
#include <QDebug>

IconRenderer::IconRenderer(const QString &svgPath, QObject *parent) :
    QObject(parent),
    m_svgPath(svgPath),
    m_renderer(new QSvgRenderer(svgPath, this))
{
    // NOTE: Experimental testing of the menu bar height:
    // CGFloat hgt = [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
    // and qsystemtrayicon_mac.mm (scale = hgt - 4) shows that we want 18px
    // square menu pixmaps for os x, so make sure to give our QIcon that size
    // 16px is the default size on Windows, and 22px is default on X11
    // 36px is in case Qt is smart enough to render for retina displays at double-size
    // (it's not).

    m_defaultSizes << 16 << 18 << 22 << 36;
    QPixmapCache::setCacheLimit(1000);

    QSvgRenderer r(QLatin1String(":images/usericon.svg"));
    foreach(int size, m_defaultSizes) {
        QImage image(QSize(size, size), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);

        r.render(&p);
        m_personIcon.addPixmap(QPixmap::fromImage(image));
    }
}

QIcon IconRenderer::icon(int unreadNormal, int unreadPMs) {
    // -1x-1 size is our magic key for icons
    const QString cKey = cacheKey(QSize(-1, -1), unreadNormal, unreadPMs);
    QIcon icn = m_iconCache.value(cKey, QIcon());
    if (!icn.isNull()) {
        return icn;
    }

    foreach(int size, m_defaultSizes) {
        const QPixmap pm = pixmap(QSize(size, size), unreadNormal, unreadPMs);
        icn.addPixmap(pm);
    }

    m_iconCache.insert(cKey, icn);

    return icn;
}

QIcon IconRenderer::winBadgeIcon(int unreadCount) {
    const QString cKey = QString("win_unread_%1").arg(unreadCount);

    QIcon icn = m_iconCache.value(cKey, QIcon());
    if (!icn.isNull()) {
        return icn;
    }

    QImage image(QSize(48, 48), QImage::Format_ARGB32_Premultiplied);

    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw a red background circle
    QGradient grad = QLinearGradient(24, 0, 24, 48);
    QColor color;
    color.setNamedColor("#F9A0A0");
    grad.setColorAt(0, color);
    color.setNamedColor("#F09A9A");
    grad.setColorAt(1, color);
    p.save();
    p.setBrush(grad);
    p.drawRoundedRect(image.rect(), 40, 40);
    p.restore();

    // Draw the foreground number
    QPen textPen(Qt::black, 1, Qt::SolidLine);
    p.setPen(textPen);
    QFont f = APP->font();
    f.setPixelSize(40);
    p.setFont(f);

    p.drawText(image.rect(), Qt::AlignCenter, QString::number(unreadCount));

    p.end();

    icn.addPixmap(QPixmap::fromImage(image));
    m_iconCache[cKey] = icn;
    return icn;
}


QPixmap IconRenderer::pixmap(const QSize &size, int unreadNormal, int unreadPMs) {
    const QString cKey = cacheKey(size, unreadNormal, unreadPMs);
    QPixmap pm;
    if (!QPixmapCache::find(cKey, &pm)) {
        pm = render(size, unreadNormal, unreadPMs);
        QPixmapCache::insert(cKey, pm);
    }

    return pm;
}

QIcon IconRenderer::personIcon() {
    return m_personIcon;
}

QString IconRenderer::cacheKey(const QSize &size, int unreadNormal, int unreadPMs) const {
    return QString("zulip_%1x%2_%3_%4").arg(size.width())
                                     .arg(size.height())
                                     .arg(unreadNormal)
                                     .arg(unreadPMs);
}

QPixmap IconRenderer::render(const QSize &size, int unreadNormal, int unreadPMs) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    QPen textPen(Qt::black, 1, Qt::SolidLine);
    p.setPen(textPen);
    QFont f = APP->font();
    f.setBold(false);

    m_renderer->render(&p);

    if (unreadNormal > 0) {
        const int padding = 1;

        QRectF textRect = image.rect();
        textRect.setSize(textRect.size() * 7 / 10);
        textRect.moveBottomRight(image.rect().bottomRight());

        // Add a half-transparent white layer
        QBrush countBackground(QColor(178, 225, 102, 255));
        p.save();
        p.setBrush(countBackground);
        p.setPen(Qt::NoPen);
        p.drawEllipse((QPointF)textRect.center(), textRect.width() / 2, textRect.width() / 2);
        p.setPen(Qt::PenStyle::SolidLine);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse((QPointF)textRect.center(), textRect.width() / 2, textRect.width() / 2);
        p.restore();

        // Unicode ∞ if greater than 99 unread msgs
        const QString disp = unreadNormal > 99 ? QChar(0x221e) : QString::number(unreadNormal);


        f.setPixelSize(textRect.height()*2/3);
        p.setFont(f);

        textRect.translate(1, 1);
        p.drawText(textRect, Qt::AlignCenter, disp);
    }

    p.end();

    return QPixmap::fromImage(image);
}
