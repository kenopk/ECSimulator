#include "items/SymbolPainter.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPolygonF>
#include <QString>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <map>

namespace SymbolPainter {

namespace {

constexpr qreal kPi = 3.14159265358979323846;
using P = QPointF;

// ------------------------------------------------------------------ Spec

Spec makeSpec(qreal body, int minGrid, int pins, bool axisOnly, int step, qreal halfH)
{
    Spec s;
    s.body = body;
    s.minGrid = minGrid;
    s.pinCount = pins;
    s.axisOnly = axisOnly;
    s.stepGrid = step;
    s.halfHeight = halfH;
    return s;
}

void setPin(Spec &s, int i, qreal u, qreal v)
{
    s.pin[i].u = u;
    s.pin[i].v = v;
}

Spec twoPin(qreal body, int minGrid, qreal halfH)
{
    Spec s = makeSpec(body, minGrid, 2, false, 1, halfH);
    setPin(s, 0, 0, 0);
    setPin(s, 1, 1, 0);
    return s;
}

Spec threePin(qreal body, int minGrid, qreal halfH, int step,
              qreal u0, qreal v0, qreal u1, qreal v1, qreal u2, qreal v2)
{
    Spec s = makeSpec(body, minGrid, 3, true, step, halfH);
    setPin(s, 0, u0, v0);
    setPin(s, 1, u1, v1);
    setPin(s, 2, u2, v2);
    return s;
}

bool isSinglePin(ElementType t)
{
    return elementInfo(t).pinCount() == 1;
}

// Земля смотрит вниз, остальные одновыводные (шины, антенна, метки) — вверх.
QPoint singlePinDirection(ElementType t)
{
    return (t == ElementType::Ground || t == ElementType::Chassis) ? QPoint(0, 1) : QPoint(0, -1);
}

QPoint dominantAxis(const QPoint &a, const QPoint &b, const QPoint &fallback)
{
    const int dx = b.x() - a.x();
    const int dy = b.y() - a.y();
    if (dx == 0 && dy == 0)
        return fallback;
    if (std::abs(dx) >= std::abs(dy))
        return QPoint(dx < 0 ? -1 : 1, 0);
    return QPoint(0, dy < 0 ? -1 : 1);
}

// Провод, линия и рамка: произвольные концы, без ограничений по длине и направлению.
bool isFreeShape(ElementType t)
{
    return t == ElementType::Wire || t == ElementType::Line || t == ElementType::Box;
}

bool isGateType(ElementType t)
{
    using T = ElementType;
    return t == T::GateAnd || t == T::GateOr || t == T::GateNand || t == T::GateNor
        || t == T::GateXor || t == T::GateXnor;
}

int gateInputs(unsigned variant)
{
    return variant ? std::clamp(int(variant), 2, 63) : 2;
}

// Вентиль с n входами: входы через 2 клетки, выход посередине.
Spec gateSpec(unsigned variant)
{
    const int n = gateInputs(variant);
    Spec s = makeSpec(40, 4, n + 1, true, 1, (n - 1) * 16.0 + 10.0);
    for (int i = 0; i < n; ++i)
        setPin(s, i, 0, -(n - 1) + 2.0 * i);
    setPin(s, n, 1, 0);
    return s;
}

// ------------------------------------------------------------ рисование

using Ctx = PaintContext;

QPen penFor(const Ctx &c, const QColor &col, qreal mult = 1.0)
{
    return QPen(col, c.width * mult, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void setBody(const Ctx &c, qreal mult = 1.0)
{
    c.p->setPen(penFor(c, c.body, mult));
    c.p->setBrush(Qt::NoBrush);
}

void lead(const Ctx &c, int pin, const P &a, const P &b)
{
    c.p->setPen(penFor(c, c.pin[pin]));
    c.p->setBrush(Qt::NoBrush);
    c.p->drawLine(a, b);
}

void leadPoly(const Ctx &c, int pin, std::initializer_list<P> pts)
{
    QPolygonF poly;
    for (const P &pt : pts)
        poly << pt;
    c.p->setPen(penFor(c, c.pin[pin]));
    c.p->setBrush(Qt::NoBrush);
    c.p->drawPolyline(poly);
}

void line(const Ctx &c, const P &a, const P &b)
{
    setBody(c);
    c.p->drawLine(a, b);
}

P unit(const P &v)
{
    const qreal l = std::hypot(v.x(), v.y());
    return l > 0 ? P(v.x() / l, v.y() / l) : P(1, 0);
}

// Залитый наконечник стрелки: остриё в tip, направление dir (единичный вектор).
void arrowHead(const Ctx &c, const P &tip, const P &dir, qreal size)
{
    const P n(-dir.y(), dir.x());
    QPolygonF poly;
    poly << tip << (tip - dir * size + n * (size * 0.42)) << (tip - dir * size - n * (size * 0.42));
    c.p->save();
    c.p->setPen(QPen(c.body, c.width * 0.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    c.p->setBrush(c.body);
    c.p->drawPolygon(poly);
    c.p->restore();
}

// Знаки «+» и «−» всегда горизонтальные/вертикальные на экране, как и надписи:
// при повороте элемента они не должны превращаться в «I».
void signGlyph(const Ctx &c, const P &at, qreal h, bool plus)
{
    QPainter *p = c.p;
    p->save();
    p->translate(at);
    p->scale(1.0, c.flip ? -1.0 : 1.0);
    p->rotate(-c.angle);
    p->setPen(penFor(c, c.body, 0.7));
    p->setBrush(Qt::NoBrush);
    p->drawLine(P(-h, 0), P(h, 0));
    if (plus)
        p->drawLine(P(0, -h), P(0, h));
    p->restore();
}

void plusSign(const Ctx &c, const P &at, qreal h)  { signGlyph(c, at, h, true); }
void minusSign(const Ctx &c, const P &at, qreal h) { signGlyph(c, at, h, false); }

// Надпись, которая остаётся горизонтальной при любом повороте/отражении элемента.
void text(const Ctx &c, const P &pos, const QString &s, qreal px, bool bold = true)
{
    QPainter *p = c.p;
    p->save();
    p->translate(pos);
    p->scale(1.0, c.flip ? -1.0 : 1.0);
    p->rotate(-c.angle);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(std::max(1, int(px * c.textScale)));
    f.setBold(bold);
    p->setFont(f);
    p->setPen(c.body);
    p->drawText(QRectF(-40, -20, 80, 40), Qt::AlignCenter, s);
    p->restore();
}

struct Span { qreal x0, x1, cx; };

Span spanOf(const Ctx &c, qreal body)
{
    const qreal cx = c.length / 2.0;
    return { cx - body / 2.0, cx + body / 2.0, cx };
}

void twoLeads(const Ctx &c, const Span &s)
{
    lead(c, 0, P(0, 0), P(s.x0, 0));
    lead(c, 1, P(s.x1, 0), P(c.length, 0));
}

// Стрелка регулирования по ГОСТ 2.721 (наклон 45°, пересекает символ).
void regulationArrow(const Ctx &c, qreal cx)
{
    line(c, P(cx - 13, 13), P(cx + 11, -11));
    arrowHead(c, P(cx + 14, -14), P(0.7071, -0.7071), 7.5);
}

// ---------------------------------------------------------------- земля

// Земля и корпус растягиваются как любой элемент: знак стоит на конце, а между
// выводом и знаком удлиняется провод.
void paintGround(const Ctx &c)
{
    const qreal L = c.length;
    lead(c, 0, P(0, 0), P(L - 12, 0));
    line(c, P(L - 12, -10),  P(L - 12, 10));
    line(c, P(L - 7,  -6.5), P(L - 7,  6.5));
    line(c, P(L - 2,  -3),   P(L - 2,  3));
}

void paintChassis(const Ctx &c)
{
    const qreal x = c.length - 9;
    lead(c, 0, P(0, 0), P(x, 0));
    line(c, P(x, -10), P(x, 10));
    for (qreal y : { -10.0, -3.0, 4.0 })
        line(c, P(x, y), P(x + 7, y + 7));
}

// ------------------------------------------------------------- пассивные

void paintResistor(const Ctx &c, bool variable, bool wiper)
{
    const Span s = spanOf(c, 40);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -8, 40, 16));
    if (variable)
        regulationArrow(c, s.cx);
    if (wiper) {
        lead(c, 2, P(s.cx, -16), P(s.cx, -9));
        arrowHead(c, P(s.cx, -8), P(0, 1), 6.5);
    }
}

void paintCapacitor(const Ctx &c, bool variable)
{
    const Span s = spanOf(c, 6);
    twoLeads(c, s);
    line(c, P(s.x0, -11), P(s.x0, 11));
    line(c, P(s.x1, -11), P(s.x1, 11));
    if (variable)
        regulationArrow(c, s.cx);
}

void paintPolarCapacitor(const Ctx &c)
{
    const Span s = spanOf(c, 10);
    twoLeads(c, s);
    line(c, P(s.x0, -11), P(s.x0, 11));                 // положительная обкладка
    setBody(c);
    c.p->drawRect(QRectF(s.x0 + 6, -11, 4, 22));        // отрицательная обкладка (оксидный слой)
    plusSign(c, P(s.x0 - 6, -10), 3);
}

void paintInductor(const Ctx &c, bool core, bool variable = false)
{
    const Span s = spanOf(c, 48);
    twoLeads(c, s);
    setBody(c);
    for (int i = 0; i < 4; ++i)
        c.p->drawArc(QRectF(s.x0 + i * 12.0, -6, 12, 12), 0, 180 * 16);
    if (core) {                                          // магнитопровод
        c.p->setPen(penFor(c, c.body, 1.7));
        c.p->drawLine(P(s.x0, -10), P(s.x1, -10));
    }
    if (variable)
        regulationArrow(c, s.cx);
}

// mode: 0 — трансформатор с магнитопроводом, 1 — со средней точкой вторичной обмотки,
//       2 — две связанные катушки без магнитопровода.
void paintTransformer(const Ctx &c, int mode = 0)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal xp = cx - 12.0;
    const qreal xs = cx + 12.0;

    leadPoly(c, 0, { P(0, -32), P(xp, -32), P(xp, -24) });
    leadPoly(c, 1, { P(0, 32),  P(xp, 32),  P(xp, 24)  });
    leadPoly(c, 2, { P(L, -32), P(xs, -32), P(xs, -24) });
    if (mode == 1) {
        leadPoly(c, 3, { P(L, 0),  P(xs, 0) });                       // средняя точка
        leadPoly(c, 4, { P(L, 32), P(xs, 32), P(xs, 24) });
    } else {
        leadPoly(c, 3, { P(L, 32), P(xs, 32), P(xs, 24) });
    }

    setBody(c);
    for (int i = 0; i < 3; ++i)
        c.p->drawArc(QRectF(xp - 8, -24.0 + 16.0 * i, 16, 16), 90 * 16, 180 * 16);   // выпуклость влево
    if (mode == 1) {
        for (int i = 0; i < 4; ++i)
            c.p->drawArc(QRectF(xs - 6, -24.0 + 12.0 * i, 12, 12), 270 * 16, 180 * 16);
    } else {
        for (int i = 0; i < 3; ++i)
            c.p->drawArc(QRectF(xs - 8, -24.0 + 16.0 * i, 16, 16), 270 * 16, 180 * 16);  // выпуклость вправо
    }
    if (mode != 2) {
        line(c, P(cx - 3, -24), P(cx - 3, 24));                       // магнитопровод
        line(c, P(cx + 3, -24), P(cx + 3, 24));
    }
}

void paintFuse(const Ctx &c)
{
    const Span s = spanOf(c, 40);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -6, 40, 12));
    c.p->drawLine(P(s.x0, 0), P(s.x1, 0));
}

void paintLamp(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 12.0, 12.0);
    const qreal d = 12.0 * 0.70711;
    c.p->drawLine(P(s.cx - d, -d), P(s.cx + d, d));
    c.p->drawLine(P(s.cx - d, d),  P(s.cx + d, -d));
}

