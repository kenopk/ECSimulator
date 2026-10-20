#include "canvas/CircuitView.h"

#include "items/NoteItem.h"
#include "tools/KarnaughLinks.h"
#include "scope/ScopeItem.h"
#include "sim/IssueBanner.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRubberBand>
#include <QScrollBar>
#include <QUndoStack>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "canvas/CircuitScene.h"
#include "commands/Commands.h"
#include "items/CircuitItem.h"
#include "items/SymbolPainter.h"

namespace {

constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 6.0;

QVector<ItemData> g_clipboard;   // общий для всех вкладок процесса

} // namespace

// ============================================================ конструктор

CircuitView::CircuitView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new CircuitScene(this);
    setScene(m_scene);
    m_banner = new IssueBanner(this);
    connect(m_banner, &IssueBanner::showClicked, this, &CircuitView::bannerShowRequested);
    connect(m_banner, &IssueBanner::closeClicked, this, &CircuitView::hideStopBanner);
    m_undo = new QUndoStack(this);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setFrameShape(QFrame::NoFrame);
    setAcceptDrops(false);              // перетаскиваемую вкладку принимает область вкладок, а не холст
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    centerOn(0, 0);

    m_rubber = new QRubberBand(QRubberBand::Rectangle, viewport());
    KarnaughLinks::instance().attachView(this);              // элементы «Карта Карно» связываются с картами проекта

    connect(m_scene, &QGraphicsScene::selectionChanged, this, &CircuitView::selectionChanged);
    connect(m_undo, &QUndoStack::indexChanged, this, [this] {
        if (m_drag != Drag::None)
            cancelOperation();       // отмена/повтор посреди перетаскивания
        dropHover();
    });
    connect(&m_flowTimer, &QTimer::timeout, this, [this] {
        m_scene->tickFlow(m_flowClock.restart() / 1000.0);
    });
}

CircuitView::~CircuitView()
{
    // При разрушении сцена снимает выделение и шлёт сигналы — отрезаем всех
    // слушателей, чтобы они не обращались к наполовину разрушенному виду.
    // Стек отмены — дочерний объект и очищается уже внутри ~QWidget: его сигналы
    // тоже отключаем, иначе обработчики полезут в полуразрушенный вид и сцену.
    QObject::disconnect(m_scene, nullptr, nullptr, nullptr);
    QObject::disconnect(m_undo, nullptr, nullptr, nullptr);
    QObject::disconnect(this, nullptr, nullptr, nullptr);
    m_flowTimer.stop();
}

// ============================================================== режимы

void CircuitView::setSelectMode()
{
    cancelOperation();
    if (!m_selectMode) {
        m_selectMode = true;
        emit modeChanged();
    }
    updateCursor();
}

void CircuitView::setPlaceMode(ElementType type, bool routed)
{
    cancelOperation();
    m_scene->clearSelection();
    dropHover();
    routed = routed && type == ElementType::Wire;
    const bool changed = m_selectMode || m_placeType != type || m_routedWire != routed;
    m_selectMode = false;
    m_placeType = type;
    m_routedWire = routed;
    if (changed)
        emit modeChanged();
    updateCursor();
}

void CircuitView::escape()
{
    if (m_drag != Drag::None) {
        cancelOperation();
        return;
    }
    if (!m_selectMode) {
        setSelectMode();
        return;
    }
    m_scene->clearSelection();
}

// ============================================================== правка

QList<CircuitItem *> CircuitView::selectedCircuitItems() const
{
    QList<CircuitItem *> out;
    const QList<QGraphicsItem *> sel = m_scene->selectedItems();
    out.reserve(sel.size());
    for (QGraphicsItem *gi : sel)
        if (auto *ci = qgraphicsitem_cast<CircuitItem *>(gi))
            out.append(ci);
    return out;
}

CircuitItem *CircuitView::singleSelected() const
{
    const QList<CircuitItem *> sel = selectedCircuitItems();
    return sel.size() == 1 ? sel.first() : nullptr;
}

bool CircuitView::hasSelection() const { return !m_scene->selectedItems().isEmpty(); }

bool CircuitView::selectionHasToggle() const
{
    for (CircuitItem *it : selectedCircuitItems())
        if (it->isToggle())
            return true;
    return false;
}

bool CircuitView::clipboardHasData() { return !g_clipboard.isEmpty(); }

void CircuitView::selectAll()
{
    for (CircuitItem *ci : m_scene->circuitItems())
        if (!ci->isPreview())
            ci->setSelected(true);
}

CircuitItem *CircuitView::itemByUid(quint64 uid) const
{
    if (!uid)
        return nullptr;
    for (CircuitItem *it : m_scene->circuitItems())
        if (it->uid() == uid)
            return it;
    return nullptr;
}

void CircuitView::focusItem(quint64 uid, const QVector<int> &)
{
    CircuitItem *it = itemByUid(uid);
    if (!it)
        return;
    m_scene->clearSelection();
    it->setSelected(true);
    centerOn(QPointF(it->p1() + it->p2()) / 2.0);
}

void CircuitView::markProblems(const QVector<SimIssue> &issues)
{
    clearProblems();
    for (const SimIssue &issue : issues) {
        CircuitItem *it = itemByUid(issue.itemUid);
        if (!it)
            continue;
        quint64 mask = 0;
        for (int pin : issue.pins)
            if (pin >= 0 && pin < 64)
                mask |= quint64(1) << pin;
        it->setProblem(mask, issue.pins.isEmpty());
    }
}

void CircuitView::clearProblems()
{
    for (CircuitItem *it : m_scene->circuitItems())
        if (it->hasProblem())
            it->clearProblem();
}

void CircuitView::showStopBanner(const QString &title, const QString &message)
{
    m_banner->setContent(title, message);
    m_banner->show();
    layoutBanner();
    m_banner->raise();
}

void CircuitView::hideStopBanner()
{
    m_banner->hide();
}

