#include "Wire.h"
#include <QPen>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace {
constexpr qreal kDotRadius = 3.0;      // запас в boundingRect() под точку конца + сглаживание
constexpr qreal kEndpointVisualRadius = 2.0;  // реальный радиус отрисовки точки конца
constexpr qreal kHitWidth = 3;
}

Wire::Wire(Pin startPin, Pin endPin, QGraphicsItem *parent)
    : QGraphicsLineItem(parent), m_startPin(startPin), m_endPin(endPin)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    QPen pen(Qt::green, 2);
    pen.setCapStyle(Qt::RoundCap);
    setPen(pen);
    updateGeometry();
}

void Wire::updateGeometry() {
    prepareGeometryChange();
    setLine(QLineF(m_startPin.scenePos(), m_endPin.scenePos()));
    m_wireShapeDirty = true;   // геометрия поменялась — старый shape() больше не верен
}

void Wire::setStartConnected(bool connected) {
    if (m_startConnected != connected) {
        m_startConnected = connected;
        update();
    }
}

void Wire::setEndConnected(bool connected) {
    if (m_endConnected != connected) {
        m_endConnected = connected;
        update();
    }
}

// <-- изменено: принимаем QPoint, но внутри храним в pin как QPoint
void Wire::setStartPos(const QPoint &pos) {
    m_startPin.owner = nullptr;
    m_startPin.standalonePos = pos;
    updateGeometry();
}

void Wire::setEndPos(const QPoint &pos) {
    m_endPin.owner = nullptr;
    m_endPin.standalonePos = pos;
    updateGeometry();
}

QRectF Wire::boundingRect() const {
    QRectF base = QGraphicsLineItem::boundingRect();
    qreal margin = kHitWidth / 2 + kDotRadius + 2;
    return base.adjusted(-margin, -margin, margin, margin);
}

void Wire::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    QPen pen;
    if (m_hovered || isSelected()) {
        pen = QPen(QColor(0, 255, 255), 2);
    } else {
        pen = QPen(Qt::green, 2);
    }
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->drawLine(line());

    painter->setPen(Qt::NoPen);

    // Общий хелпер вместо дублированных if/else для start- и end-точки:
    // рисует опциональное кольцо подсветки (если именно этот конец наведён
    // мышью — см. setEndpointHovered()), а поверх — саму точку конца.
    auto drawEndpoint = [&](const QPointF &p, bool nearOther, bool hovered) {
        if (hovered) {
            painter->setBrush(QColor(0, 255, 255, 120));
            painter->drawEllipse(p, kEndpointVisualRadius + 2.0, kEndpointVisualRadius + 2.0);
        }
        painter->setBrush(nearOther ? Qt::red : Qt::white);
        painter->drawEllipse(p, kEndpointVisualRadius, kEndpointVisualRadius);
    };

    if (!m_startConnected) drawEndpoint(line().p1(), m_startNearOther, m_startHovered);
    if (!m_endConnected)   drawEndpoint(line().p2(), m_endNearOther, m_endHovered);
}

QPainterPath Wire::shape() const {
    if (!m_wireShapeDirty) return m_wireCachedShape;

    QPainterPath path;
    path.moveTo(line().p1());
    path.lineTo(line().p2());

    QPainterPathStroker stroker;
    stroker.setWidth(kHitWidth);
    stroker.setCapStyle(Qt::RoundCap);

    m_wireCachedShape = stroker.createStroke(path);
    m_wireShapeDirty = false;
    return m_wireCachedShape;
}

void Wire::setHovered(bool hovered) {
    if (m_hovered != hovered) {
        m_hovered = hovered;
        update();
    }
}

void Wire::setEndpointHovered(bool start, bool hovered) {
    if (start) {
        if (m_startHovered != hovered) {
            m_startHovered = hovered;
            update();
        }
    } else {
        if (m_endHovered != hovered) {
            m_endHovered = hovered;
            update();
        }
    }
}