// -------------------------------------------------------------- источники

void paintVoltageDC(const Ctx &c)
{
    const Span s = spanOf(c, 32);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 16.0, 16.0);
    c.p->drawLine(P(s.x0, 0), P(s.x1, 0));               // ГОСТ 2.721: линия вдоль выводов
    plusSign(c, P(s.x0 - 7, -9), 3);
    minusSign(c, P(s.x1 + 7, -9), 3);
}

void paintBattery(const Ctx &c)
{
    const Span s = spanOf(c, 8);
    twoLeads(c, s);
    line(c, P(s.x0, -12), P(s.x0, 12));                  // длинная — «+»
    c.p->setPen(penFor(c, c.body, 1.9));
    c.p->drawLine(P(s.x1, -7), P(s.x1, 7));              // короткая — «−»
    plusSign(c, P(s.x0 - 6, -11), 3);
}

void paintWaveSource(const Ctx &c, int wave)
{
    const Span s = spanOf(c, 32);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 16.0, 16.0);

    QPolygonF w;
    const qreal x = s.cx;
    switch (wave) {
    case 1:     // прямоугольник
        w << P(x - 9, 0) << P(x - 9, -6) << P(x, -6) << P(x, 6) << P(x + 9, 6) << P(x + 9, 0);
        break;
    case 2:     // треугольник
        w << P(x - 9, 0) << P(x - 4.5, -6) << P(x + 4.5, 6) << P(x + 9, 0);
        break;
    case 3:     // пила
        w << P(x - 9, 6) << P(x, -6) << P(x, 6) << P(x + 9, -6);
        break;
    default:    // синус
        for (int t = -10; t <= 10; ++t)
            w << P(x + t * 0.9, -6.0 * std::sin(t * kPi / 10.0));
        break;
    }
    c.p->drawPolyline(w);
}