void CircuitView::layoutBanner()
{
    if (!m_banner || !m_banner->isVisible())
        return;
    const int w = std::max(240, std::min(width() - 32, 780));
    m_banner->setFixedWidth(w);
    m_banner->adjustSize();
    m_banner->move((width() - w) / 2, 14);
}

void CircuitView::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    layoutBanner();
}

ScopeItem *CircuitView::scopeAt(const QPointF &scenePos) const
{
    for (QGraphicsItem *gi : m_scene->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder))
        if (gi->type() == ScopeItem::Type)
            return static_cast<ScopeItem *>(gi);
    return nullptr;
}

NoteItem *CircuitView::noteAt(const QPointF &scenePos) const
{
    for (QGraphicsItem *gi : m_scene->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder))
        if (gi->type() == NoteItem::Type)
            return static_cast<NoteItem *>(gi);
    return nullptr;
}

void CircuitView::refreshHoverTip()
{
    viewport()->setToolTip(m_hoverItem ? m_hoverItem->hoverTip() : QString());
}

void CircuitView::setItemNote(CircuitItem *item, const WireNote &note, const QString &undoText, bool mergeable)
{
    if (!item || item->note() == note)
        return;
    m_undo->push(new ChangeNoteCommand(item, item->note(), note, undoText, mergeable));
    refreshHoverTip();
}

void CircuitView::setItemLink(CircuitItem *item, const QString &mapName)
{
    if (!item || !item->isKarnaugh() || item->link() == mapName)
        return;
    m_undo->push(new ChangeLinkCommand(item, mapName));
}

void CircuitView::setMmPerCell(double mm)
{
    if (!(mm > 0.0) || qFuzzyCompare(mm, m_scene->mmPerCell()))
        return;
    m_undo->push(new ChangeScaleCommand(m_scene, m_scene->mmPerCell(), mm));
    refreshHoverTip();
}

ScopeItem *CircuitView::addScope(CircuitItem *target)
{
    if (!target)
        return nullptr;
    ScopeSource *src = target->probe(true);
    if (!src)
        return nullptr;

    // Ставим справа-сверху от элемента; каждая следующая осциллограмма того же элемента — чуть ниже.
    int sameTarget = 0;
    for (QGraphicsItem *gi : m_scene->items())
        if (gi->type() == ScopeItem::Type && static_cast<ScopeItem *>(gi)->source() == src)
            ++sameTarget;
    const QPointF center = QPointF(target->p1() + target->p2()) / 2.0;
    const QPoint pos = CircuitScene::snap(center + QPointF(48, -ScopeItem::kHeight - 32 + sameTarget * 32));

    auto *item = new ScopeItem(src);
    item->setPos(pos);
    m_scene->addItem(item);
    m_scene->clearSelection();
    item->setSelected(true);
    return item;
}

void CircuitView::removeScope(ScopeItem *item)
{
    if (!item)
        return;
    if (m_scopeDrag == item)
        m_scopeDrag = nullptr;
    m_scene->removeItem(item);
    delete item;
}

void CircuitView::deleteSelected()
{
    // Осциллограммы на схеме удаляются сразу (в историю отмены они не входят).
    const QList<QGraphicsItem *> selected = m_scene->selectedItems();
    for (QGraphicsItem *gi : selected) {
        if (gi->type() == ScopeItem::Type) {
            m_scene->removeItem(gi);
            delete gi;
        }
    }
    // Выделенные блоки комментариев не удаляются вместе с текстом: блок только скрывается (текст остаётся у провода).
    QList<CircuitItem *> hideNotes;
    for (QGraphicsItem *gi : selected)
        if (gi->type() == NoteItem::Type) {
            CircuitItem *owner = static_cast<NoteItem *>(gi)->owner();
            if (owner && !owner->isSelected() && !hideNotes.contains(owner))
                hideNotes.append(owner);
        }
    const QList<CircuitItem *> sel = selectedCircuitItems();
    if (sel.isEmpty() && hideNotes.isEmpty())
        return;
    if (!hideNotes.isEmpty()) {
        m_undo->beginMacro(QStringLiteral("Удаление"));
        for (CircuitItem *w : std::as_const(hideNotes)) {
            WireNote n = w->note();
            n.shown = false;
            m_undo->push(new ChangeNoteCommand(w, w->note(), n, QStringLiteral("Скрыть блок комментария")));
        }
        if (!sel.isEmpty())
            m_undo->push(new RemoveItemsCommand(m_scene, sel, QStringLiteral("Удаление")));
        m_undo->endMacro();
        return;
    }
    m_undo->push(new RemoveItemsCommand(m_scene, sel, QStringLiteral("Удаление")));
}

void CircuitView::clearAll()
{
    for (QGraphicsItem *gi : m_scene->items()) {
        if (gi->type() == ScopeItem::Type) {
            m_scene->removeItem(gi);
            delete gi;
        }
    }
    QList<CircuitItem *> all;
    for (CircuitItem *ci : m_scene->circuitItems())
        if (!ci->isPreview())
            all.append(ci);
    if (all.isEmpty())
        return;
    m_undo->push(new RemoveItemsCommand(m_scene, all, QStringLiteral("Очистка схемы")));
}

QVector<ItemData> CircuitView::snapshot(bool selectedOnly) const
{
    QVector<ItemData> out;
    const QList<CircuitItem *> items = selectedOnly ? selectedCircuitItems() : m_scene->circuitItems();
    out.reserve(items.size());
    for (CircuitItem *it : items) {
        if (it->isPreview())
            continue;
        ItemData d;
        d.type = it->elementType();
        d.a = it->p1();
        d.b = it->p2();
        d.flip = it->flipped();
        d.params = it->params();
        d.text = it->text();
        if (it->isKarnaugh())
            d.cells = it->cells();
        if (it->isWire())
            d.note = it->note();
        if (it->isKarnaugh())
            d.link = it->link();
        out.append(d);
    }
    return out;
}

