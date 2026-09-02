#include "component.h"
#include "wire.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QStyleOptionGraphicsItem>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QGraphicsScene> // <-- ИСПРАВЛЕНИЕ 1: добавлен инклюд
#include <cmath>

namespace {

// Размеры корпуса резистора в единицах сцены.
constexpr qreal kBodyLength    = 20.0;  // длина прямоугольника
constexpr qreal kBodyHeight    = 8.0;   // высота прямоугольника
constexpr int kLabelFontSize   = 6;     // размер шрифта подписей

// Ширина зоны клика вокруг контактов и корпуса.
constexpr qreal kComponentHitWidth = 10.0;

// ---- Размеры неполярного конденсатора ----
constexpr qreal kCapPlateThickness = 2.5;   // ТОЛЩИНА пластины
constexpr qreal kCapPlateHeight    = 16.0;  // ДЛИНА пластины
constexpr qreal kCapGap            = 5.5;   // ЗАЗОР между пластинами


struct ResistorGeometry {
    bool valid = false;
    QPointF p1, p2;
    QPointF bodyA, bodyB;
    QPointF a1, a2, b1, b2;
    bool drawLeads = false;
};

ResistorGeometry makeResistorGeometry(const QLineF &axis)
{
    ResistorGeometry g;
    g.p1 = axis.p1();
    g.p2 = axis.p2();
    const qreal len = axis.length();
    if (len < 1e-4) return g;

    const QPointF d = g.p2 - g.p1;
    const QPointF u = d / len;
    const QPointF n(-u.y(), u.x());
    const QPointF center = (g.p1 + g.p2) / 2.0;
    const qreal halfBody = kBodyLength / 2.0;

    g.bodyA = center - u * halfBody;
    g.bodyB = center + u * halfBody;
    g.drawLeads = (len >= kBodyLength);

    const QPointF halfSide = n * (kBodyHeight / 2.0);
    g.a1 = g.bodyA + halfSide;
    g.b1 = g.bodyB + halfSide;
    g.b2 = g.bodyB - halfSide;
    g.a2 = g.bodyA - halfSide;
    g.valid = true;
    return g;
}

QPolygonF makeResistorPolygon(const ResistorGeometry &g)
{
    return QPolygonF() << g.a1 << g.b1 << g.b2 << g.a2;
}

// <-- ИСПРАВЛЕНИЕ 3: добавлена функция форматирования сопротивления
QString formatResistanceStr(double ohms)
{
    if (ohms >= 1000000.0) return QString::number(ohms / 1000000.0, 'f', 1) + " MΩ";
    if (ohms >= 1000.0)    return QString::number(ohms / 1000.0, 'f', 1) + " kΩ";
    return QString::number(ohms, 'f', 0) + " Ω";
}

QString formatCapacitanceShort(double farads)
{
    if (farads >= 1.0)  return QString::number(farads, 'g', 4) + " F";
    if (farads >= 1e-3) return QString::number(farads * 1e3, 'g', 4) + " mF";
    if (farads >= 1e-6) return QString::number(farads * 1e6, 'g', 4) + " µF";
    if (farads >= 1e-9) return QString::number(farads * 1e9, 'g', 4) + " nF";
    return QString::number(farads * 1e12, 'g', 4) + " pF";
}

// Общий хелпер для расчёта прямоугольника текста с учётом зеркалирования
QRectF labelRect(const ResistorGeometry &g, const QString &valueStr,
                 qreal bodyOffset, qreal gap, bool flip)
{
    if (!g.valid || valueStr.isEmpty()) return QRectF();
    const QPointF d = g.p2 - g.p1;
    const qreal length = std::hypot(d.x(), d.y());
    if (length < 0.001) return QRectF();

    QPointF n(-d.y() / length, d.x() / length);
    if (n.y() > 0) n = -n;   // базовая сторона — вверх
    if (flip)      n = -n;   // зеркало — вниз

    QFont font;
    font.setPixelSize(kLabelFontSize);
    const QFontMetrics fm(font);
    const qreal tw = fm.horizontalAdvance(valueStr);
    const qreal th = fm.height();
    const qreal dim = (qAbs(n.x()) > qAbs(n.y())) ? tw : th;

    const qreal offset = bodyOffset + dim / 2.0 + gap;
    const QPointF center = (g.p1 + g.p2) / 2.0;
    const QPointF pos = center + n * offset;
    return QRectF(pos.x() - tw / 2.0, pos.y() - th / 2.0, tw, th);
}

// <-- ИСПРАВЛЕНИЕ 4: короткие версии, использующие labelRect и параметр flip
QRectF capacitorTextRect(const ResistorGeometry &g, double farads, bool flip = false)
{
    return labelRect(g, formatCapacitanceShort(farads), kCapPlateHeight / 2.0, 4.0, flip);
}

QRectF resistorTextRect(const ResistorGeometry &g, double resistance, bool flip = false)
{
    return labelRect(g, formatResistanceStr(resistance), kBodyHeight / 4.0, 6.0, flip);
}

// <-- ИСПРАВЛЕНИЕ 2: корректная проверка пересечения с QPolygonF
bool rectHitsWire(const QGraphicsItem *self, const QRectF &sceneRect)
{
    const QGraphicsScene *sc = self->scene();
    if (!sc) return false;
    const QList<QGraphicsItem*> hits = sc->items(sceneRect, Qt::IntersectsItemShape);
    for (QGraphicsItem *it : hits) {
        if (it == self) continue;
        if (!dynamic_cast<Wire*>(it)) continue;

        QPainterPath mappedPath;
        mappedPath.addPolygon(it->mapFromScene(sceneRect));
        if (it->shape().intersects(mappedPath)) return true;
    }
    return false;
}

bool labelFlipped(const Component *self, const QRectF &upRect, const QRectF &downRect)
{
    if (!rectHitsWire(self, upRect)) return false;
    return !rectHitsWire(self, downRect);
}

} // namespace