void paintCurrentSource(const Ctx &c)
{
    const Span s = spanOf(c, 32);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 16.0, 16.0);
    c.p->drawLine(P(s.cx, -16), P(s.cx, 16));            // ГОСТ 2.721: линия поперёк выводов
    // направление тока: от вывода A к выводу B
    line(c, P(s.cx - 9, -23), P(s.cx + 6, -23));
    arrowHead(c, P(s.cx + 11, -23), P(1, 0), 6);
}

// ---------------------------------------------------------- полупроводники

void diodeBody(const Ctx &c, const Span &s)
{
    twoLeads(c, s);
    setBody(c);
    QPolygonF tri;
    tri << P(s.x0, -9) << P(s.x0, 9) << P(s.x1, 0);
    c.p->drawPolygon(tri);
    c.p->drawLine(P(s.x1, -9), P(s.x1, 9));
}

void paintDiode(const Ctx &c)
{
    diodeBody(c, spanOf(c, 16));
}

void paintZener(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    c.p->drawLine(P(s.x1, -9), P(s.x1 - 4, -9));         // «Z»-образные загибы катода
    c.p->drawLine(P(s.x1, 9),  P(s.x1 + 4, 9));
}

void paintSchottky(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    QPolygonF top, bottom;
    top    << P(s.x1 - 4, -6) << P(s.x1 - 4, -9) << P(s.x1, -9);
    bottom << P(s.x1, 9) << P(s.x1 + 4, 9) << P(s.x1 + 4, 6);
    c.p->drawPolyline(top);
    c.p->drawPolyline(bottom);
}

void paintLed(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    const P d(0.7071, -0.7071);
    const P a1(s.cx - 5, -11);
    const P a2(s.cx + 1, -14);
    line(c, a1, a1 + d * 7.0);
    arrowHead(c, a1 + d * 11.0, d, 5);
    line(c, a2, a2 + d * 7.0);
    arrowHead(c, a2 + d * 11.0, d, 5);
}

void paintBjt(const Ctx &c, bool pnp)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal xb = cx - 8.0;
    const qreal xe = cx + 12.0;                          // где выводы выходят из корпуса (круг r=20)
    const qreal eS = pnp ? -1.0 : 1.0;                   // сторона эмиттера: вниз (n-p-n) / вверх (p-n-p)

    lead(c, 0, P(0, 0), P(xb, 0));
    lead(c, 1, P(L, -eS * 16), P(xe, -eS * 16));         // коллектор
    lead(c, 2, P(L,  eS * 16), P(xe,  eS * 16));         // эмиттер

    setBody(c);
    c.p->drawEllipse(P(cx, 0), 20.0, 20.0);              // корпус
    c.p->drawLine(P(xb, -10), P(xb, 10));                // база
    c.p->drawLine(P(xb, -eS * 4), P(xe, -eS * 16));
    const P a(xb, eS * 4), b(xe, eS * 16);
    c.p->drawLine(a, b);

    const P d = unit(b - a);
    if (pnp)
        arrowHead(c, a + d * 3.0, -d, 7.5);              // к базе
    else
        arrowHead(c, b - d * 2.0, d, 7.5);               // от базы
}