QList<CircuitItem *> CircuitView::addItems(const QVector<ItemData> &data, const QPoint &offset,
                                           const QString &undoText, bool selectNew)
{
    QList<CircuitItem *> created;
    created.reserve(data.size());
    for (const ItemData &d : data) {
        auto *it = new CircuitItem(d.type);
        it->setParams(d.params);
        it->setText(d.text);
        if (!d.cells.isEmpty())
            it->setCells(d.cells);
        it->setGeometry(d.a + offset, d.b + offset, d.flip);
        if (it->isWire() && d.note != WireNote())
            it->setNote(d.note);
        if (it->isKarnaugh())
            it->setLink(d.link);
        created.append(it);
    }
    if (created.isEmpty())
        return created;

    if (selectNew)
        m_scene->clearSelection();
    m_undo->push(new AddItemsCommand(m_scene, created, undoText));
    if (selectNew)
        for (CircuitItem *it : created)
            it->setSelected(true);
    return created;
}

void CircuitView::loadSnapshot(const QVector<ItemData> &data, const QString &undoText)
{
    addItems(data, QPoint(0, 0), undoText, false);
}

void CircuitView::copySelected()
{
    QVector<ItemData> data = snapshot(true);
    if (data.isEmpty())
        return;

    int minX = INT_MAX, minY = INT_MAX;
    for (const ItemData &d : std::as_const(data)) {
        minX = std::min({ minX, d.a.x(), d.b.x() });
        minY = std::min({ minY, d.a.y(), d.b.y() });
    }
    const QPoint anchor(minX, minY);
    for (ItemData &d : data) {
        d.a -= anchor;
        d.b -= anchor;
    }
    g_clipboard = data;

    m_lastPasteTarget = QPoint(INT_MIN, INT_MIN);
    m_pasteCount = 0;
}

void CircuitView::cutSelected()
{
    copySelected();
    deleteSelected();
}

void CircuitView::pasteClipboard()
{
    if (g_clipboard.isEmpty())
        return;

    const QPoint cursorPos = viewport()->mapFromGlobal(QCursor::pos());
    const QPoint viewPoint = viewport()->rect().contains(cursorPos) ? cursorPos
                                                                    : viewport()->rect().center();
    const QPoint target = CircuitScene::snap(mapToScene(viewPoint));

    if (target == m_lastPasteTarget) {
        ++m_pasteCount;
    } else {
        m_pasteCount = 0;
        m_lastPasteTarget = target;
    }
    const QPoint offset = target + QPoint(2 * CircuitScene::kGrid, 2 * CircuitScene::kGrid) * m_pasteCount;
    addItems(g_clipboard, offset, QStringLiteral("Вставка"), true);
}

void CircuitView::setItemParam(CircuitItem *item, const QString &key, double value)
{
    const double old = item->param(key);
    if (old == value)
        return;
    m_undo->push(new ChangeParamCommand(item, key, old, value));
}

void CircuitView::setItemText(CircuitItem *item, const QString &text)
{
    if (item->text() == text)
        return;
    m_undo->push(new ChangeTextCommand(item, item->text(), text));
}

void CircuitView::setItemParams(CircuitItem *item, const QMap<QString, double> &values)
{
    QVector<ChangeParamCommand *> cmds;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const double old = item->param(it.key());
        if (old != it.value())
            cmds.append(new ChangeParamCommand(item, it.key(), old, it.value()));
    }
    if (cmds.isEmpty())
        return;
    if (cmds.size() == 1) {
        m_undo->push(cmds.first());
        return;
    }
    m_undo->beginMacro(QStringLiteral("Параметры: ") + item->displayName().toLower());
    for (ChangeParamCommand *c : std::as_const(cmds))
        m_undo->push(c);
    m_undo->endMacro();
}

void CircuitView::toggleDipSwitch(CircuitItem *item, int position)
{
    if (item && item->isKarnaugh()) {                        // клик по клетке карты: 0 → 1 → X → 0
        const int cols = std::clamp(int(std::lround(item->param(QStringLiteral("width")))), 1, CircuitItem::kKarnaughStride);
        if (position < 1)
            return;
        const int idx = ((position - 1) / cols) * CircuitItem::kKarnaughStride + (position - 1) % cols;
        QVector<quint8> cells = item->cells();
        if (idx < 0 || idx >= cells.size())
            return;
        cells[idx] = quint8((cells[idx] + 1) % 3);
        m_undo->push(new ChangeCellsCommand(item, item->cells(), cells));
        return;
    }
    if (!item || position < 1 || position > 32)
        return;
    const double old = item->param(QStringLiteral("mask"));
    const quint32 mask = quint32(std::llround(old)) ^ (1u << (position - 1));
    m_undo->beginMacro(QStringLiteral("Переключатель DIP"));
    m_undo->push(new ChangeParamCommand(item, QStringLiteral("mask"), old, double(mask)));
    m_undo->endMacro();
}

void CircuitView::releaseHeldKey()
{
    CircuitItem *k = m_heldKeypad;
    m_heldKeypad = nullptr;
    if (k)
        k->setParam(QStringLiteral("key"), 0.0);
}

void CircuitView::toggleSelectedSwitches()
{
    toggleItems(selectedCircuitItems());
}

void CircuitView::toggleItems(const QList<CircuitItem *> &items)
{
    QVector<ChangeParamCommand *> cmds;
    for (CircuitItem *it : items) {
        if (!it->isToggle())
            continue;
        const QString key = it->toggleKey();
        const double old = it->param(key);
        cmds.append(new ChangeParamCommand(it, key, old, it->toggledValue()));
    }
    if (cmds.isEmpty())
        return;
    m_undo->beginMacro(QStringLiteral("Переключение"));
    for (ChangeParamCommand *c : std::as_const(cmds))
        m_undo->push(c);
    m_undo->endMacro();
}