// ---------- Конструктор ----------
Component::Component(Pin startPin, Pin endPin, ComponentType type, QGraphicsItem *parent)
    : Wire(startPin, endPin, parent), m_type(type)
{
    switch (type) {
    case ComponentType::Resistor:
        m_params["resistance"] = 1000.0;
        m_params["power"]      = 0.25;
        m_params["accuracy"]   = 5.0;
        break;
    case ComponentType::Non_Polar_Capacitor:
        m_params["capacitance"] = 10e-6;
        m_params["voltage"]     = 5.0;
        m_params["tolerance"]   = 10;
        m_params["esr"]         = 0.1;
        break;
    default: break;
    }
}

QVariant Component::param(const QString &key) const { return m_params.value(key); }

void Component::setParam(const QString &key, const QVariant &value) {
    if (m_params.value(key) != value) { m_params[key] = value; update(); }
}

double Component::resistance() const { return m_params.value("resistance", 1000.0).toDouble(); }
void Component::setResistance(double ohms) { setParam("resistance", ohms); }

double Component::capacitance() const { return m_params.value("capacitance", 10e-6).toDouble(); }
void Component::setCapacitance(double farads) { setParam("capacitance", farads); }

// ---------- Отрисовка (ОДНА КОПИЯ!) ----------
// <-- ИСПРАВЛЕНИЕ 5: удалена дублирующая функция paint()
void Component::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option); Q_UNUSED(widget);
    const QLineF axis = line();

    QPen pen;
    if (isHovered() || isSelected()) pen = QPen(QColor(0, 255, 255), 2);
    else                             pen = QPen(Qt::green, 2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);

    QFont f = painter->font();
    f.setPixelSize(kLabelFontSize);
    painter->setFont(f);

    if (m_type == ComponentType::Resistor) {
        const ResistorGeometry g = makeResistorGeometry(axis);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        if (!g.valid) { painter->drawLine(axis); return; }

        if (g.drawLeads) {
            painter->drawLine(g.p1, g.bodyA);
            painter->drawLine(g.p2, g.bodyB);
        }
        painter->drawPolygon(makeResistorPolygon(g));

        painter->setPen(Qt::NoPen);
        if (!isStartConnected()) painter->setBrush(isStartNearOther() ? Qt::red : Qt::white), painter->drawEllipse(g.p1, 2.0, 2.0);
        if (!isEndConnected())   painter->setBrush(isEndNearOther() ? Qt::red : Qt::white),   painter->drawEllipse(g.p2, 2.0, 2.0);

        const QString valueStr = formatResistanceStr(resistance());
        const QRectF upRect = resistorTextRect(g, resistance(), false);
        const QRectF dnRect = resistorTextRect(g, resistance(), true);
        painter->setPen(Qt::white);
        painter->drawText(labelFlipped(this, upRect, dnRect) ? dnRect : upRect, Qt::AlignCenter, valueStr);
        return;
    }

    if (m_type == ComponentType::Non_Polar_Capacitor) {
        const ResistorGeometry g = makeResistorGeometry(axis);
        painter->setBrush(Qt::NoBrush);
        if (!g.valid) { painter->setPen(pen); painter->drawLine(axis); return; }

        const QPointF d = g.p2 - g.p1;
        const qreal len = std::hypot(d.x(), d.y());
        const QPointF u = (len > 1e-6) ? (d / len) : QPointF(1, 0);
        const QPointF n(-u.y(), u.x());
        const QPointF center = (g.p1 + g.p2) / 2.0;
        const qreal plateHalf = kCapPlateHeight / 2.0;
        const qreal gapHalf   = kCapGap / 2.0;
        const QPointF leftPlate  = center - u * gapHalf;
        const QPointF rightPlate = center + u * gapHalf;

        painter->setPen(pen);
        painter->drawLine(g.p1, leftPlate);
        painter->drawLine(g.p2, rightPlate);

        QPen platePen = pen;
        platePen.setWidthF(kCapPlateThickness);
        platePen.setCapStyle(Qt::FlatCap);
        painter->setPen(platePen);
        painter->drawLine(leftPlate  - n * plateHalf, leftPlate  + n * plateHalf);
        painter->drawLine(rightPlate - n * plateHalf, rightPlate + n * plateHalf);

        painter->setPen(Qt::NoPen);
        if (!isStartConnected()) painter->setBrush(isStartNearOther() ? Qt::red : Qt::white), painter->drawEllipse(g.p1, 2.0, 2.0);
        if (!isEndConnected())   painter->setBrush(isEndNearOther() ? Qt::red : Qt::white),   painter->drawEllipse(g.p2, 2.0, 2.0);

        const QString valueStr = formatCapacitanceShort(capacitance());
        const QRectF upRect = capacitorTextRect(g, capacitance(), false);
        const QRectF dnRect = capacitorTextRect(g, capacitance(), true);
        painter->setPen(Qt::white);
        painter->drawText(labelFlipped(this, upRect, dnRect) ? dnRect : upRect, Qt::AlignCenter, valueStr);
        return;
    }

    painter->setPen(pen);
    painter->drawLine(axis);
    painter->setPen(Qt::NoPen);
    if (!isStartConnected()) painter->setBrush(isStartNearOther() ? Qt::red : Qt::white), painter->drawEllipse(axis.p1(), 2.0, 2.0);
    if (!isEndConnected())   painter->setBrush(isEndNearOther() ? Qt::red : Qt::white),   painter->drawEllipse(axis.p2(), 2.0, 2.0);
}

