#ifndef COMPONENT_H
#define COMPONENT_H

#include "wire.h"
#include <QVariantMap>

enum class ComponentType {
    Resistor,
    Non_Polar_Capacitor,
    Inductor,
    Diode,
    // ... добавляйте новые
};

class Component : public Wire
{

public:
    // Конструктор – принимает начальный и конечный пины, тип и родителя
    Component(Pin startPin, Pin endPin, ComponentType type, QGraphicsItem *parent = nullptr);
    QVariant param(const QString &key) const;
    void setParam(const QString &key, const QVariant &value);
    double resistance() const;
    double capacitance() const;
    void setResistance(double ohms);
    void setCapacitance(double farads);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,QWidget *widget) override;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    ComponentType componentType() const { return m_type; }
    // Ctrl+c ctrl+v
    QVariantMap params() const { return m_params; }
    void setParams(const QVariantMap &p) { m_params = p; update(); }
    void updateGeometry() override;

private:
    ComponentType m_type;
    QVariantMap m_params;   // храним все параметры компонента
    mutable QPainterPath m_cachedShape;
    mutable bool m_shapeDirty = true;
};

#endif // COMPONENT_H