void CircuitView::moveSelection(const QPoint &delta)
{
    const QList<CircuitItem *> sel = selectedCircuitItems();
    if (sel.isEmpty())
        return;
    QVector<GeometryEntry> entries;
    entries.reserve(sel.size());
    for (CircuitItem *it : sel) {
        GeometryEntry e;
        e.item = it;
        e.oldA = it->p1();
        e.oldB = it->p2();
        e.newA = e.oldA + delta;
        e.newB = e.oldB + delta;
        e.oldFlip = e.newFlip = it->flipped();
        entries.append(e);
    }
    m_undo->push(new GeometryCommand(entries, QStringLiteral("Перемещение"), true));
}

void CircuitView::rotateSelection(bool clockwise)
{
    const QList<CircuitItem *> sel = selectedCircuitItems();
    if (sel.isEmpty())
        return;

    // Один элемент вращается вокруг своего первого вывода (он остаётся на месте и не
    // отрывается от проводов); группа — вокруг центра охватывающего прямоугольника.
    QPoint pivot;
    if (sel.size() == 1) {
        pivot = sel.first()->p1();
    } else {
        int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
        for (CircuitItem *it : sel) {
            for (const QPoint &p : { it->p1(), it->p2() }) {
                minX = std::min(minX, p.x());
                maxX = std::max(maxX, p.x());
                minY = std::min(minY, p.y());
                maxY = std::max(maxY, p.y());
            }
        }
        pivot = CircuitScene::snap(QPointF((minX + maxX) / 2.0, (minY + maxY) / 2.0));
    }

    const auto rot = [&](const QPoint &p) {
        const QPoint d = p - pivot;
        return clockwise ? pivot + QPoint(-d.y(), d.x())
                         : pivot + QPoint(d.y(), -d.x());
    };

    QVector<GeometryEntry> entries;
    entries.reserve(sel.size());
    for (CircuitItem *it : sel) {
        GeometryEntry e;
        e.item = it;
        e.oldA = it->p1();
        e.oldB = it->p2();
        e.newA = rot(e.oldA);
        e.newB = rot(e.oldB);
        e.oldFlip = e.newFlip = it->flipped();
        entries.append(e);
    }
    m_undo->push(new GeometryCommand(entries, QStringLiteral("Поворот")));
}

void CircuitView::flipSelection()
{
    QVector<GeometryEntry> entries;
    for (CircuitItem *it : selectedCircuitItems()) {
        if (it->isWire() || it->isSinglePin())
            continue;
        GeometryEntry e;
        e.item = it;
        e.oldA = e.newA = it->p1();
        e.oldB = e.newB = it->p2();
        e.oldFlip = it->flipped();
        e.newFlip = !e.oldFlip;
        entries.append(e);
    }
    if (entries.isEmpty())
        return;
    m_undo->push(new GeometryCommand(entries, QStringLiteral("Отражение")));
}

// ================================================================== вид

qreal CircuitView::zoom() const { return transform().m11(); }

void CircuitView::applyScale(qreal factor, const QPoint &anchorViewPos)
{
    const qreal cur = zoom();
    const qreal target = std::clamp(cur * factor, kMinZoom, kMaxZoom);
    const qreal real = target / cur;
    if (qFuzzyCompare(real, 1.0))
        return;

    // Точка сцены под курсором должна остаться под курсором. Свою привязку делаем вручную:
    // встроенная AnchorUnderMouse берёт устаревшую позицию мыши и «телепортирует» вид.
    const QPointF before = mapToScene(anchorViewPos);
    const ViewportAnchor old = transformationAnchor();
    setTransformationAnchor(QGraphicsView::NoAnchor);
    scale(real, real);
    const QPointF after = mapToScene(anchorViewPos);
    const QPointF d = after - before;
    translate(d.x(), d.y());
    setTransformationAnchor(old);
    emit zoomChanged(zoom());
}

void CircuitView::zoomIn()  { applyScale(1.25, viewport()->rect().center()); }
void CircuitView::zoomOut() { applyScale(0.8, viewport()->rect().center()); }

void CircuitView::resetView()
{
    resetTransform();
    const QRectF r = m_scene->contentRect();
    centerOn(r.isNull() ? QPointF(0, 0) : r.center());
    emit zoomChanged(zoom());
}

void CircuitView::fitToContent()
{
    QRectF r = m_scene->contentRect();
    if (r.isNull()) {
        resetView();
        return;
    }
    r.adjust(-96, -96, 96, 96);
    fitInView(r, Qt::KeepAspectRatio);
    const qreal z = zoom();
    const qreal clamped = std::clamp(z, kMinZoom, 3.0);
    if (!qFuzzyCompare(z, clamped))
        scale(clamped / z, clamped / z);
    centerOn(r.center());
    emit zoomChanged(zoom());
}

void CircuitView::setFlowAnimation(bool on)
{
    if (on) {
        m_flowClock.start();
        m_flowTimer.start(33);
    } else {
        m_flowTimer.stop();
    }
}

void CircuitView::resetSimulationState()
{
    m_scene->clearElectricalState();
}

// ============================================================ поиск

qreal CircuitView::hitTolerance() const
{
    return std::clamp(8.0 / zoom(), 6.0, 40.0);
}

