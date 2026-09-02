#include "Wire.h"
#include <QPen>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace {
constexpr qreal kDotRadius = 3.0;
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

    if (!m_startConnected) {
        if (m_startNearOther) {
            painter->setBrush(Qt::red);
            painter->drawEllipse(line().p1(), 2.0, 2.0);
        } else {
            painter->setBrush(Qt::white);
            painter->drawEllipse(line().p1(), 2.0, 2.0);
        }
    }

    if (!m_endConnected) {
        if (m_endNearOther) {
            painter->setBrush(Qt::red);
            painter->drawEllipse(line().p2(), 2.0, 2.0);
        } else {
            painter->setBrush(Qt::white);
            painter->drawEllipse(line().p2(), 2.0, 2.0);
        }
    }
}

QPainterPath Wire::shape() const {
    QPainterPath path;
    path.moveTo(line().p1());
    path.lineTo(line().p2());

    QPainterPathStroker stroker;
    stroker.setWidth(kHitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    return stroker.createStroke(path);
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