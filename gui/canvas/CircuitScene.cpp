#include "canvas/CircuitScene.h"

#include <QPainter>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <cmath>

#include "items/CircuitItem.h"
#include "theme/Theme.h"

CircuitScene::CircuitScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-50000, -50000, 100000, 100000);

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        invalidateConnectivity();                       // могли переключить «красные концы»
        const QList<QGraphicsItem *> all = items();
        for (QGraphicsItem *gi : all)
            gi->update();
        invalidate(QRectF(), QGraphicsScene::BackgroundLayer | QGraphicsScene::ForegroundLayer);
        update();
    });
}

QPoint CircuitScene::snap(const QPointF &pos)
{
    return QPoint(qRound(pos.x() / kGrid) * kGrid, qRound(pos.y() / kGrid) * kGrid);
}

qint64 CircuitScene::key(const QPoint &p)
{
    return (qint64(p.x()) << 32) ^ qint64(quint32(p.y()));
}

QList<CircuitItem *> CircuitScene::circuitItems() const
{
    QList<CircuitItem *> out;
    const QList<QGraphicsItem *> all = items();
    out.reserve(all.size());
    for (QGraphicsItem *gi : all)
        if (auto *ci = qgraphicsitem_cast<CircuitItem *>(gi))
            out.append(ci);
    return out;
}

QRectF CircuitScene::contentRect() const
{
    QRectF r;
    for (CircuitItem *ci : circuitItems()) {
        if (ci->isPreview())
            continue;
        const QRectF b = QRectF(QPointF(ci->p1()), QPointF(ci->p2())).normalized();
        r = r.isNull() ? b : r.united(b);
        for (const QPoint &pt : ci->connectionPoints())
            r = r.united(QRectF(QPointF(pt), QSizeF(0, 0)));
    }
    return r;
}

// ------------------------------------------------------------- соединения

void CircuitScene::invalidateConnectivity()
{
    m_connectivityDirty = true;
    if (m_refreshQueued)
        return;
    m_refreshQueued = true;
    // Пересчёт после обработки текущего события: красные концы у соседей
    // должны обновиться, даже если сами они не перерисовываются.
    QTimer::singleShot(0, this, [this] { refresh(); });
}

void CircuitScene::refresh()
{
    m_refreshQueued = false;
    ensureConnectivity();
    invalidate(QRectF(), QGraphicsScene::ForegroundLayer);
    emit topologyChanged();
}

void CircuitScene::ensureConnectivity() const
{
    if (!m_connectivityDirty)
        return;
    rebuild();
    m_connectivityDirty = false;
}

namespace {

struct WireRef {
    CircuitItem *item;
    int id1;
    QPoint a, b;
};

// Точка строго внутри отрезка (не на концах).
bool strictlyInside(const QPoint &p, const QPoint &a, const QPoint &b)
{
    const qint64 abx = b.x() - a.x(), aby = b.y() - a.y();
    const qint64 apx = p.x() - a.x(), apy = p.y() - a.y();
    if (abx * apy - aby * apx != 0)
        return false;
    const qint64 dot = abx * apx + aby * apy;
    return dot > 0 && dot < abx * abx + aby * aby;
}

} // namespace

void CircuitScene::rebuild() const
{
    m_nodes.clear();
    m_nearPoints.clear();

    const QList<CircuitItem *> all = circuitItems();
    const bool checkNear = Theme::instance().showNearMiss();

    // --- узлы (для рисования точек соединений) ---------------------------
    for (CircuitItem *ci : all) {
        if (ci->isPreview())
            continue;
        for (const QPoint &pt : ci->connectionPoints()) {
            Node &n = m_nodes[key(pt)];
            n.pt = pt;
            ++n.count;
        }
    }

    if (!checkNear) {
        for (CircuitItem *ci : all)
            ci->setNearMiss(0);
        return;
    }

    // --- «почти касания»: объединяем точки, связанные проводами -----------
    QHash<qint64, int> idOf;
    QVector<QPoint> idPoint;
    QVector<int> parent;
    QVector<int> count;
    QVector<QVector<CircuitItem *>> owners;
    QVector<WireRef> wires;

    const auto getId = [&](const QPoint &pt) {
        const qint64 k = key(pt);
        auto it = idOf.constFind(k);
        if (it != idOf.constEnd())
            return it.value();
        const int id = idPoint.size();
        idOf.insert(k, id);
        idPoint.append(pt);
        parent.append(id);
        count.append(0);
        owners.append(QVector<CircuitItem *>());
        return id;
    };
    const auto find = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    for (CircuitItem *ci : all) {
        const QVector<QPoint> pts = ci->connectionPoints();
        QVector<int> ids;
        for (const QPoint &pt : pts) {
            const int id = getId(pt);
            ids.append(id);
            ++count[id];
            owners[id].append(ci);
        }
        if (ci->isWire() && ids.size() == 2) {
            const int ra = find(ids[0]), rb = find(ids[1]);
            if (ra != rb)
                parent[ra] = rb;
            WireRef ref;
            ref.item = ci;
            ref.id1 = ids[0];
            ref.a = ci->p1();
            ref.b = ci->p2();
            wires.append(ref);
        }
    }

    constexpr int kOff[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1} };
    QHash<CircuitItem *, quint64> masks;

    for (int id = 0; id < idPoint.size(); ++id) {
        if (count[id] != 1)
            continue;                                   // красим только свободные концы
        CircuitItem *owner = owners[id].first();
        const QPoint pt = idPoint[id];
        const int root = find(id);

        bool bad = false;
        for (const auto &off : kOff) {                  // вплотную к чужому узлу
            const QPoint q = pt + QPoint(off[0] * kGrid, off[1] * kGrid);
            const auto it = idOf.constFind(key(q));
            if (it == idOf.constEnd())
                continue;
            if (find(it.value()) == root || owners[it.value()].contains(owner))
                continue;
            bad = true;
            break;
        }
        if (!bad) {                                     // конец лежит на чужом проводе, но не соединён
            for (const WireRef &w : std::as_const(wires)) {
                if (w.item == owner || find(w.id1) == root)
                    continue;
                if (strictlyInside(pt, w.a, w.b)) {
                    bad = true;
                    break;
                }
            }
        }
        if (!bad)
            continue;

        const QVector<QPoint> pts = owner->connectionPoints();
        for (int i = 0; i < pts.size(); ++i)
            if (pts[i] == pt)
                masks[owner] |= (quint64(1) << i);
        m_nearPoints.append(pt);
    }

    for (CircuitItem *ci : all)
        ci->setNearMiss(masks.value(ci, quint64(0)));
}