CircuitItem *CircuitView::hitItem(const QPointF &sp, int *endpoint) const
{
    const qreal tol = hitTolerance();
    const QRectF probe(sp.x() - tol, sp.y() - tol, 2 * tol, 2 * tol);
    const QList<QGraphicsItem *> candidates =
        m_scene->items(probe, Qt::IntersectsItemShape, Qt::DescendingOrder);

    CircuitItem *top = nullptr;
    CircuitItem *best = nullptr;
    int bestEp = -1;
    qreal bestDist = 1e18;

    for (QGraphicsItem *gi : candidates) {
        auto *ci = qgraphicsitem_cast<CircuitItem *>(gi);
        if (!ci || ci == m_preview || ci == m_preview2)
            continue;
        if (!top)
            top = ci;
        const int ep = ci->endpointAt(sp, tol);
        if (ep < 0)
            continue;
        const QPoint pt = ci->endpointPoint(ep);
        const qreal d = std::hypot(sp.x() - pt.x(), sp.y() - pt.y());
        if (d < bestDist) {
            bestDist = d;
            best = ci;
            bestEp = ep;
        }
    }

    if (best) {
        if (endpoint)
            *endpoint = bestEp;
        return best;
    }
    if (endpoint)
        *endpoint = -1;
    return top;
}

void CircuitView::updateHover(const QPointF &sp)
{
    int ep = -1;
    CircuitItem *it = m_selectMode ? hitItem(sp, &ep) : nullptr;
    if (it == m_hoverItem && ep == m_hoverEndpoint)
        return;
    dropHover();
    m_hoverItem = it;
    m_hoverEndpoint = ep;
    if (it)
        it->setHover(ep < 0, ep);
    refreshHoverTip();
    updateCursor();
}

void CircuitView::dropHover()
{
    if (m_hoverItem)
        m_hoverItem->setHover(false, -1);
    m_hoverItem = nullptr;
    m_hoverEndpoint = -1;
    refreshHoverTip();
    updateCursor();
}

void CircuitView::updateCursor()
{
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (m_drag == Drag::Pan)
        shape = Qt::ClosedHandCursor;
    else if (!m_selectMode)
        shape = Qt::CrossCursor;
    else if (m_drag == Drag::MoveItems || m_drag == Drag::MoveEndpoint)
        shape = Qt::SizeAllCursor;
    else if (m_hoverItem)
        shape = m_hoverEndpoint >= 0 ? Qt::CrossCursor : Qt::SizeAllCursor;
    viewport()->setCursor(shape);
}

// ============================================================ операции

void CircuitView::beginPan(const QPoint &viewPos)
{
    cancelOperation();
    m_drag = Drag::Pan;
    m_lastPanPos = viewPos;
    updateCursor();
}

void CircuitView::beginPlace(const QPoint &snapped)
{
    m_preview = new CircuitItem(m_placeType);
    m_preview->setPreview(true);
    m_preview->setEndpoints(snapped, snapped);
    m_scene->addItem(m_preview);
    if (m_routedWire && m_placeType == ElementType::Wire) {
        m_preview2 = new CircuitItem(ElementType::Wire);
        m_preview2->setPreview(true);
        m_preview2->setEndpoints(snapped, snapped);
        m_scene->addItem(m_preview2);
    }
    m_drag = Drag::Place;
    m_pressSnapped = snapped;
    updatePlace(snapped, false);
}

void CircuitView::updatePlace(const QPoint &snapped, bool shift)
{
    if (!m_preview)
        return;

    const QPoint a = m_pressSnapped;
    QPoint b = snapped;

    if (m_preview->isWire() && m_preview2) {          // трассированный: сначала по горизонтали, с Shift — по вертикали
        const QPoint corner = shift ? QPoint(a.x(), b.y()) : QPoint(b.x(), a.y());
        m_preview->setEndpoints(a, corner);
        m_preview2->setEndpoints(corner, b);
        return;
    }
    if (m_preview->isWire()) {
        if (shift) {
            const int dx = b.x() - a.x();
            const int dy = b.y() - a.y();
            if (std::abs(dx) >= std::abs(dy))
                b.setY(a.y());
            else
                b.setX(a.x());
        }
    } else if (b == a) {
        const int len = int(SymbolPainter::defaultLength(m_preview->elementType(), m_preview->variant()));
        b = a + SymbolPainter::defaultDirection(m_preview->elementType()) * len;   // земля — вниз, шины — вверх
    }
    // Для остальных случаев тип сам приводит b к допустимому (ось, длина, шаг,
    // направление земли), поэтому протяжка работает одинаково для любого элемента.
    m_preview->setEndpoints(a, b);
}

void CircuitView::finishPlace()
{
    CircuitItem *it = m_preview;
    CircuitItem *it2 = m_preview2;
    m_preview = nullptr;
    m_preview2 = nullptr;
    m_drag = Drag::None;
    if (!it)
        return;

    QList<CircuitItem *> made;
    for (CircuitItem *x : { it, it2 }) {
        if (!x)
            continue;
        if (x->isWire() && x->p1() == x->p2()) {     // клик без протяжки / нулевая половина — провод не создаём
            m_scene->removeItem(x);
            delete x;
        } else {
            made.append(x);
        }
    }
    if (made.isEmpty())
        return;
    const QString text = QStringLiteral("Добавление: ") + made.first()->displayName().toLower();
    m_undo->push(new AddItemsCommand(m_scene, made, text));
}

void CircuitView::beginMoveItems(const QPoint &snapped, bool withWires)
{
    m_entries.clear();
    QSet<CircuitItem *> moving;
    QVector<QPoint> movingPins;
    for (CircuitItem *it : selectedCircuitItems()) {
        m_entries.append({ it, -1, it->p1(), it->p2() });
        moving.insert(it);
        movingPins += it->connectionPoints();
    }
    if (m_entries.isEmpty())
        return;

    if (withWires) {
        // провода, чьи концы лежат на выводах переносимых элементов, растягиваются следом
        for (CircuitItem *ci : m_scene->circuitItems()) {
            if (!ci->isWire() || ci == m_preview || ci == m_preview2 || moving.contains(ci))
                continue;
            const bool c1 = movingPins.contains(ci->p1());
            const bool c2 = movingPins.contains(ci->p2());
            if (c1 && c2)
                m_entries.append({ ci, -1, ci->p1(), ci->p2() });
            else if (c1)
                m_entries.append({ ci, 0, ci->p1(), ci->p2() });
            else if (c2)
                m_entries.append({ ci, 1, ci->p1(), ci->p2() });
        }
    }

    m_drag = Drag::MoveItems;
    m_pressSnapped = snapped;
    updateCursor();
}

