#pragma once

#include <QElapsedTimer>
#include <QGraphicsView>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>
#include <climits>

#include "util/CoreTypes.h"

class CircuitItem;
class CircuitScene;
class QRubberBand;
class QUndoStack;

// Слепок одного элемента (буфер обмена, дублирование вкладки).
struct ItemData {
    ElementType type = ElementType::Wire;
    QPoint a, b;
    bool flip = false;
    QMap<QString, double> params;
    QString text;                     // текст надписи
};

// Редактор схемы: выбор, рисование проводов и элементов, перенос, поворот,
// масштаб, панорама, буфер обмена. Владеет сценой и стеком отмены.
//
// Перемещение (режим «Выбор»):
//   • тянуть тело элемента        — переносятся выделенные элементы, провода остаются;
//   • тянуть вывод (ручку)        — переносится узел: вместе с ним все концы в этой точке;
//   • Shift + тянуть что угодно   — элемент переносится ЦЕЛИКОМ, даже если схвачен за вывод;
//   • Alt + тянуть                — как Shift, но подключённые провода растягиваются следом;
//   • земля: тело/вывод — перенос, кружок на конце стержня — поворот;
//   • стрелки — сдвиг на клетку (Shift — на 5 клеток).
class CircuitView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CircuitView(QWidget *parent = nullptr);
    ~CircuitView() override;

    CircuitScene *circuitScene() const { return m_scene; }
    QUndoStack   *undoStack() const { return m_undo; }

    // --- режимы ----------------------------------------------------------
    bool isSelectMode() const { return m_selectMode; }
    ElementType placeType() const { return m_placeType; }
    void setSelectMode();                    // также отменяет текущую операцию
    void setPlaceMode(ElementType type, bool routed = false);   // routed — Г-образный провод одним движением
    bool routedWire() const { return m_routedWire && !m_selectMode; }     // Wire тоже ElementType
    void escape();                           // Esc: отмена операции -> режим выбора -> снять выделение

    // --- правка ----------------------------------------------------------
    QList<CircuitItem *> selectedCircuitItems() const;
    CircuitItem *singleSelected() const;     // выбран ровно один элемент или nullptr
    bool hasSelection() const;
    bool selectionHasToggle() const;
    static bool clipboardHasData();
    void selectAll();
    void deleteSelected();
    void copySelected();
    void cutSelected();
    void pasteClipboard();
    void clearAll();
    void rotateSelection(bool clockwise);
    void flipSelection();
    void toggleSelectedSwitches();
    void toggleItems(const QList<CircuitItem *> &items);
    void toggleDipSwitch(CircuitItem *item, int position);   // позиция 1…N (одно действие отмены)
    CircuitItem *contextItem() const { return m_contextItem; }   // элемент под последним вызовом меню (или nullptr)   // замкнуть/разомкнуть (одно действие отмены)
    void setItemParam(CircuitItem *item, const QString &key, double value);
    void setItemParams(CircuitItem *item, const QMap<QString, double> &values);
    void setItemText(CircuitItem *item, const QString &text);

    // --- слепки (дублирование схемы) --------------------------------------
    QVector<ItemData> snapshot(bool selectedOnly) const;
    void loadSnapshot(const QVector<ItemData> &data, const QString &undoText);

    // --- вид -------------------------------------------------------------
    qreal zoom() const;
    void zoomIn();
    void zoomOut();
    void resetView();
    void fitToContent();

    // Анимация точек тока (включит ядро, когда появится расчёт).
    void setFlowAnimation(bool on);
    void resetSimulationState();

signals:
    void modeChanged();
    void selectionChanged();
    void cursorGridChanged(const QPoint &gridPos);
    void zoomChanged(qreal zoom);
    void itemActivated(CircuitItem *item);            // двойной клик
    void contextMenuRequested(const QPoint &globalPos);

protected:
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    enum class Drag { None, Pan, Rubber, MoveItems, MoveEndpoint, Place };

    struct DragEntry {
        CircuitItem *item = nullptr;
        int endpoint = -1;          // -1 — переносится весь элемент, 0/1 — только этот конец
        QPoint oldA, oldB;
    };

    // поиск под курсором
    CircuitItem *hitItem(const QPointF &scenePos, int *endpoint) const;
    qreal hitTolerance() const;

    // операции
    void beginPan(const QPoint &viewPos);
    void beginPlace(const QPoint &snapped);
    void updatePlace(const QPoint &snapped, bool shift);
    void finishPlace();
    void beginMoveItems(const QPoint &snapped, bool withWires);
    void beginEndpointDrag(CircuitItem *item, int endpoint);
    void applyMove(const QPoint &delta);
    void applyEndpoint(const QPoint &pt);
    void finishMove(const QString &text);
    void beginRubber(const QPoint &viewPos, bool additive);
    void updateRubber(const QPoint &viewPos);
    void endRubber();
    void cancelOperation();

    QList<CircuitItem *> addItems(const QVector<ItemData> &data, const QPoint &offset,
                                  const QString &undoText, bool selectNew);
    void updateHover(const QPointF &scenePos);
    void dropHover();
    void updateCursor();
    void applyScale(qreal factor, const QPoint &anchorViewPos);
    void moveSelection(const QPoint &delta);

    CircuitScene *m_scene = nullptr;
    QUndoStack   *m_undo = nullptr;

    bool m_selectMode = true;
    ElementType m_placeType = ElementType::Wire;

    Drag m_drag = Drag::None;
    QPoint m_lastPanPos;
    QPoint m_pressSnapped;
    QVector<DragEntry> m_entries;
    CircuitItem *m_preview = nullptr;
    CircuitItem *m_preview2 = nullptr;       // вторая половина трассированного провода
    bool m_routedWire = false;
    CircuitItem *m_clickToggle = nullptr;   // ключ/кнопка под нажатой ЛКМ: щелчок без сдвига переключает
    CircuitItem *m_contextItem = nullptr;
    CircuitItem *m_heldKeypad = nullptr;    // клавиатура с удерживаемой клавишей
    CircuitItem *m_clickDip = nullptr;      // DIP-переключатель: щелчок без сдвига переключает позицию m_clickDipPart
    int m_clickDipPart = 0;
    void releaseHeldKey();

    QRubberBand *m_rubber = nullptr;
    QPoint m_rubberOrigin;
    QSet<CircuitItem *> m_rubberBase;

    int m_wheelAccum = 0;                // накопленный поворот колеса над элементом
    CircuitItem *m_hoverItem = nullptr;
    int m_hoverEndpoint = -1;
    QPoint m_cursorGrid = QPoint(INT_MIN, INT_MIN);

    QPoint m_lastPasteTarget = QPoint(INT_MIN, INT_MIN);
    int m_pasteCount = 0;

    QTimer m_flowTimer;
    QElapsedTimer m_flowClock;
};