int CircuitScene::connectionCount(const QPoint &p) const
{
    ensureConnectivity();
    const auto it = m_nodes.constFind(key(p));
    return it == m_nodes.constEnd() ? 0 : it.value().count;
}

void CircuitScene::tickFlow(qreal dt)
{
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *gi : all)
        if (auto *ci = qgraphicsitem_cast<CircuitItem *>(gi))
            ci->advanceFlow(dt);
}

void CircuitScene::clearElectricalState()
{
    for (CircuitItem *ci : circuitItems())
        ci->clearElectrical();
}

// ---------------------------------------------------------------- рисование

void CircuitScene::drawBackground(QPainter *p, const QRectF &rect)
{
    const Theme &theme = Theme::instance();
    const Palette &pal = theme.palette();
    p->fillRect(rect, pal.bg);

    const GridStyle style = theme.gridStyle();
    if (style == GridStyle::None)
        return;

    const qreal scale = p->worldTransform().m11();
    if (scale <= 0.0)
        return;

    // Прореживаем сетку, чтобы клетки на экране были не мельче ~8 px.
    const int mult = std::max(1, int(std::ceil(8.0 / (kGrid * scale))));
    const int step = kGrid * mult;
    const int majorEvery = 5 * mult;

    const int ix0 = int(std::floor(rect.left()   / step));
    const int ix1 = int(std::ceil (rect.right()  / step));
    const int iy0 = int(std::floor(rect.top()    / step));
    const int iy1 = int(std::ceil (rect.bottom() / step));

    p->setRenderHint(QPainter::Antialiasing, true);

    if (style == GridStyle::Dots) {
        QVector<QPointF> minor, major;
        for (int ix = ix0; ix <= ix1; ++ix) {
            for (int iy = iy0; iy <= iy1; ++iy) {
                const bool isMajor = (ix * mult) % majorEvery == 0 && (iy * mult) % majorEvery == 0;
                (isMajor ? major : minor).append(QPointF(ix * step, iy * step));
            }
        }
        QPen pen(pal.gridMinor, 2.0, Qt::SolidLine, Qt::RoundCap);
        pen.setCosmetic(true);
        p->setPen(pen);
        p->drawPoints(minor.constData(), int(minor.size()));
        pen.setColor(pal.gridMajor);
        pen.setWidthF(3.0);
        p->setPen(pen);
        p->drawPoints(major.constData(), int(major.size()));
    } else {
        QVector<QLineF> minor, major;
        for (int ix = ix0; ix <= ix1; ++ix) {
            const bool isMajor = (ix * mult) % majorEvery == 0;
            (isMajor ? major : minor).append(QLineF(ix * step, rect.top(), ix * step, rect.bottom()));
        }
        for (int iy = iy0; iy <= iy1; ++iy) {
            const bool isMajor = (iy * mult) % majorEvery == 0;
            (isMajor ? major : minor).append(QLineF(rect.left(), iy * step, rect.right(), iy * step));
        }
        QPen pen(pal.gridMinor, 1.0);
        pen.setCosmetic(true);
        p->setPen(pen);
        p->drawLines(minor.constData(), int(minor.size()));
        pen.setColor(pal.gridMajor);
        p->setPen(pen);
        p->drawLines(major.constData(), int(major.size()));
    }
}

void CircuitScene::drawForeground(QPainter *p, const QRectF &rect)
{
    const Theme &theme = Theme::instance();
    ensureConnectivity();

    const Palette &pal = theme.palette();
    const QRectF r = rect.adjusted(-8, -8, 8, 8);
    p->setRenderHint(QPainter::Antialiasing, true);

    if (theme.showJunctions()) {
        QPen ring(pal.openPin, 1.6);
        for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
            const QPointF pt(it.value().pt);
            if (!r.contains(pt))
                continue;
            const int n = it.value().count;
            if (n >= 3) {
                p->setPen(Qt::NoPen);
                p->setBrush(pal.junction);
                p->drawEllipse(pt, 4.0, 4.0);
            } else if (n == 1 && !m_nearPoints.contains(it.value().pt)) {
                p->setPen(ring);
                p->setBrush(Qt::NoBrush);
                p->drawEllipse(pt, 3.0, 3.0);
            }
        }
    }

    if (!m_nearPoints.isEmpty()) {                       // ярко-красные близкие концы
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(pal.nearMiss, 2.4));
        for (const QPoint &pt : std::as_const(m_nearPoints)) {
            if (!r.contains(QPointF(pt)))
                continue;
            p->drawEllipse(QPointF(pt), 5.5, 5.5);
        }
    }
}