void CircuitView::beginEndpointDrag(CircuitItem *item, int endpoint)
{
    if (!item->isSelected()) {
        m_scene->clearSelection();
        item->setSelected(true);
    }

    m_entries.clear();
    const QPoint pt = item->endpointPoint(endpoint);

    if (item->isSinglePin() && endpoint == 1) {
        m_entries.append({ item, 1, item->p1(), item->p2() });     // конец земли: растянуть / повернуть
    } else {
        // узел: вместе с выбранным концом тянутся все концы в этой же точке
        const QRectF probe(pt.x() - 2, pt.y() - 2, 4, 4);
        const QList<QGraphicsItem *> near = m_scene->items(probe, Qt::IntersectsItemShape);
        for (QGraphicsItem *gi : near) {
            auto *ci = qgraphicsitem_cast<CircuitItem *>(gi);
            if (!ci || ci == m_preview || ci == m_preview2 || ci->isMultiPin())
                continue;
            const QVector<QPoint> pts = ci->connectionPoints();
            for (int i = 0; i < pts.size(); ++i) {
                if (pts[i] == pt) {
                    m_entries.append({ ci, i, ci->p1(), ci->p2() });
                    break;
                }
            }
        }
    }
    if (m_entries.isEmpty())
        return;
    m_drag = Drag::MoveEndpoint;
    m_pressSnapped = pt;
    updateCursor();
}

void CircuitView::applyMove(const QPoint &delta)
{
    for (const DragEntry &e : std::as_const(m_entries)) {
        if (e.endpoint < 0)
            e.item->setEndpoints(e.oldA + delta, e.oldB + delta);
        else
            e.item->setEndpoint(e.endpoint, (e.endpoint == 0 ? e.oldA : e.oldB) + delta);
    }
}

void CircuitView::applyEndpoint(const QPoint &pt)
{
    // Один элемент тянут свободно: тип сам приводит конец к допустимому (ось, длина, шаг).
    // Если вместе с узлом едут несколько элементов, «ломать» геометрию нельзя —
    // иначе концы разойдутся, поэтому тогда движение допустимо только целиком.
    if (m_entries.size() > 1) {
        for (const DragEntry &e : std::as_const(m_entries))
            if (!e.item->acceptsEndpoint(e.endpoint, pt))
                return;
    }
    for (const DragEntry &e : std::as_const(m_entries))
        e.item->setEndpoint(e.endpoint, pt);
}

void CircuitView::finishMove(const QString &text)
{
    QVector<GeometryEntry> changed;
    for (const DragEntry &e : std::as_const(m_entries)) {
        if (e.item->p1() == e.oldA && e.item->p2() == e.oldB)
            continue;
        GeometryEntry g;
        g.item = e.item;
        g.oldA = e.oldA;
        g.oldB = e.oldB;
        g.newA = e.item->p1();
        g.newB = e.item->p2();
        g.oldFlip = g.newFlip = e.item->flipped();
        changed.append(g);
    }
    m_entries.clear();
    m_drag = Drag::None;        // до push(): иначе indexChanged отменит операцию
    updateCursor();
    if (!changed.isEmpty())
        m_undo->push(new GeometryCommand(changed, text));
}

void CircuitView::beginRubber(const QPoint &viewPos, bool additive)
{
    m_drag = Drag::Rubber;
    m_rubberOrigin = viewPos;
    m_rubberBase.clear();
    if (additive) {
        for (CircuitItem *it : selectedCircuitItems())
            m_rubberBase.insert(it);
    } else {
        m_scene->clearSelection();
    }
    m_rubber->setGeometry(QRect(viewPos, QSize()));
    m_rubber->show();
}

void CircuitView::updateRubber(const QPoint &viewPos)
{
    const QRect r = QRect(m_rubberOrigin, viewPos).normalized();
    m_rubber->setGeometry(r);

    QPainterPath path;
    path.addPolygon(mapToScene(r));
    QSet<CircuitItem *> hit;
    const QList<QGraphicsItem *> inside = m_scene->items(path, Qt::IntersectsItemShape);
    for (QGraphicsItem *gi : inside)
        if (auto *ci = qgraphicsitem_cast<CircuitItem *>(gi))
            if (ci != m_preview && ci != m_preview2)
                hit.insert(ci);

    // selectionChanged приходит на каждый элемент; получатели дешёвые
    // (панель свойств склеивает перестройки), так что блокировать сигналы не нужно.
    for (CircuitItem *it : selectedCircuitItems())
        if (!hit.contains(it) && !m_rubberBase.contains(it))
            it->setSelected(false);
    for (CircuitItem *it : std::as_const(hit))
        if (!it->isSelected())
            it->setSelected(true);
}

void CircuitView::endRubber()
{
    m_rubber->hide();
    m_rubberBase.clear();
    m_drag = Drag::None;
}

void CircuitView::cancelOperation()
{
    m_clickToggle = nullptr;
    m_clickDip = nullptr;
    releaseHeldKey();
    switch (m_drag) {
    case Drag::Place:
        if (m_preview) {
            m_scene->removeItem(m_preview);
            delete m_preview;
            m_preview = nullptr;
        }
        if (m_preview2) {
            m_scene->removeItem(m_preview2);
            delete m_preview2;
            m_preview2 = nullptr;
        }
        break;
    case Drag::MoveItems:
    case Drag::MoveEndpoint:
        for (const DragEntry &e : std::as_const(m_entries))
            e.item->setEndpoints(e.oldA, e.oldB);
        break;
    case Drag::Rubber:
        m_rubber->hide();
        m_rubberBase.clear();
        break;
    default:
        break;
    }
    m_entries.clear();
    m_drag = Drag::None;
    updateCursor();
}

// ============================================================== события

