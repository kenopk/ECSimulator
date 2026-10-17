#include "scope/ScopePlot.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "items/CircuitItem.h"
#include "theme/Theme.h"
#include "util/Units.h"

namespace ScopePlot {

namespace {

constexpr int kDivX = 10;
constexpr int kDivY = 8;

// Округление вверх до 1 / 2 / 5 × 10^n — так делают значения на клетку у настоящих осциллографов.
double niceStep(double x)
{
    if (!(x > 0.0) || !std::isfinite(x))
        return 1.0;
    const double e = std::floor(std::log10(x));
    const double base = std::pow(10.0, e);
    const double f = x / base;
    const double m = f <= 1.0 ? 1.0 : f <= 2.0 ? 2.0 : f <= 5.0 ? 5.0 : 10.0;
    return m * base;
}

struct Range {
    double perDiv = 1.0;
    double mid = 0.0;
};

// Видимый участок: индексы отсчётов и границы по времени.
struct Window {
    int i0 = 0;
    int i1 = -1;
    double t0 = 0;
    double t1 = 0;
};

double channelValue(const ScopeSample &s, int channel)
{
    return channel == 0 ? s.v : s.i;
}

Window computeWindow(const QVector<ScopeSample> &s, const ScopeSettings &st)
{
    Window w;
    const int n = int(s.size());
    w.i0 = 0;
    w.i1 = n - 1;
    w.t0 = s.first().t;
    w.t1 = s.last().t;
    if (st.timePerDiv <= 0.0 || st.xy)
        return w;

    const double span = st.timePerDiv * kDivX;
    const double tEnd = s.last().t;
    double start = tEnd - span;

    if (st.trigger) {
        // Последнее пересечение уровня, после которого ещё помещается всё окно; если такого нет — самое свежее.
        int best = -1, last = -1;
        for (int k = 1; k < n; ++k) {
            const double a = channelValue(s[k - 1], st.triggerChannel);
            const double b = channelValue(s[k], st.triggerChannel);
            const bool cross = st.triggerRising ? (a < st.triggerLevel && b >= st.triggerLevel)
                                                : (a > st.triggerLevel && b <= st.triggerLevel);
            if (!cross)
                continue;
            last = k;
            if (s[k].t + span <= tEnd)
                best = k;
        }
        const int k = best >= 0 ? best : last;
        if (k >= 0)
            start = s[k].t;
    }

    w.t0 = start;
    w.t1 = start + span;
    w.i0 = n;
    w.i1 = -1;
    for (int k = 0; k < n; ++k) {
        if (s[k].t < w.t0)
            continue;
        if (s[k].t > w.t1)
            break;
        w.i0 = std::min(w.i0, k);
        w.i1 = k;
    }
    return w;
}

Range rangeFor(const ScopeSource *src, const Window &w, bool voltage)
{
    const ScopeSettings &st = src->settings();
    Range r;
    if (st.vscale == VScale::Manual) {
        r.perDiv = std::max(1e-12, (voltage ? st.vMax : st.iMax) / (kDivY / 2.0));
        return r;
    }
    if (st.vscale == VScale::AutoMax) {
        const double m = voltage ? src->maxVoltage() : src->maxCurrent();
        r.perDiv = niceStep(std::max(m, 1e-12) / (kDivY / 2.0 - 0.5));
        return r;
    }

    double lo = 0, hi = 0;
    bool first = true;
    const QVector<ScopeSample> &s = src->samples();
    for (int k = w.i0; k <= w.i1 && k < s.size(); ++k) {
        const double y = voltage ? s[k].v : s[k].i;
        if (first) { lo = hi = y; first = false; }
        lo = std::min(lo, y);
        hi = std::max(hi, y);
    }
    r.perDiv = niceStep(std::max(hi - lo, 1e-12) / (kDivY - 2));
    r.mid = (lo + hi) / 2.0;
    if (std::fabs(r.mid) < r.perDiv)        // около нуля — середину сетки ставим на ноль
        r.mid = 0.0;
    return r;
}

// Частота по переходам через среднее значение напряжения на видимом участке (или тока, если сигнала нет).
double estimateFrequency(const QVector<ScopeSample> &s, const Window &w, bool voltage)
{
    if (w.i1 - w.i0 < 3)
        return 0;
    double mean = 0;
    for (int k = w.i0; k <= w.i1; ++k)
        mean += voltage ? s[k].v : s[k].i;
    mean /= (w.i1 - w.i0 + 1);

    double firstT = 0, lastT = 0;
    int crossings = 0;
    for (int k = w.i0 + 1; k <= w.i1; ++k) {
        const double a = (voltage ? s[k - 1].v : s[k - 1].i) - mean;
        const double b = (voltage ? s[k].v : s[k].i) - mean;
        if (a < 0 && b >= 0) {
            const double t = s[k - 1].t + (s[k].t - s[k - 1].t) * (-a / (b - a));
            if (crossings == 0)
                firstT = t;
            lastT = t;
            ++crossings;
        }
    }
    if (crossings < 2 || lastT <= firstT)
        return 0;
    return (crossings - 1) / (lastT - firstT);
}

int indexAtTime(const QVector<ScopeSample> &s, const Window &w, double t)
{
    int best = w.i0;
    double bestD = 1e300;
    for (int k = w.i0; k <= w.i1 && k < s.size(); ++k) {
        const double d = std::fabs(s[k].t - t);
        if (d < bestD) {
            bestD = d;
            best = k;
        }
    }
    return best;
}

void drawTrace(QPainter *p, const QRectF &plot, const QVector<ScopeSample> &s, const Window &w,
               bool voltage, const Range &range, const QColor &color)
{
    if (w.i1 < w.i0)
        return;
    const double dt = w.t1 - w.t0;
    const double stepY = plot.height() / kDivY;
    const int count = w.i1 - w.i0 + 1;
    const int stride = std::max<int>(1, int(count / std::max(1.0, plot.width() * 2.0)));

    QPainterPath path;
    bool started = false;
    for (int k = w.i0; k <= w.i1; k += stride) {
        const double x = dt > 0 ? (s[k].t - w.t0) / dt : double(k - w.i0) / std::max(1, count - 1);
        const double y = voltage ? s[k].v : s[k].i;
        const QPointF pt(plot.left() + x * plot.width(),
                         plot.center().y() - (y - range.mid) / range.perDiv * stepY);
        if (!started) { path.moveTo(pt); started = true; } else { path.lineTo(pt); }
    }
    p->setClipRect(plot);
    p->setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
    p->setClipping(false);
}

void drawXY(QPainter *p, const QRectF &plot, const QVector<ScopeSample> &s, const Window &w,
            const Range &rv, const Range &ri, const QColor &color)
{
    const double stepX = plot.width() / kDivX;
    const double stepY = plot.height() / kDivY;
    QPainterPath path;
    bool started = false;
    const int stride = std::max<int>(1, int((w.i1 - w.i0 + 1) / std::max(1.0, plot.width() * 2.0)));
    for (int k = w.i0; k <= w.i1; k += stride) {
        const QPointF pt(plot.center().x() + (s[k].v - rv.mid) / rv.perDiv * stepX,
                         plot.center().y() - (s[k].i - ri.mid) / ri.perDiv * stepY);
        if (!started) { path.moveTo(pt); started = true; } else { path.lineTo(pt); }
    }
    p->setClipRect(plot);
    p->setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
    p->setClipping(false);
}

void drawGear(QPainter *p, const QRectF &r, const QColor &color)
{
    const QPointF c = r.center();
    const qreal outer = r.width() * 0.46;
    const qreal inner = r.width() * 0.34;
    QPainterPath path;
    const int teeth = 8;
    for (int k = 0; k < teeth * 2; ++k) {
        const qreal a0 = (k * M_PI) / teeth;
        const qreal a1 = ((k + 1) * M_PI) / teeth;
        const qreal rad = (k % 2 == 0) ? outer : inner;
        const QPointF p0(c.x() + std::cos(a0) * rad, c.y() + std::sin(a0) * rad);
        const QPointF p1(c.x() + std::cos(a1) * rad, c.y() + std::sin(a1) * rad);
        if (k == 0)
            path.moveTo(p0);
        path.lineTo(p1);
    }
    path.closeSubpath();
    path.addEllipse(c, r.width() * 0.16, r.width() * 0.16);   // отверстие
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPath(path);
}

QString num(double v, const char *unit)
{
    return Units::format(v, QString::fromUtf8(unit));
}

} // namespace

QRectF gearRect(const QRectF &rect)
{
    return QRectF(rect.left() + 6, rect.bottom() - 24, 18, 18);
}

const QVector<double> &timeSteps()
{
    static const QVector<double> steps = [] {
        QVector<double> v;
        for (int e = -6; e <= 0; ++e)
            for (double m : { 1.0, 2.0, 5.0 })
                v.append(m * std::pow(10.0, e));
        v.append(10.0);
        return v;
    }();
    return steps;
}

void paint(QPainter *p, const QRectF &rect, const ScopeSource *src, const Options &opt)
{
    const Palette &pal = Theme::instance().palette();
    const ScopeSettings &st = src->settings();
    const QVector<ScopeSample> &samples = src->samples();

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    // фон
    p->setPen(Qt::NoPen);
    p->setBrush(pal.panel);
    p->drawRect(rect);

    const QRectF plot = rect.adjusted(2, 2, -2, -2);

    // сетка
    const qreal stepX = plot.width() / kDivX;
    const qreal stepY = plot.height() / kDivY;
    for (int i = 0; i <= kDivX; ++i) {
        p->setPen(QPen(i == kDivX / 2 ? pal.gridMajor : pal.gridMinor, 1));
        const qreal x = plot.left() + i * stepX;
        p->drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int j = 0; j <= kDivY; ++j) {
        p->setPen(QPen(j == kDivY / 2 ? pal.gridMajor : pal.gridMinor, 1));
        const qreal y = plot.top() + j * stepY;
        p->drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    QFont small = Theme::monoFont(11);                       // подписи шкал и значения — моноширинным
    p->setFont(small);
    const QFontMetricsF fm(small);
    const qreal lh = fm.height() + 1;

    const bool hasData = !samples.isEmpty();
    Window w;
    Range rv, ri;
    if (hasData) {
        w = computeWindow(samples, st);
        rv = rangeFor(src, w, true);
        ri = rangeFor(src, w, false);

        if (st.xy) {
            drawXY(p, plot, samples, w, rv, ri, pal.accent);
        } else {
            if (st.showVoltage)
                drawTrace(p, plot, samples, w, true, rv, pal.accent);
            if (st.showCurrent)
                drawTrace(p, plot, samples, w, false, ri, pal.current);
        }
    }

    // --- подписи слева сверху: максимум и название
    qreal y = plot.top() + lh;
    p->setPen(pal.text);
    if (hasData && st.showMax && w.i1 >= w.i0) {
        double maxV = 0, maxI = 0;
        for (int k = w.i0; k <= w.i1; ++k) {
            maxV = std::max(maxV, std::fabs(samples[k].v));
            maxI = std::max(maxI, std::fabs(samples[k].i));
        }
        const bool useV = st.xy || st.showVoltage || !st.showCurrent;
        p->drawText(QPointF(plot.left() + 6, y),
                    useV ? QStringLiteral("Max=") + num(maxV, "V") : QStringLiteral("Max=") + num(maxI, "A"));
        y += lh;
    }
    if (st.infoName) {
        p->setPen(pal.textDim);
        p->drawText(QPointF(plot.left() + 6, y), fm.elidedText(src->title(), Qt::ElideRight, plot.width() * 0.45));
    }

    // --- нет данных / канал отключён
    if (!hasData) {
        p->setPen(pal.textDim);
        p->drawText(plot.adjusted(plot.width() * 0.1, 0, -plot.width() * 0.1, 0), Qt::AlignCenter | Qt::TextWordWrap,
                    src->enabled() ? QStringLiteral("Нет данных — отсчёты появятся, когда ядро начнёт расчёт цепи")
                                   : QStringLiteral("Канал отключён"));
    } else if (!src->enabled()) {
        p->setPen(pal.negative);
        p->drawText(plot.adjusted(0, lh, -6, 0), Qt::AlignRight | Qt::AlignTop, QStringLiteral("канал отключён"));
    }

    // --- информационный блок справа (по свойствам)
    if (hasData && w.i1 >= w.i0) {
        const int idx = opt.hover >= 0 && !st.xy
            ? indexAtTime(samples, w, w.t0 + (w.t1 - w.t0) * opt.hover)
            : w.i1;
        const ScopeSample &cur = samples[idx];

        QStringList lines;
        if (st.infoName)
            lines << src->title().toLower();
        if (st.infoCurrent)
            lines << QStringLiteral("I = ") + num(cur.i, "A");
        if (st.infoVoltage)
            lines << QStringLiteral("Vd = ") + num(cur.v, "V");
        if (st.infoResistance) {
            double r = 0;
            bool ok = false;
            if (src->target() && src->target()->params().contains(QStringLiteral("resistance"))) {
                r = src->target()->param(QStringLiteral("resistance"));
                ok = true;
            } else if (std::fabs(cur.i) > 1e-12) {
                r = std::fabs(cur.v / cur.i);
                ok = true;
            }
            lines << QStringLiteral("R = ") + (ok ? num(r, "Ω") : QStringLiteral("—"));
        }
        if (st.infoPower)
            lines << QStringLiteral("P = ") + num(cur.v * cur.i, "W");
        if (st.infoFrequency) {
            double f = estimateFrequency(samples, w, st.showVoltage || !st.showCurrent);
            lines << QStringLiteral("f₀ = ") + (f > 0 ? num(f, "Hz") : QStringLiteral("—"));
        }

        if (!lines.isEmpty()) {
            qreal width = 0;
            for (const QString &l : std::as_const(lines))
                width = std::max(width, fm.horizontalAdvance(l));
            const QRectF box(plot.right() - width - 18, plot.top() + 4, width + 14, lines.size() * lh + 8);
            QColor bg = pal.panel;
            bg.setAlpha(215);
            p->setPen(Qt::NoPen);
            p->setBrush(bg);
            p->drawRoundedRect(box, 4, 4);
            p->setPen(pal.text);
            qreal ty = box.top() + lh;
            for (const QString &l : std::as_const(lines)) {
                p->drawText(QPointF(box.left() + 7, ty), l);
                ty += lh;
            }
        }
    }

    // --- масштабы внизу
    if (hasData) {
        qreal x = plot.left() + 30;
        const qreal by = plot.bottom() - 6;
        if (st.xy) {
            p->setPen(pal.accent);
            const QString t = QStringLiteral("X: %1/кл   Y: %2/кл")
                                  .arg(num(rv.perDiv, "V"), num(ri.perDiv, "A"));
            p->drawText(QPointF(x, by), t);
        } else {
            if (st.showVoltage) {
                p->setPen(pal.accent);
                const QString t = QStringLiteral("V: %1/кл").arg(num(rv.perDiv, "V"));
                p->drawText(QPointF(x, by), t);
                x += fm.horizontalAdvance(t) + 12;
            }
            if (st.showCurrent) {
                p->setPen(pal.current);
                p->drawText(QPointF(x, by), QStringLiteral("I: %1/кл").arg(num(ri.perDiv, "A")));
            }
            p->setPen(pal.textDim);
            const double perDivT = (w.t1 - w.t0) / kDivX;
            p->drawText(plot.adjusted(0, 0, -6, -6), Qt::AlignRight | Qt::AlignBottom,
                        perDivT > 0 ? QStringLiteral("t: %1/кл").arg(num(perDivT, "с")) : QStringLiteral("t: —"));
        }
    }

    // --- курсор: вертикальная линия и значения в этой точке
    if (hasData && opt.hover >= 0 && !st.xy && w.i1 >= w.i0) {
        const double t = w.t0 + (w.t1 - w.t0) * opt.hover;
        const ScopeSample &cur = samples[indexAtTime(samples, w, t)];
        const qreal x = plot.left() + opt.hover * plot.width();
        p->setPen(QPen(pal.textDim, 1, Qt::DashLine));
        p->drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));

        QStringList tip;
        if (st.showVoltage || !st.showCurrent)
            tip << num(cur.v, "V");
        if (st.showCurrent)
            tip << num(cur.i, "A");
        tip << num(cur.t, "с");
        qreal tw = 0;
        for (const QString &l : std::as_const(tip))
            tw = std::max(tw, fm.horizontalAdvance(l));
        QRectF box(x + 8, plot.center().y() - 4, tw + 12, tip.size() * lh + 6);
        if (box.right() > plot.right() - 2)
            box.moveRight(x - 8);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 200));
        p->drawRoundedRect(box, 3, 3);
        p->setPen(Qt::white);
        qreal ty = box.top() + lh - 1;
        for (const QString &l : std::as_const(tip)) {
            p->drawText(QPointF(box.left() + 6, ty), l);
            ty += lh;
        }
    }

    // --- шестерёнка «Параметры»
    drawGear(p, gearRect(rect), opt.gearHover ? pal.accent : pal.textDim);

    p->restore();
}

} // namespace ScopePlot