// <-- ИСПРАВЛЕНИЕ 6: возвращён boundingRect с поддержкой обеих сторон текста
QRectF Component::boundingRect() const
{
    const ResistorGeometry g = makeResistorGeometry(line());
    if (!g.valid) return Wire::boundingRect();

    QRectF rect = shape().boundingRect();

    if (m_type == ComponentType::Resistor) {
        rect = rect.united(resistorTextRect(g, resistance(), false))
        .united(resistorTextRect(g, resistance(), true));
    } else if (m_type == ComponentType::Non_Polar_Capacitor) {
        rect = rect.united(capacitorTextRect(g, capacitance(), false))
        .united(capacitorTextRect(g, capacitance(), true));
    }
    return rect.adjusted(-2.0, -2.0, 2.0, 2.0);
}

QPainterPath Component::shape() const
{
    if (m_type == ComponentType::Non_Polar_Capacitor) {
        const ResistorGeometry g = makeResistorGeometry(line());
        if (!g.valid) return Wire::shape();

        const QPointF d = g.p2 - g.p1;
        const qreal len = std::hypot(d.x(), d.y());
        const QPointF u = (len > 1e-6) ? (d / len) : QPointF(1, 0);
        const QPointF n(-u.y(), u.x());
        const QPointF center = (g.p1 + g.p2) / 2.0;
        const qreal plateHalf = kCapPlateHeight / 2.0;
        const qreal gapHalf   = kCapGap / 2.0;
        const QPointF leftPlate  = center - u * gapHalf;
        const QPointF rightPlate = center + u * gapHalf;

        QPainterPath path;
        path.moveTo(g.p1); path.lineTo(leftPlate);
        path.moveTo(g.p2); path.lineTo(rightPlate);
        path.moveTo(leftPlate  - n * plateHalf); path.lineTo(leftPlate  + n * plateHalf);
        path.moveTo(rightPlate - n * plateHalf); path.lineTo(rightPlate + n * plateHalf);

        QPainterPathStroker stroker;
        stroker.setWidth(2.0);
        stroker.setCapStyle(Qt::FlatCap);
        QPainterPath stroke = stroker.createStroke(path);

        QPainterPath gap;
        gap.addPolygon(QPolygonF() << (leftPlate  - n * plateHalf)
                                   << (leftPlate  + n * plateHalf)
                                   << (rightPlate + n * plateHalf)
                                   << (rightPlate - n * plateHalf));
        gap.closeSubpath();

        QPainterPath dots;
        dots.addEllipse(g.p1, 2.0, 2.0);
        dots.addEllipse(g.p2, 2.0, 2.0);

        return stroke.united(gap).united(dots);
    }

    if (m_type != ComponentType::Resistor) return Wire::shape();

    const ResistorGeometry g = makeResistorGeometry(line());
    if (!g.valid) return Wire::shape();

    constexpr qreal kPenWidth = 2.0;

    QPainterPath body;
    body.addPolygon(makeResistorPolygon(g));
    body.closeSubpath();

    QPainterPathStroker outlineStroker;
    outlineStroker.setWidth(kPenWidth);
    outlineStroker.setJoinStyle(Qt::MiterJoin);
    const QPainterPath outline = outlineStroker.createStroke(body);

    QPainterPath leads;
    if (g.drawLeads) {
        leads.moveTo(g.p1); leads.lineTo(g.bodyA);
        leads.moveTo(g.p2); leads.lineTo(g.bodyB);
    }
    QPainterPathStroker leadStroker;
    leadStroker.setWidth(kPenWidth);
    leadStroker.setCapStyle(Qt::RoundCap);
    const QPainterPath leadsStroke = leadStroker.createStroke(leads);

    QPainterPath dots;
    dots.addEllipse(g.p1, 2.0, 2.0);
    dots.addEllipse(g.p2, 2.0, 2.0);

    return body.united(outline).united(leadsStroke).united(dots);
}