void paintMosfet(const Ctx &c, bool nmos)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal xg = cx - 9.0;
    const qreal xc = cx - 3.0;
    const qreal xs = cx + 8.0;
    const qreal sS = nmos ? 1.0 : -1.0;                  // сторона истока
    const qreal dS = -sS;                                // сторона стока

    lead(c, 0, P(0, 0), P(xg, 0));
    lead(c, 1, P(L, dS * 16), P(xs, dS * 16));
    lead(c, 2, P(L, sS * 16), P(xs, sS * 16));

    setBody(c);
    c.p->drawEllipse(P(cx, 0), 20.0, 20.0);
    c.p->drawLine(P(xg, -9), P(xg, 9));                  // затвор
    c.p->drawLine(P(xc, -10), P(xc, -6));                // индуцированный канал
    c.p->drawLine(P(xc, -3),  P(xc, 3));
    c.p->drawLine(P(xc, 6),   P(xc, 10));
    c.p->drawLine(P(xs, dS * 16), P(xs, dS * 8));        // сток
    c.p->drawLine(P(xs, dS * 8),  P(xc, dS * 8));
    c.p->drawLine(P(xs, sS * 16), P(xs, 0));             // исток, соединённый с подложкой
    c.p->drawLine(P(xs, sS * 8),  P(xc, sS * 8));
    c.p->drawLine(P(xs, 0),       P(xc, 0));

    if (nmos)
        arrowHead(c, P(xc + 1, 0), P(-1, 0), 5.5);       // подложка -> канал
    else
        arrowHead(c, P(xs - 2, 0), P(1, 0), 5.5);
}

void paintJfet(const Ctx &c, bool nChannel)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal xc = cx - 2.0;
    const qreal xs = cx + 8.0;
    const qreal xgl = cx - 14.0;
    const qreal sS = nChannel ? 1.0 : -1.0;
    const qreal dS = -sS;

    lead(c, 0, P(0, 0), P(xgl, 0));
    lead(c, 1, P(L, dS * 16), P(xs, dS * 16));
    lead(c, 2, P(L, sS * 16), P(xs, sS * 16));

    setBody(c);
    c.p->drawEllipse(P(cx, 0), 20.0, 20.0);
    c.p->drawLine(P(xgl, 0), P(xc, 0));                  // затвор
    c.p->drawLine(P(xc, -11), P(xc, 11));                // канал
    c.p->drawLine(P(xc, dS * 9), P(xs, dS * 9));
    c.p->drawLine(P(xs, dS * 9), P(xs, dS * 16));
    c.p->drawLine(P(xc, sS * 9), P(xs, sS * 9));
    c.p->drawLine(P(xs, sS * 9), P(xs, sS * 16));

    if (nChannel)
        arrowHead(c, P(xc - 1, 0), P(1, 0), 6);          // к каналу
    else
        arrowHead(c, P(xc - 7, 0), P(-1, 0), 6);         // от канала
}

void paintThyristor(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    leadPoly(c, 2, { P(c.length, 32), P(s.x1, 32), P(s.x1, 9) });
}

// -------------------------------------------------------------- коммутация

void lever(const Ctx &c, const Span &s, bool closed)
{
    constexpr qreal r = 2.2;
    setBody(c);
    const P pivot(s.x0, 0);
    const P tip = closed ? P(s.x1, 0)
                         : P(s.x0 + 22.0 * std::cos(25.0 * kPi / 180.0),
                             -22.0 * std::sin(25.0 * kPi / 180.0));
    c.p->drawLine(pivot, tip);
    c.p->drawEllipse(pivot, r, r);
    c.p->drawEllipse(P(s.x1, 0), r, r);
}

void paintSwitch(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    twoLeads(c, s);
    lever(c, s, c.state != 0);
}

void paintPushButton(const Ctx &c, bool normallyClosed = false)
{
    const Span s = spanOf(c, 24);
    twoLeads(c, s);
    const bool closed = normallyClosed ? (c.state == 0) : (c.state != 0);
    lever(c, s, closed);

    const qreal leverY = closed ? 0.0 : -(s.cx - s.x0) * std::tan(25.0 * kPi / 180.0);
    c.p->setPen(QPen(c.body, c.width * 0.8, Qt::DashLine, Qt::FlatCap));
    c.p->drawLine(P(s.cx, leverY - 1.5), P(s.cx, -18));      // механическая связь
    line(c, P(s.cx - 6, -18), P(s.cx + 6, -18));             // толкатель (кнопка)
}

void paintSpdt(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    lead(c, 0, P(0, 0), P(s.x0, 0));
    lead(c, 1, P(c.length, -16), P(s.x1, -16));
    lead(c, 2, P(c.length, 16),  P(s.x1, 16));

    constexpr qreal r = 2.2;
    setBody(c);
    const P pivot(s.x0, 0);
    const P target(s.x1, c.state == 0 ? -16.0 : 16.0);
    c.p->drawLine(pivot, target);
    c.p->drawEllipse(pivot, r, r);
    c.p->drawEllipse(P(s.x1, -16), r, r);
    c.p->drawEllipse(P(s.x1, 16), r, r);
}

// --------------------------------------------------------------- усилители

void paintOpAmp(const Ctx &c, bool power = false, const QString &glyph = QStringLiteral("∞"))
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal x0 = cx - 24.0;
    const qreal x1 = cx + 24.0;

    lead(c, 0, P(0, -16), P(x0 - 7, -16));               // инвертирующий вход (с кружком)
    lead(c, 1, P(0, 16),  P(x0, 16));
    lead(c, 2, P(L, 0),   P(x1, 0));

    setBody(c);
    c.p->drawRect(QRectF(x0, -32, 48, 64));
    c.p->drawEllipse(P(x0 - 3.5, -16), 3.5, 3.5);
    QPolygonF tri;                                       // знак «усиление» по ГОСТ 2.721
    tri << P(cx - 8, -16) << P(cx - 8, 0) << P(cx + 6, -8);
    c.p->drawPolygon(tri);
    text(c, P(cx, 17), glyph, glyph == QLatin1String("∞") ? 18 : 15, glyph != QLatin1String("∞"));
    if (power) {                                         // выводы питания сверху и снизу
        lead(c, 3, P(cx, -48), P(cx, -32));
        lead(c, 4, P(cx, 48),  P(cx, 32));
        plusSign(c, P(cx + 8, -26), 3);
        minusSign(c, P(cx + 8, 26), 3);
    }
}

// ------------------------------------------------------------------ логика

