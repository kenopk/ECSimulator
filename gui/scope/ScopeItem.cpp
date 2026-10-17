#include "scope/ScopeItem.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QTimer>

#include "items/CircuitItem.h"
#include "scope/ScopePlot.h"
#include "theme/Theme.h"

ScopeItem::ScopeItem(ScopeSource *source, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_source(source)
{
    setFlag(ItemIsSelectable, true);       // двигает и удаляет CircuitView
    setZValue(50);                         // поверх элементов схемы

    if (m_source) {
        connect(m_source, &ScopeSource::changed, this, [this] { m_dirty = true; });
        connect(m_source, &ScopeSource::moved, this, &ScopeItem::linkChanged);
    }
    connect(&Theme::instance(), &Theme::changed, this, [this] { update(); });

    // Перерисовка не чаще ~30 раз в секунду.
    m_timer = new QTimer(this);
    m_timer->setInterval(33);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_dirty) {
            m_dirty = false;
            update();
        }
    });
    m_timer->start();
}

QRectF ScopeItem::closeRect() const
{
    return QRectF(kWidth - kTitle, 0, kTitle, kTitle);
}

bool ScopeItem::closeHit(const QPointF &local) const
{
    return closeRect().contains(local);
}

bool ScopeItem::gearHit(const QPointF &local) const
{
    return ScopePlot::gearRect(plotRect()).contains(local);
}

QLineF ScopeItem::linkLine() const
{
    if (!m_source || !m_source->target() || !m_source->target()->scene())
        return QLineF();
    CircuitItem *t = m_source->target();
    const QPointF center = QPointF(t->p1() + t->p2()) / 2.0;
    return QLineF(box().center(), mapFromScene(center));
}

void ScopeItem::linkChanged()
{
    prepareGeometryChange();
    update();
}

QRectF ScopeItem::boundingRect() const
{
    QRectF r = box().adjusted(-3, -3, 3, 3);
    const QLineF link = linkLine();
    if (!link.isNull())
        r = r.united(QRectF(link.p1(), link.p2()).normalized().adjusted(-3, -3, 3, 3));
    return r;
}

QPainterPath ScopeItem::shape() const
{
    QPainterPath path;
    path.addRect(box());
    return path;
}

void ScopeItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const Palette &pal = Theme::instance().palette();
    p->setRenderHint(QPainter::Antialiasing);

    // линия связи с элементом
    const QLineF link = linkLine();
    if (!link.isNull()) {
        p->setPen(QPen(pal.textDim, 1, Qt::DashLine));
        p->drawLine(link);
        const QPointF end = link.p2();
        p->setBrush(pal.textDim);
        p->setPen(Qt::NoPen);
        p->drawEllipse(end, 3, 3);
    }

    // заголовок
    const QRectF titleRect(0, 0, kWidth, kTitle);
    p->setPen(Qt::NoPen);
    p->setBrush(pal.panelAlt);
    p->drawRect(titleRect);

    QFont f = p->font();
    f.setPixelSize(12);
    f.setBold(true);
    p->setFont(f);
    p->setPen(pal.text);
    const QString title = m_source ? m_source->title() : QString();
    p->drawText(titleRect.adjusted(8, 0, -kTitle, 0), Qt::AlignVCenter | Qt::AlignLeft,
                QStringLiteral("Осциллограмма: ") + title);

    // «×»
    p->setPen(QPen(pal.textDim, 1.5, Qt::SolidLine, Qt::RoundCap));
    const QRectF c = closeRect().adjusted(6, 6, -6, -6);
    p->drawLine(c.topLeft(), c.bottomRight());
    p->drawLine(c.topRight(), c.bottomLeft());

    // график
    if (m_source)
        ScopePlot::paint(p, plotRect(), m_source, ScopePlot::Options());

    // рамка (при выделении — акцентом)
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(isSelected() ? pal.accent : pal.border, isSelected() ? 2 : 1));
    p->drawRect(box());
}
