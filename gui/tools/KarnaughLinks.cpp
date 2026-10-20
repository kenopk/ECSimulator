#include "tools/KarnaughLinks.h"

#include <QMap>
#include <algorithm>
#include <cmath>

#include "canvas/CircuitScene.h"
#include "canvas/CircuitView.h"
#include "items/CircuitItem.h"
#include "project/ProjectFile.h"
#include "tools/KarnaughWidget.h"

namespace {

constexpr int kStride = CircuitItem::kKarnaughStride;

int clampSide(double v) { return std::clamp(int(std::lround(v)), 1, kStride); }

} // namespace

KarnaughLinks &KarnaughLinks::instance()
{
    static KarnaughLinks links;
    return links;
}

KarnaughLinks::KarnaughLinks()
{
    // Карту переименовали — связанные с ней элементы переходят на новое имя.
    connect(&ProjectHub::instance(), &ProjectHub::pageRenamed, this,
            [this](QWidget *page, const QString &oldName, const QString &newName) {
        const int project = ProjectHub::projectOf(page);
        for (CircuitItem *it : linkedElements(project, oldName))
            it->setLink(newName);
    });
}

CircuitView *KarnaughLinks::viewOf(const CircuitItem *item)
{
    if (!item || !item->scene())
        return nullptr;
    return qobject_cast<CircuitView *>(item->scene()->parent());
}

void KarnaughLinks::attachView(CircuitView *view)
{
    CircuitScene *scene = view->circuitScene();
    connect(scene, &CircuitScene::karnaughChanged, this, [this](CircuitItem *item) { onElementChanged(item); });
    connect(scene, &CircuitScene::karnaughAttached, this, [this](CircuitItem *item) { pull(item); });
}

void KarnaughLinks::attachMap(KarnaughWidget *map)
{
    connect(map, &KarnaughWidget::stateChanged, this, [this, map] { onMapChanged(map); });
}

QStringList KarnaughLinks::mapNames(int projectId) const
{
    QStringList out;
    for (QWidget *w : ProjectHub::instance().pagesOf(projectId))
        if (ProjectHub::kindOf(w) == ProjectHub::Map)
            out << ProjectHub::nameOf(w);
    return out;
}

KarnaughWidget *KarnaughLinks::mapOf(int projectId, const QString &name) const
{
    if (name.isEmpty())
        return nullptr;
    for (QWidget *w : ProjectHub::instance().pagesOf(projectId))
        if (ProjectHub::kindOf(w) == ProjectHub::Map && ProjectHub::nameOf(w) == name)
            return qobject_cast<KarnaughWidget *>(w);
    return nullptr;
}

QVector<CircuitItem *> KarnaughLinks::linkedElements(int projectId, const QString &name, const CircuitItem *except) const
{
    QVector<CircuitItem *> out;
    if (name.isEmpty())
        return out;
    for (QWidget *w : ProjectHub::instance().pagesOf(projectId)) {
        auto *view = qobject_cast<CircuitView *>(w);
        if (!view)
            continue;
        for (CircuitItem *it : view->circuitScene()->circuitItems())
            if (it != except && it->isKarnaugh() && !it->isPreview() && it->link() == name)
                out.append(it);
    }
    return out;
}

KarnaughLinks::ElementState KarnaughLinks::captureElement(const CircuitItem *item)
{
    ElementState s;
    s.width = item->param(QStringLiteral("width"));
    s.height = item->param(QStringLiteral("height"));
    s.cells = item->cells();
    s.names = item->varNames();
    return s;
}

void KarnaughLinks::restoreElement(CircuitItem *item, const ElementState &state)
{
    ++m_busy;
    QMap<QString, double> p;
    p.insert(QStringLiteral("width"), state.width);
    p.insert(QStringLiteral("height"), state.height);
    item->setParams(p);
    item->setCells(state.cells);
    item->setVarNames(state.names);
    --m_busy;
}

