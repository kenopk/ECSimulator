#include "items/NoteItem.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

#include "items/CircuitItem.h"
#include "theme/Theme.h"

namespace {
constexpr qreal kClose = 14.0;
}

NoteItem::NoteItem(CircuitItem *owner)
    : QGraphicsObject(owner), m_owner(owner)
{
    setFlag(ItemIsSelectable, true);       // двигает и скрывает CircuitView
    setZValue(48);                         // поверх элементов, под осциллограммами
    connect(&Theme::instance(), &Theme::changed, this, [this] { refresh(); });
    refresh();
}

QRectF NoteItem::closeRect() const
{
    return QRectF(m_size.width() - kClose - 3, 3, kClose, kClose);
}

bool NoteItem::closeHit(const QPointF &local) const
{
    return closeRect().adjusted(-2, -2, 2, 2).contains(local);
}

void NoteItem::refresh()
{
    const QString text = m_owner ? m_owner->note().text : QString();
    const QString footer = m_owner ? m_owner->noteFooter() : QString();
    m_text = text;
    m_footer = footer;

    const QFontMetricsF fm(Theme::uiFont(12));
    const QRectF textRect = fm.boundingRect(QRectF(0, 0, kMaxTextWidth, 10000), Qt::TextWordWrap | Qt::AlignLeft, m_text);
    qreal w = std::max<qreal>(textRect.width(), 60.0);
    qreal h = textRect.height();
    if (!m_footer.isEmpty()) {
        const QFontMetricsF ff(Theme::monoFont(10));
        w = std::max(w, ff.horizontalAdvance(m_footer));
        h += ff.height() + 4;
    }
    const QSizeF size(w + 2 * kPad + 6 + kClose, h + 2 * kPad - 2);
    if (size != m_size) {
        prepareGeometryChange();
        m_size = size;
    }
    update();
}

void NoteItem::moveTo(const QPointF &scenePos)
{
    prepareGeometryChange();
    setPos(scenePos);
}

QLineF NoteItem::linkLine() const
{
    if (!m_owner || !m_owner->scene())
        return QLineF();
    return QLineF(box().center(), m_owner->midpoint() - pos());
}

QRectF NoteItem::boundingRect() const
{
    QRectF r = box().adjusted(-3, -3, 3, 3);
    const QLineF link = linkLine();
    if (!link.isNull())
        r = r.united(QRectF(link.p1(), link.p2()).normalized().adjusted(-4, -4, 4, 4));
    return r;
}

QPainterPath NoteItem::shape() const
{
    QPainterPath path;
    path.addRect(box());
    return path;
}

void NoteItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const Palette &pal = Theme::instance().palette();
    p->setRenderHint(QPainter::Antialiasing);

    // линия привязки к проводу
    const QLineF link = linkLine();
    if (!link.isNull()) {
        p->setPen(QPen(pal.textDim, 1, Qt::DashLine));
        p->drawLine(link);
        p->setBrush(pal.textDim);
        p->setPen(Qt::NoPen);
        p->drawEllipse(link.p2(), 3, 3);
    }

    // карточка
    QColor fill = pal.panelAlt;
    fill.setAlpha(240);
    p->setPen(Qt::NoPen);
    p->setBrush(fill);
    p->drawRoundedRect(box(), 5, 5);
    p->setBrush(pal.accent);
    p->drawRoundedRect(QRectF(0, 0, 3.5, m_size.height()), 1.5, 1.5);

    // текст
    p->setFont(Theme::uiFont(12));
    p->setPen(pal.text);
    const QFontMetricsF fm(Theme::uiFont(12));
    const QRectF textRect = fm.boundingRect(QRectF(0, 0, kMaxTextWidth, 10000), Qt::TextWordWrap | Qt::AlignLeft, m_text);
    p->drawText(QRectF(kPad + 3, kPad - 1, textRect.width() + 2, textRect.height() + 2),
                Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_text);
    if (!m_footer.isEmpty()) {
        p->setFont(Theme::monoFont(10));
        p->setPen(pal.textDim);
        p->drawText(QRectF(kPad + 3, m_size.height() - kPad - 8, m_size.width() - kPad, 14),
                    Qt::AlignLeft | Qt::AlignVCenter, m_footer);
    }

    // «×»
    p->setPen(QPen(pal.textDim, 1.3, Qt::SolidLine, Qt::RoundCap));
    const QRectF c = closeRect().adjusted(4, 4, -4, -4);
    p->drawLine(c.topLeft(), c.bottomRight());
    p->drawLine(c.topRight(), c.bottomLeft());

    // рамка (при выделении — акцентом)
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(isSelected() ? pal.accent : pal.border, isSelected() ? 1.8 : 1));
    p->drawRoundedRect(box(), 5, 5);
}