void CircuitView::wheelEvent(QWheelEvent *e)
{
    const int dy = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
    if (dy == 0) {
        e->ignore();
        return;
    }

    // Колесо над элементом с главной характеристикой (R, C, L, напряжение…) меняет её значение.
    // Ctrl + колесо — масштаб в любом случае; Shift + колесо — вторичный параметр (номинал, частота).
    if (m_selectMode && m_drag == Drag::None && !(e->modifiers() & Qt::ControlModifier)) {
        updateHover(mapToScene(e->position().toPoint()));
        if (m_hoverItem) {
            const QString key = m_hoverItem->wheelKey(e->modifiers() & Qt::ShiftModifier);
            if (!key.isEmpty()) {
                m_wheelAccum += dy;
                const int steps = m_wheelAccum / 120;
                if (steps != 0) {
                    m_wheelAccum -= steps * 120;
                    setItemParam(m_hoverItem, key, m_hoverItem->wheelStep(key, steps));
                }
                e->accept();
                return;
            }
        }
    }
    m_wheelAccum = 0;
    applyScale(std::pow(1.0015, dy), e->position().toPoint());
    e->accept();
}

void CircuitView::mousePressEvent(QMouseEvent *e)
{
    setFocus();

    if (e->button() == Qt::MiddleButton) {
        beginPan(e->position().toPoint());
        e->accept();
        return;
    }
    if (e->button() != Qt::LeftButton) {
        e->accept();             // ПКМ обработает contextMenuEvent
        return;
    }

    const QPointF sp = mapToScene(e->position().toPoint());
    const QPoint snapped = CircuitScene::snap(sp);

    if (!m_selectMode) {
        beginPlace(snapped);
        e->accept();
        return;
    }

    const Qt::KeyboardModifiers mods = e->modifiers();
    const bool ctrl  = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    const bool alt   = mods & Qt::AltModifier;

    // Окошко осциллограммы: «×» удаляет, иначе — выделить и тащить.
    if (ScopeItem *scope = scopeAt(sp)) {
        if (scope->closeHit(scope->mapFromScene(sp))) {
            m_scene->removeItem(scope);
            delete scope;
        } else if (scope->gearHit(scope->mapFromScene(sp))) {
            m_scene->clearSelection();
            scope->setSelected(true);
            emit scopeActivated(scope);                      // параметры осциллографа
        } else {
            if (!ctrl)
                m_scene->clearSelection();
            scope->setSelected(true);
            m_scopeDrag = scope;
            m_scopeOffset = scope->pos() - sp;
        }
        e->accept();
        return;
    }

    // Блок комментария провода: «×» скрывает его, иначе — выделить и тащить.
    if (NoteItem *note = noteAt(sp)) {
        CircuitItem *owner = note->owner();
        if (note->closeHit(note->mapFromScene(sp))) {
            WireNote n = owner->note();
            n.shown = false;
            setItemNote(owner, n, QStringLiteral("Скрыть блок комментария"));
        } else {
            if (!ctrl)
                m_scene->clearSelection();
            note->setSelected(true);
            m_noteDrag = note;
            m_noteMouseOffset = note->pos() - sp;
            m_noteStartOffset = owner->note().offset;
        }
        e->accept();
        return;
    }

    int ep = -1;
    CircuitItem *it = hitItem(sp, &ep);

    if (!it) {
        beginRubber(e->position().toPoint(), ctrl || shift);
    } else if (ctrl && !alt) {
        it->setSelected(!it->isSelected());                  // Ctrl+клик — добавить/убрать из выделения
    } else if (shift || alt) {
        // Shift — перенести элемент ЦЕЛИКОМ, даже если схвачен за вывод.
        // Alt   — то же, но подключённые провода растягиваются следом.
        if (!it->isSelected()) {
            m_scene->clearSelection();
            it->setSelected(true);
        }
        beginMoveItems(snapped, alt);
    } else if (ep >= 0) {
        beginEndpointDrag(it, ep);                           // вывод: перенос узла / поворот земли
    } else {
        const bool wasSelected = it->isSelected();
        if (!wasSelected) {
            m_scene->clearSelection();
            it->setSelected(true);
        }
        beginMoveItems(snapped, false);
        // Выключатель, кнопка, переключатель: щелчок ЛКМ без сдвига меняет состояние.
        if (m_drag == Drag::MoveItems && it->isToggle()) {
            m_clickToggle = it;
        } else if (m_drag == Drag::MoveItems && it->hasParts()) {
            const int part = it->partAt(sp);
            if (part > 0) {
                if (it->elementType() == ElementType::Keypad) {
                    m_heldKeypad = it;                       // клавиша нажата, пока держим кнопку мыши
                    it->setParam(QStringLiteral("key"), part);
                } else if (!it->isKarnaugh() || wasSelected) {
                    m_clickDip = it;                         // переключатель меняется щелчком без сдвига
                    m_clickDipPart = part;                   // (клетка карты — только у выделенной карты: первый щелчок лишь выделяет)
                }
            }
        }
    }
    e->accept();
}

void CircuitView::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF sp = mapToScene(e->position().toPoint());
    const QPoint snapped = CircuitScene::snap(sp);

    if (m_scopeDrag && (e->buttons() & Qt::LeftButton)) {
        m_scopeDrag->setPos(CircuitScene::snap(sp + m_scopeOffset));
        e->accept();
        return;
    }
    if (m_noteDrag && (e->buttons() & Qt::LeftButton)) {
        m_noteDrag->moveTo(QPointF(CircuitScene::snap(sp + m_noteMouseOffset)));
        e->accept();
        return;
    }

    const QPoint grid(snapped.x() / CircuitScene::kGrid, snapped.y() / CircuitScene::kGrid);
    if (grid != m_cursorGrid) {
        m_cursorGrid = grid;
        emit cursorGridChanged(grid);
    }

    switch (m_drag) {
    case Drag::Pan: {
        const QPoint d = e->position().toPoint() - m_lastPanPos;
        m_lastPanPos = e->position().toPoint();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        break;
    }
    case Drag::Place:
        updatePlace(snapped, e->modifiers() & Qt::ShiftModifier);
        break;
    case Drag::MoveItems:
        applyMove(snapped - m_pressSnapped);
        if (m_heldKeypad && (snapped != m_pressSnapped))
            releaseHeldKey();                                // потянули — это перенос, а не нажатие
        break;
    case Drag::MoveEndpoint:
        applyEndpoint(snapped);
        break;
    case Drag::Rubber:
        updateRubber(e->position().toPoint());
        break;
    case Drag::None:
        updateHover(sp);
        break;
    }
    e->accept();
}