void paintGate(const Ctx &c, const QString &sym, bool inverted, int inputs = 2)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal extra = inverted ? 7.0 : 0.0;
    const qreal x0 = cx - (40.0 + extra) / 2.0;
    const qreal x1 = x0 + 40.0;
    const qreal h = (inputs - 1) * 16.0 + 8.0;

    for (int i = 0; i < inputs; ++i) {
        const qreal y = (-(inputs - 1) + 2.0 * i) * kGrid;
        lead(c, i, P(0, y), P(x0, y));
    }
    lead(c, inputs, P(L, 0), P(x1 + extra, 0));

    setBody(c);
    c.p->drawRect(QRectF(x0, -h, 40, 2 * h));
    if (inverted)
        c.p->drawEllipse(P(x1 + 3.5, 0), 3.5, 3.5);
    text(c, P(cx - extra / 2.0, 0), sym, 18);
}

void paintNot(const Ctx &c)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal x0 = cx - 35.0 / 2.0;
    const qreal x1 = x0 + 28.0;

    lead(c, 0, P(0, 0), P(x0, 0));
    lead(c, 1, P(x1 + 7, 0), P(L, 0));

    setBody(c);
    c.p->drawRect(QRectF(x0, -14, 28, 28));
    c.p->drawEllipse(P(x1 + 3.5, 0), 3.5, 3.5);
    text(c, P(x0 + 14, 0), QStringLiteral("1"), 18);
}

// ------------------------------------------------------------------ приборы

void paintMeter(const Ctx &c, const QString &letter)
{
    const Span s = spanOf(c, 28);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 14.0, 14.0);
    text(c, P(s.cx, 0), letter, 16);
}

#include "items/SymbolPainterExtra.inc"

} // namespace

// ================================================================== Spec

