#include "sim/Preflight.h"

#include <QHash>
#include <QStringList>
#include <numeric>
#include <vector>

#include "canvas/CircuitScene.h"
#include "items/CircuitItem.h"

namespace Preflight {

namespace {

qint64 key(const QPoint &p)
{
    return (qint64(p.x()) << 32) ^ qint64(quint32(p.y()));
}

bool isResistor(ElementType t)
{
    switch (t) {
    case ElementType::Resistor:
    case ElementType::VariableResistor:
    case ElementType::Potentiometer:
    case ElementType::Photoresistor:
    case ElementType::Thermistor:
        return true;
    default:
        return false;
    }
}

bool isGround(ElementType t)
{
    return t == ElementType::Ground || t == ElementType::Chassis;
}

// Узлы схемы: точки выводов, соединённые проводами. Выводы разных элементов в одной точке — один узел.
class Nets
{
public:
    int idOf(const QPoint &p)
    {
        const qint64 k = key(p);
        auto it = m_index.constFind(k);
        if (it != m_index.constEnd())
            return it.value();
        const int id = int(m_parent.size());
        m_index.insert(k, id);
        m_parent.push_back(id);
        return id;
    }
    int find(int x)
    {
        while (m_parent[size_t(x)] != x) {
            m_parent[size_t(x)] = m_parent[size_t(m_parent[size_t(x)])];
            x = m_parent[size_t(x)];
        }
        return x;
    }
    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b)
            m_parent[size_t(a)] = b;
    }

private:
    QHash<qint64, int> m_index;
    std::vector<int> m_parent;
};

struct PinRef {
    CircuitItem *item = nullptr;
    int pin = 0;
};

} // namespace

bool needsSeriesResistor(ElementType type)
{
    switch (type) {
    case ElementType::LED:
    case ElementType::LedRGB:
    case ElementType::SegDisplay:
    case ElementType::LedMatrix:
        return true;
    default:
        return false;
    }
}

QVector<SimIssue> check(CircuitScene *scene)
{
    QVector<SimIssue> issues;
    if (!scene)
        return issues;

    const QList<CircuitItem *> items = scene->circuitItems();

    // 1. Узлы: провода сливают свои концы в один узел.
    Nets nets;
    QVector<CircuitItem *> parts;                              // всё, кроме проводов и оформления
    for (CircuitItem *it : items) {
        if (it->isPreview() || it->isDecoration())
            continue;
        if (it->isWire()) {
            nets.unite(nets.idOf(it->p1()), nets.idOf(it->p2()));
        } else {
            parts.append(it);
        }
    }

    // 2. Что подключено к каждому узлу.
    QHash<int, QVector<PinRef>> pinsOfNet;
    for (CircuitItem *it : std::as_const(parts)) {
        const QVector<QPoint> pts = it->connectionPoints();
        for (int i = 0; i < pts.size(); ++i)
            pinsOfNet[nets.find(nets.idOf(pts[i]))].append({ it, i });
    }

    // 3. Правило индикаторов.
    for (CircuitItem *it : std::as_const(parts)) {
        if (!needsSeriesResistor(it->elementType()))
            continue;

        const QVector<QPoint> pts = it->connectionPoints();
        const govno::core::ElementInfo &info = govno::core::elementInfo(it->elementType());
        const std::vector<std::string> names = govno::core::pinNamesFor(info, it->variant());

        QVector<int> bad;
        for (int i = 0; i < pts.size(); ++i) {
            const QVector<PinRef> &net = pinsOfNet.value(nets.find(nets.idOf(pts[i])));
            bool anyOther = false, ok = false;
            for (const PinRef &r : net) {
                if (r.item == it)
                    continue;                                  // соседние выводы самого индикатора не считаются
                anyOther = true;
                if (isResistor(r.item->elementType()) || isGround(r.item->elementType()))
                    ok = true;
            }
            if (anyOther && !ok)
                bad.append(i);                                 // подключён, но резистора (или земли) рядом нет
        }
        if (bad.isEmpty())
            continue;

        QStringList pinNames;
        for (int i : std::as_const(bad)) {
            if (pinNames.size() >= 6) {
                pinNames << QStringLiteral("…");
                break;
            }
            pinNames << (i < int(names.size()) ? QString::fromUtf8(names[size_t(i)].c_str()) : QString::number(i + 1));
        }

        SimIssue issue;
        issue.severity = SimIssue::Severity::Error;
        issue.code = QStringLiteral("no_series_resistor");
        issue.itemUid = it->uid();
        issue.pins = bad;
        issue.text = QStringLiteral("%1: %2 «%3» подключён без резистора")
                         .arg(it->displayName(),
                              bad.size() == 1 ? QStringLiteral("вывод") : QStringLiteral("выводы"),
                              pinNames.join(QStringLiteral(", ")));
        issue.hint = QStringLiteral("Поставьте резистор последовательно с каждым подключённым выводом индикатора "
                                    "(выводы, соединённые с землёй, резистора не требуют). Без него ток не ограничен.");
        issues.append(issue);
    }
    return issues;
}

} // namespace Preflight