void CircuitView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        if (m_drag == Drag::Pan) {
            m_drag = Drag::None;
            updateCursor();
        }
        e->accept();
        return;
    }
    if (e->button() != Qt::LeftButton) {
        e->accept();
        return;
    }

    if (m_scopeDrag) {
        m_scopeDrag = nullptr;
        e->accept();
        return;
    }
    if (m_noteDrag) {
        NoteItem *note = m_noteDrag;
        m_noteDrag = nullptr;
        CircuitItem *owner = note->owner();
        const QPoint offset = (note->pos() - owner->midpoint()).toPoint();
        if (offset != m_noteStartOffset) {
            WireNote n = owner->note();
            n.offset = offset;
            setItemNote(owner, n, QStringLiteral("Перенос блока комментария"));
        }
        e->accept();
        return;
    }

    CircuitItem *toggle = m_clickToggle;
    m_clickToggle = nullptr;
    releaseHeldKey();
    CircuitItem *dip = m_clickDip;
    const int dipPart = m_clickDipPart;
    m_clickDip = nullptr;
    bool moved = false;
    if ((toggle || dip) && m_drag == Drag::MoveItems) {
        for (const DragEntry &de : std::as_const(m_entries))
            moved = moved || de.item->p1() != de.oldA || de.item->p2() != de.oldB;
    }

    switch (m_drag) {
    case Drag::Place:        finishPlace(); break;
    case Drag::MoveItems:    finishMove(QStringLiteral("Перемещение элементов"));
                             if (toggle && !moved)
                                 toggleItems({ toggle });
                             if (dip && !moved)
                                 toggleDipSwitch(dip, dipPart);
                             break;
    case Drag::MoveEndpoint: finishMove(QStringLiteral("Перемещение вывода")); break;
    case Drag::Rubber:       endRubber(); break;
    default: break;
    }
    updateHover(mapToScene(e->position().toPoint()));
    e->accept();
}

void CircuitView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_selectMode) {
        if (ScopeItem *scope = scopeAt(mapToScene(e->position().toPoint()))) {
            m_scopeDrag = nullptr;
            emit scopeActivated(scope);
            e->accept();
            return;
        }
    }
    if (e->button() == Qt::LeftButton && m_selectMode) {
        if (NoteItem *note = noteAt(mapToScene(e->position().toPoint()))) {
            m_noteDrag = nullptr;
            emit noteEditRequested(note->owner());
            e->accept();
            return;
        }
    }
    if (e->button() == Qt::LeftButton && m_selectMode) {
        if (CircuitItem *it = hitItem(mapToScene(e->position().toPoint()), nullptr)) {
            cancelOperation();           // первый клик двойного клика мог начать перенос
            if (it->isWire()) {          // у провода окна параметров нет, но есть комментарий
                if (!it->isSelected()) {
                    m_scene->clearSelection();
                    it->setSelected(true);
                }
                emit noteEditRequested(it);
                e->accept();
                return;
            }
            if (!it->hasProperties()) {
                // У выключателей и кнопок окна свойств нет: двойной клик — это два переключения подряд.
                if (it->isToggle())
                    toggleItems({ it });
                e->accept();
                return;
            }
            if (!it->isSelected()) {
                m_scene->clearSelection();
                it->setSelected(true);
            }
            emit itemActivated(it);
            e->accept();
            return;
        }
    }
    e->accept();
}

void CircuitView::keyPressEvent(QKeyEvent *e)
{
    const int step = CircuitScene::kGrid * ((e->modifiers() & Qt::ShiftModifier) ? 5 : 1);
    switch (e->key()) {
    case Qt::Key_Left:  moveSelection(QPoint(-step, 0)); break;
    case Qt::Key_Right: moveSelection(QPoint(step, 0));  break;
    case Qt::Key_Up:    moveSelection(QPoint(0, -step)); break;
    case Qt::Key_Down:  moveSelection(QPoint(0, step));  break;
    case Qt::Key_Space: toggleSelectedSwitches();        break;
    default:
        QGraphicsView::keyPressEvent(e);
        return;
    }
    e->accept();
}

void CircuitView::contextMenuEvent(QContextMenuEvent *e)
{
    if (m_selectMode && m_drag == Drag::None) {
        if (ScopeItem *scope = scopeAt(mapToScene(e->pos()))) {
            m_contextItem = nullptr;
            m_scene->clearSelection();
            scope->setSelected(true);
            emit scopeMenuRequested(scope, e->globalPos());
            e->accept();
            return;
        }
    }
    if (m_selectMode && m_drag == Drag::None) {
        NoteItem *note = noteAt(mapToScene(e->pos()));
        CircuitItem *it = note ? note->owner() : hitItem(mapToScene(e->pos()), nullptr);
        m_contextItem = it;
        if (it && !it->isSelected()) {
            m_scene->clearSelection();
            it->setSelected(true);
        }
    } else {
        m_contextItem = nullptr;
    }
    emit contextMenuRequested(e->globalPos());
    e->accept();
}

void CircuitView::leaveEvent(QEvent *e)
{
    dropHover();
    QGraphicsView::leaveEvent(e);
}