// Карта → элемент: размер (до 16×16), клетки видимой части и имена переменных.
void KarnaughLinks::applyToElement(CircuitItem *item, KarnaughWidget *map)
{
    const KarnaughWidget::State st = map->state();
    const int ew = std::min(st.width, kStride), eh = std::min(st.height, kStride);

    ++m_busy;
    if (clampSide(item->param(QStringLiteral("width"))) != ew || clampSide(item->param(QStringLiteral("height"))) != eh) {
        QMap<QString, double> p;
        p.insert(QStringLiteral("width"), ew);
        p.insert(QStringLiteral("height"), eh);
        item->setParams(p);
    }
    QVector<quint8> cells = item->cells();
    for (int r = 0; r < eh; ++r)
        for (int c = 0; c < ew; ++c)
            cells[r * kStride + c] = st.cells.value(r * st.width + c, quint8(0));
    item->setCells(cells);
    item->setVarNames(map->names());
    --m_busy;
}

bool KarnaughLinks::pull(CircuitItem *item)
{
    CircuitView *view = viewOf(item);
    if (!view || !item->isKarnaugh() || item->link().isEmpty())
        return false;
    KarnaughWidget *map = mapOf(ProjectHub::projectOf(view), item->link());
    if (!map)
        return false;
    applyToElement(item, map);
    return true;
}

void KarnaughLinks::syncProject(int projectId)
{
    for (QWidget *w : ProjectHub::instance().pagesOf(projectId)) {
        auto *view = qobject_cast<CircuitView *>(w);
        if (!view)
            continue;
        for (CircuitItem *it : view->circuitScene()->circuitItems())
            if (it->isKarnaugh() && !it->isPreview() && !it->link().isEmpty())
                if (KarnaughWidget *map = mapOf(projectId, it->link()))
                    applyToElement(it, map);
    }
}

void KarnaughLinks::onMapChanged(KarnaughWidget *map)
{
    if (m_busy || !map)
        return;
    const int project = ProjectHub::projectOf(map);
    for (CircuitItem *it : linkedElements(project, ProjectHub::nameOf(map)))
        applyToElement(it, map);
}

// Элемент → карта. Правка элемента попадает в карту, а из карты — на остальные элементы с той же связью.
void KarnaughLinks::onElementChanged(CircuitItem *item)
{
    if (m_busy || !item || item->link().isEmpty() || item->isPreview())
        return;
    CircuitView *view = viewOf(item);
    if (!view)
        return;
    const int project = ProjectHub::projectOf(view);
    KarnaughWidget *map = mapOf(project, item->link());
    if (!map)
        return;

    KarnaughWidget::State st = map->state();
    const int ow = st.width, oh = st.height;
    const int ew = clampSide(item->param(QStringLiteral("width"))), eh = clampSide(item->param(QStringLiteral("height")));
    int nw = ow, nh = oh;
    if (ow <= kStride && oh <= kStride) {            // небольшую карту размер элемента меняет и в карте
        nw = ew;
        nh = eh;
    }
    QVector<quint8> cells(nw * nh, quint8(0));
    for (int r = 0; r < std::min(oh, nh); ++r)
        for (int c = 0; c < std::min(ow, nw); ++c)
            cells[r * nw + c] = st.cells.value(r * ow + c, quint8(0));
    const QVector<quint8> &ec = item->cells();
    for (int r = 0; r < std::min(nh, eh); ++r)
        for (int c = 0; c < std::min(nw, ew); ++c)
            cells[r * nw + c] = ec.value(r * kStride + c, quint8(0));

    if (nw == ow && nh == oh && cells == st.cells)
        return;                                       // карта уже такая же
    st.width = nw;
    st.height = nh;
    st.cells = cells;

    ++m_busy;
    map->setState(st);
    --m_busy;
    for (CircuitItem *other : linkedElements(project, item->link(), item))
        applyToElement(other, map);
    applyToElement(item, map);                        // у самого элемента могли поменяться имена переменных (число переменных)
}