Spec spec(ElementType t, unsigned variant)
{
    using T = ElementType;
    if (isGateType(t))
        return gateSpec(variant);
    if (const ChipGeom *g = chipGeom(t, variant))
        return g->spec;

    switch (t) {
    case T::Wire: {
        Spec s = twoPin(0, 0, 6);
        return s;
    }
    case T::Ground:
    case T::Chassis: {
        Spec s = makeSpec(0, 1, 1, false, 1, 12);   // любое направление
        setPin(s, 0, 0, 0);
        return s;
    }
    case T::NodeLabel: {
        Spec s = makeSpec(0, 3, 1, false, 1, 10);
        setPin(s, 0, 0, 0);
        return s;
    }
    case T::RailDC: case T::RailAC: case T::RailSquare: case T::RailClock: case T::RailSweep:
    case T::RailVariable: case T::RailNoise: case T::RailAM: case T::RailFM:
    case T::LogicInput: case T::LogicOutput: case T::Probe: case T::Antenna:
    case T::AudioInput: case T::DataInput: case T::ExtVoltage: case T::AnalogOutput:
    case T::TestPoint: case T::DataExport: case T::AudioOutput: case T::StopTrigger: {
        Spec s = makeSpec(0, 2, 1, false, 1, 12);
        setPin(s, 0, 0, 0);
        return s;
    }
    case T::TextLabel:  return makeSpec(0, 3, 0, false, 1, 10);       // надпись: выводов нет
    case T::Box:
    case T::Line:       return makeSpec(0, 1, 0, false, 1, 6);        // рамка и линия: выводов нет
    case T::Resistor:          return twoPin(40, 4, 10);
    case T::VariableResistor:  return twoPin(40, 4, 15);
    case T::Potentiometer:     return threePin(40, 4, 16, 2, 0, 0, 1, 0, 0.5, -1);
    case T::Capacitor:         return twoPin(6, 3, 12);
    case T::PolarCapacitor:    return twoPin(10, 3, 13);
    case T::VariableCapacitor: return twoPin(6, 3, 15);
    case T::Inductor:          return twoPin(48, 4, 8);
    case T::Choke:             return twoPin(48, 4, 12);
    case T::Transformer: {
        Spec s = makeSpec(40, 4, 4, true, 1, 34);
        setPin(s, 0, 0, -2);
        setPin(s, 1, 0, 2);
        setPin(s, 2, 1, -2);
        setPin(s, 3, 1, 2);
        return s;
    }
    case T::Fuse:              return twoPin(40, 4, 8);
    case T::Lamp:              return twoPin(24, 3, 13);
    case T::VoltageSourceDC:   return twoPin(32, 4, 18);
    case T::Battery:           return twoPin(8, 3, 13);
    case T::VoltageSourceAC:   return twoPin(32, 4, 17);
    case T::Generator:         return twoPin(32, 4, 17);
    case T::CurrentSource:     return twoPin(32, 4, 24);
    case T::Diode:             return twoPin(16, 3, 12);
    case T::ZenerDiode:        return twoPin(16, 3, 12);
    case T::SchottkyDiode:     return twoPin(16, 3, 12);
    case T::LED:               return twoPin(16, 3, 22);
    case T::NPN:
    case T::NMOS:
    case T::NJFET:             return threePin(40, 4, 22, 1, 0, 0, 1, -1, 1, 1);
    case T::PNP:
    case T::PMOS:
    case T::PJFET:             return threePin(40, 4, 22, 1, 0, 0, 1, 1, 1, -1);
    case T::Thyristor:         return threePin(16, 3, 34, 1, 0, 0, 1, 0, 1, 2);
    case T::Switch:            return twoPin(24, 3, 14);
    case T::PushButton:        return twoPin(24, 3, 22);
    case T::SwitchSPDT:        return threePin(24, 3, 20, 1, 0, 0, 1, -1, 1, 1);
    case T::OpAmp:             return threePin(48, 5, 34, 1, 0, -1, 0, 1, 1, 0);
    case T::GateNot:           return twoPin(28, 4, 18);
    case T::Voltmeter:
    case T::Ammeter:
    case T::Ohmmeter:
    case T::Motor:             return twoPin(28, 3, 16);

    // --- пассивные и радио
    case T::Memristor:         return twoPin(40, 4, 10);
    case T::Photoresistor:     return twoPin(40, 4, 28);
    case T::Thermistor:
    case T::Varistor:          return twoPin(40, 4, 22);
    case T::SparkGap:          return twoPin(20, 3, 10);
    case T::Crystal:           return twoPin(16, 3, 13);
    case T::InductorVar:       return twoPin(48, 4, 14);
    case T::NeonLamp:          return twoPin(24, 3, 13);
    case T::Speaker:
    case T::Microphone:        return twoPin(24, 3, 14);
    case T::Scope:             return twoPin(32, 3, 18);
    case T::DecimalDisplay:    return twoPin(36, 3, 12);
    case T::AutoTransformer: {
        Spec s = makeSpec(24, 4, 3, true, 1, 34);
        setPin(s, 0, 0, -2);
        setPin(s, 1, 1, 0);
        setPin(s, 2, 0, 2);
        return s;
    }
    case T::TransformerTapped: {
        Spec s = makeSpec(40, 4, 5, true, 1, 34);
        setPin(s, 0, 0, -2);
        setPin(s, 1, 0, 2);
        setPin(s, 2, 1, -2);
        setPin(s, 3, 1, 0);
        setPin(s, 4, 1, 2);
        return s;
    }
    case T::CoupledInductors: {
        Spec s = makeSpec(40, 4, 4, true, 1, 34);
        setPin(s, 0, 0, -2);
        setPin(s, 1, 0, 2);
        setPin(s, 2, 1, -2);
        setPin(s, 3, 1, 2);
        return s;
    }
    case T::VCVS: case T::VCCS: case T::CCVS: case T::CCCS: {
        Spec s = makeSpec(56, 5, 4, true, 1, 34);
        setPin(s, 0, 0, -2);
        setPin(s, 1, 0, 2);
        setPin(s, 2, 1, -2);
        setPin(s, 3, 1, 2);
        return s;
    }

    // --- полупроводники
    case T::Varactor:          return twoPin(20, 3, 12);
    case T::TunnelDiode:
    case T::Diac:              return twoPin(16, 3, 12);
    case T::Photodiode:        return twoPin(16, 3, 26);
    case T::Triac:
    case T::TL431:             return threePin(16, 3, 34, 1, 0, 0, 1, 0, 1, 2);
    case T::DarlingtonNPN:
    case T::Phototransistor:
    case T::UJT:
    case T::Triode:            return threePin(40, 4, 24, 1, 0, 0, 1, -1, 1, 1);
    case T::DarlingtonPNP:     return threePin(40, 4, 24, 1, 0, 0, 1, 1, 1, -1);
    case T::IGBT:              return threePin(40, 4, 22, 1, 0, 0, 1, -1, 1, 1);
    case T::DiodeBridge: {
        Spec s = makeSpec(64, 6, 4, true, 2, 50);        // шаг 2: выводы «+» и «−» посередине
        setPin(s, 0, 0, 0);
        setPin(s, 1, 1, 0);
        setPin(s, 2, 0.5, -3);
        setPin(s, 3, 0.5, 3);
        return s;
    }

    // --- коммутация
    case T::PushButtonNC:      return twoPin(24, 3, 22);
    case T::SwitchMBB:         return threePin(24, 3, 20, 1, 0, 0, 1, -1, 1, 1);
    case T::SwitchSP3T: {
        Spec s = makeSpec(24, 3, 4, true, 1, 36);
        setPin(s, 0, 0, 0);
        setPin(s, 1, 1, -2);
        setPin(s, 2, 1, 0);
        setPin(s, 3, 1, 2);
        return s;
    }
    case T::SwitchDPST: {
        Spec s = makeSpec(24, 3, 4, true, 1, 36);
        setPin(s, 0, 0, -2);
        setPin(s, 1, 1, -2);
        setPin(s, 2, 0, 2);
        setPin(s, 3, 1, 2);
        return s;
    }
    case T::SwitchDPDT: {
        Spec s = makeSpec(24, 3, 6, true, 1, 68);
        setPin(s, 0, 0, -3);
        setPin(s, 1, 1, -4);
        setPin(s, 2, 1, -2);
        setPin(s, 3, 0, 3);
        setPin(s, 4, 1, 2);
        setPin(s, 5, 1, 4);
        return s;
    }

    // --- усилители и логика
    case T::OpAmpPower: {
        Spec s = makeSpec(48, 6, 5, true, 2, 50);        // шаг 2: выводы питания посередине
        setPin(s, 0, 0, -1);
        setPin(s, 1, 0, 1);
        setPin(s, 2, 1, 0);
        setPin(s, 3, 0.5, -3);
        setPin(s, 4, 0.5, 3);
        return s;
    }
    case T::Comparator:        return threePin(48, 5, 34, 1, 0, -1, 0, 1, 1, 0);
    case T::GateBuffer:
    case T::SchmittTrigger:
    case T::SchmittInverting:  return twoPin(28, 4, 18);
    default:                   return twoPin(40, 4, 12);
    }
}

qreal minLength(ElementType type, unsigned variant)
{
    return spec(type, variant).minGrid * kGrid;
}

bool hasSinglePin(ElementType type) { return isSinglePin(type); }

int pinCount(ElementType type, unsigned variant) { return spec(type, variant).pinCount; }

QPoint defaultDirection(ElementType type)
{
    return isSinglePin(type) ? singlePinDirection(type) : QPoint(1, 0);
}

qreal defaultLength(ElementType type, unsigned variant)
{
    if (isSinglePin(type))
        return 2 * kGrid;
    const Spec s = spec(type, variant);
    int g = std::max(s.minGrid, 4);
    const int step = std::max(1, s.stepGrid);
    g = ((g + step - 1) / step) * step;
    return g * kGrid;
}

QPointF pinLocal(ElementType type, int pin, qreal length, bool flip, unsigned variant)
{
    const Spec s = spec(type, variant);
    if (pin < 0 || pin >= s.pinCount)
        return QPointF();
    return QPointF(s.pin[pin].u * length + s.pin[pin].du, s.pin[pin].v * kGrid * (flip ? -1.0 : 1.0));
}

