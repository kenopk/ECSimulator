#ifndef WIRE_H
#define WIRE_H

#include <QGraphicsLineItem>
#include "pin.h"
#include <QStyleOptionGraphicsItem>
#include <QStyle>

class Wire : public QGraphicsLineItem {
public:
    Wire(Pin startPin, Pin endPin, QGraphicsItem *parent = nullptr);

    virtual void updateGeometry();

    void setStartConnected(bool connected);
    void setEndConnected(bool connected);
    void setStartPos(const QPoint &pos);
    void setEndPos(const QPoint &pos);
    bool isStartConnected() const { return m_startConnected; }
    bool isEndConnected() const { return m_endConnected; }

    QPointF startScenePos() const { return m_startPin.scenePos(); }
    QPointF endScenePos() const { return m_endPin.scenePos(); }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setHovered(bool hovered);
    void setEndpointHovered(bool start, bool hovered);
    bool isHovered() const { return m_hovered; }
    bool isStartHovered() const { return m_startHovered; }
    bool isEndHovered() const { return m_endHovered; }
    Pin getStartPin() const { return m_startPin; }
    Pin getEndPin() const { return m_endPin; }
    void setStartNearOther(bool near) { m_startNearOther = near; }
    void setEndNearOther(bool near)   { m_endNearOther = near; }
    bool isStartNearOther() const { return m_startNearOther; }
    bool isEndNearOther() const   { return m_endNearOther; }
    QPainterPath shape() const override;

private:
    Pin m_startPin;
    Pin m_endPin;
    bool m_startConnected = false;
    bool m_endConnected = false;
    bool m_hovered = false;
    bool m_startHovered = false;
    bool m_endHovered = false;
    bool m_startNearOther = false;
    bool m_endNearOther = false;

    // Кэш shape(): пересобирается только когда геометрия провода реально
    // меняется (см. updateGeometry()), а не при каждом hit-тесте/hover.
    mutable QPainterPath m_wireCachedShape;
    mutable bool m_wireShapeDirty = true;
};

#endif