QPointF pinInward(ElementType type, int pin, unsigned variant)
{
    const Spec s = spec(type, variant);
    if (pin < 0 || pin >= s.pinCount)
        return QPointF(1, 0);
    const PinSpec &ps = s.pin[pin];
    if (ps.side == 2)
        return QPointF(0, 1);
    if (ps.side == 3)
        return QPointF(0, -1);
    if (ps.u <= 0.0)
        return QPointF(1, 0);
    if (ps.u >= 1.0)
        return QPointF(-1, 0);
    return QPointF(0, ps.v < 0 ? 1 : -1);               // вывод посередине (движок)
}

int partAt(ElementType type, unsigned variant, qreal length, const QPointF &local)
{
    if (const ChipGeom *g = chipGeom(type, variant))
        return partAtImpl(*g, length, local);
    return 0;
}

bool isValidGeometry(ElementType type, const QPoint &a, const QPoint &b, unsigned variant)
{
    if (isFreeShape(type))
        return true;

    const int dx = b.x() - a.x();
    const int dy = b.y() - a.y();

    const Spec s = spec(type, variant);
    const int minLen = s.minGrid * int(kGrid);
    if (s.axisOnly) {
        if (dx != 0 && dy != 0)
            return false;
        const int len = std::abs(dx) + std::abs(dy);
        const int step = std::max(1, s.stepGrid) * int(kGrid);
        return len >= minLen && len % step == 0;
    }
    return std::hypot(double(dx), double(dy)) >= minLen - 0.5;
}

QPoint normalizeEnd(ElementType type, const QPoint &a, const QPoint &b, unsigned variant)
{
    if (isFreeShape(type))
        return b;

    const Spec s = spec(type, variant);
    const int minLen = s.minGrid * int(kGrid);

    if (!s.axisOnly) {
        if (std::hypot(double(b.x() - a.x()), double(b.y() - a.y())) >= minLen - 0.5)
            return b;
        return a + dominantAxis(a, b, isSinglePin(type) ? singlePinDirection(type) : QPoint(1, 0)) * minLen;     // слишком коротко
    }

    const QPoint dir = dominantAxis(a, b, isSinglePin(type) ? singlePinDirection(type) : QPoint(1, 0));
    const int raw = std::abs(b.x() - a.x()) + std::abs(b.y() - a.y());
    const int step = std::max(1, s.stepGrid) * int(kGrid);
    int len = int(std::lround(double(raw) / step)) * step;
    len = std::max(len, ((minLen + step - 1) / step) * step);
    return a + dir * len;
}

// ================================================================== paint

void paint(const PaintContext &c)
{
    using T = ElementType;
    switch (c.type) {
    case T::Wire:              break;
    case T::Ground:            paintGround(c); break;
    case T::Chassis:           paintChassis(c); break;
    case T::Resistor:          paintResistor(c, false, false); break;
    case T::VariableResistor:  paintResistor(c, true, false); break;
    case T::Potentiometer:     paintResistor(c, false, true); break;
    case T::Capacitor:         paintCapacitor(c, false); break;
    case T::PolarCapacitor:    paintPolarCapacitor(c); break;
    case T::VariableCapacitor: paintCapacitor(c, true); break;
    case T::Inductor:          paintInductor(c, false); break;
    case T::Choke:             paintInductor(c, true); break;
    case T::Transformer:       paintTransformer(c); break;
    case T::Fuse:              paintFuse(c); break;
    case T::Lamp:              paintLamp(c); break;
    case T::VoltageSourceDC:   paintVoltageDC(c); break;
    case T::Battery:           paintBattery(c); break;
    case T::VoltageSourceAC:   paintWaveSource(c, 0); break;
    case T::Generator:         paintWaveSource(c, c.state); break;
    case T::CurrentSource:     paintCurrentSource(c); break;
    case T::Diode:             paintDiode(c); break;
    case T::ZenerDiode:        paintZener(c); break;
    case T::SchottkyDiode:     paintSchottky(c); break;
    case T::LED:               paintLed(c); break;
    case T::NPN:               paintBjt(c, false); break;
    case T::PNP:               paintBjt(c, true); break;
    case T::NMOS:              paintMosfet(c, true); break;
    case T::PMOS:              paintMosfet(c, false); break;
    case T::NJFET:             paintJfet(c, true); break;
    case T::PJFET:             paintJfet(c, false); break;
    case T::Thyristor:         paintThyristor(c); break;
    case T::Switch:            paintSwitch(c); break;
    case T::PushButton:        paintPushButton(c); break;
    case T::SwitchSPDT:        paintSpdt(c); break;
    case T::OpAmp:             paintOpAmp(c); break;
    case T::GateAnd:           paintGate(c, QStringLiteral("&"), false, gateInputs(c.variant)); break;
    case T::GateOr:            paintGate(c, QStringLiteral("1"), false, gateInputs(c.variant)); break;
    case T::GateNand:          paintGate(c, QStringLiteral("&"), true, gateInputs(c.variant)); break;
    case T::GateNor:           paintGate(c, QStringLiteral("1"), true, gateInputs(c.variant)); break;
    case T::GateXor:           paintGate(c, QStringLiteral("=1"), false, gateInputs(c.variant)); break;
    case T::GateNot:           paintNot(c); break;
    case T::Voltmeter:         paintMeter(c, QStringLiteral("V")); break;
    case T::Ammeter:           paintMeter(c, QStringLiteral("A")); break;

    case T::NodeLabel:         paintSingle(c, 13); break;
    case T::Memristor:         paintMemristor(c); break;
    case T::Photoresistor:     paintPhotoresistor(c); break;
    case T::Thermistor:        paintThermistor(c); break;
    case T::Varistor:          paintVaristor(c); break;
    case T::SparkGap:          paintSparkGap(c); break;
    case T::InductorVar:       paintInductor(c, false, true); break;
    case T::AutoTransformer:   paintAutoTransformer(c); break;
    case T::TransformerTapped: paintTransformer(c, 1); break;
    case T::CoupledInductors:  paintTransformer(c, 2); break;

    case T::RailDC:            paintSingle(c, 0); break;
    case T::RailAC:            paintSingle(c, 1); break;
    case T::RailSquare:        paintSingle(c, 2); break;
    case T::RailClock:         paintSingle(c, 3); break;
    case T::RailSweep:         paintSingle(c, 4); break;
    case T::RailVariable:      paintSingle(c, 5); break;
    case T::RailNoise:         paintSingle(c, 6); break;
    case T::RailAM:            paintSingle(c, 7); break;
    case T::RailFM:            paintSingle(c, 8); break;
    case T::Antenna:           paintSingle(c, 9); break;
    case T::LogicInput:        paintSingle(c, 10); break;
    case T::LogicOutput:       paintSingle(c, 11); break;
    case T::Probe:             paintSingle(c, 12); break;
    case T::AudioInput:        paintSingle(c, 14); break;
    case T::DataInput:         paintSingle(c, 15); break;
    case T::ExtVoltage:        paintSingle(c, 16); break;
    case T::AnalogOutput:      paintSingle(c, 17); break;
    case T::TestPoint:         paintSingle(c, 18); break;
    case T::DataExport:        paintSingle(c, 19); break;
    case T::AudioOutput:       paintSingle(c, 20); break;
    case T::StopTrigger:       paintSingle(c, 21); break;
    case T::TextLabel:         break;                       // сам текст рисует CircuitItem
    case T::Line:              paintLine(c); break;
    case T::Box:               paintBox(c); break;

    case T::VCVS:              paintControlled(c, 0); break;
    case T::VCCS:              paintControlled(c, 1); break;
    case T::CCVS:              paintControlled(c, 2); break;
    case T::CCCS:              paintControlled(c, 3); break;

    case T::Varactor:          paintVaractor(c); break;
    case T::TunnelDiode:       paintTunnelDiode(c); break;
    case T::Diac:              paintDiac(c); break;
    case T::Triac:             paintTriac(c); break;
    case T::DarlingtonNPN:     paintDarlington(c, false); break;
    case T::DarlingtonPNP:     paintDarlington(c, true); break;
    case T::Photodiode:        paintPhotodiode(c); break;
    case T::Phototransistor:   paintPhototransistor(c); break;
    case T::IGBT:              paintIgbt(c); break;
    case T::UJT:               paintUjt(c); break;
    case T::TL431:             paintTl431(c); break;
    case T::Triode:            paintTriode(c); break;
    case T::DiodeBridge:       paintDiodeBridge(c); break;

    case T::PushButtonNC:      paintPushButton(c, true); break;
    case T::SwitchMBB:         paintSpdt(c); break;
    case T::SwitchSP3T:        paintSwitchSP3T(c); break;
    case T::SwitchDPST:        paintSwitchDPST(c); break;
    case T::SwitchDPDT:        paintSwitchDPDT(c); break;

    case T::OpAmpPower:        paintOpAmp(c, true); break;
    case T::Comparator:        paintOpAmp(c, false, QStringLiteral(">")); break;
    case T::GateXnor:          paintGate(c, QStringLiteral("=1"), true, gateInputs(c.variant)); break;
    case T::GateBuffer:        paintBuffer(c); break;
    case T::SchmittTrigger:    paintSchmitt(c, false); break;
    case T::SchmittInverting:  paintSchmitt(c, true); break;

    case T::Ohmmeter:          paintMeter(c, QString(QChar(0x03A9))); break;
    case T::Motor:             paintMeter(c, QStringLiteral("M")); break;
    case T::Scope:             paintScope(c); break;
    case T::DecimalDisplay:    paintDecimalDisplay(c); break;

    case T::Crystal:           paintCrystal(c); break;
    case T::NeonLamp:          paintNeonLamp(c); break;
    case T::Speaker:           paintSpeaker(c); break;
    case T::Microphone:        paintMicrophone(c); break;

    default:
        if (const ChipGeom *g = chipGeom(c.type, c.variant))
            paintChip(c, *g);
        break;
    }
}

// ================================================================== иконки

QPixmap icon(ElementType type, int size, const QColor &color)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.scale(size / 80.0, size / 80.0);

    if (type == ElementType::Wire) {
        p.setPen(QPen(color, 5.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(12, 40), QPointF(68, 40));
        p.setBrush(color);
        p.drawEllipse(QPointF(12, 40), 5.0, 5.0);
        p.drawEllipse(QPointF(68, 40), 5.0, 5.0);
        return pm;
    }

    PaintContext c;
    c.p = &p;
    c.type = type;
    c.body = color;
    for (QColor &pc : c.pin)
        pc = color;
    c.width = 5.5;
    c.textScale = 1.3;

    if (isSinglePin(type)) {
        c.length = 40;
        const bool down = singlePinDirection(type).y() > 0;
        p.translate(40, down ? 14 : 66);
        p.rotate(down ? 90 : -90);         // земля — стержень вниз, шины и антенна — вверх
        p.scale(1.15, 1.15);
        c.width = 5.0;
        paint(c);
        return pm;
    }

    const Spec s = spec(type);
    c.length = std::max<qreal>(s.minGrid * kGrid, 64.0);
    c.width = 5.5;
    const qreal fit = std::min({ 1.0, 66.0 / c.length, 70.0 / (2.0 * s.halfHeight) });
    p.translate(40, 40);
    p.scale(fit, fit);
    p.translate(-c.length / 2.0, 0);
    paint(c);
    return pm;
}

QPixmap selectIcon(int size, const QColor &color)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / 80.0, size / 80.0);
    p.setPen(QPen(color, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(color);
    QPolygonF arrow;
    arrow << QPointF(22, 12) << QPointF(22, 62) << QPointF(34, 51)
          << QPointF(42, 68) << QPointF(50, 64) << QPointF(42, 47)
          << QPointF(57, 47);
    p.drawPolygon(arrow);
    return pm;
}

} // namespace SymbolPainter
