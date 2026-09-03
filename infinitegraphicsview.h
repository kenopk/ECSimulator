#pragma once

#include <QGraphicsView>
#include <QUndoStack>
#include <QHash>
#include <QPair>
#include "pin.h"
#include "component.h"
#include <QShortcut>

class Wire;

class InfiniteGraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    static constexpr qreal kGridSize = 5;
    QUndoStack* undoStack() const { return m_undoStack; }
    explicit InfiniteGraphicsView(QWidget *parent = nullptr);

    void setWireMode(bool on);
    bool isWireMode() const { return m_wireMode; }

    // snapToGrid теперь возвращает QPoint (целое)
    static QPoint snapToGrid(const QPointF &pos, qreal gridSize = kGridSize);

    void deleteSelectedItems();
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;

    struct CopiedWireData {
        QPoint anchor;          // <-- QPoint
        QPoint relativeStart;   // <-- QPoint
        QPoint relativeEnd;     // <-- QPoint
        bool startConnected;
        bool endConnected;
        bool isComponent = false;
        ComponentType componentType = ComponentType::Resistor;
        QVariantMap params;
    };
    QList<CopiedWireData> m_clipboard;
    QPoint m_clipboardAnchor;   // <-- QPoint
    QPoint m_pasteOffset;       // <-- QPoint
    void setComponentMode(ComponentType type);
    bool isComponentMode() const { return m_componentMode; }
    ComponentType currentComponentType() const { return m_componentType; }
    void disableComponentMode();
    void copySelected();
    void cutSelected();
    void pasteClipboard();
    void setPreferredCursor(Qt::CursorShape shape);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    QPoint countSteps(QKeyEvent *event); // шаг стрелок: базовый 50, с Shift — 200
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    // findWireAt(const QPoint&, qreal) удалён как неиспользуемый (dead code).
    Wire* findWireEndpointNear(const QPoint &scenePos, bool *isStart); // <-- QPoint

    bool m_panning = false;
    QPoint m_lastPanPos; // уже QPoint

    bool m_wireMode = false;
    bool m_wireDrawing = false;
    Pin m_wireStartPin;
    QGraphicsLineItem *m_previewLine = nullptr;

    Wire *m_draggedWire = nullptr;
    bool m_draggingStart = false;
    QPoint m_dragOldPos;          // <-- QPoint

    QList<Wire*> m_movingWires;
    QHash<Wire*, QPair<QPoint, QPoint>> m_moveOriginalPositions; // <-- QPoint
    QPoint m_movePressScenePos;   // <-- QPoint

    QUndoStack *m_undoStack = nullptr;
    bool m_dragWithShift = false;
    Wire *m_hoveredWire = nullptr;
    bool m_hoveredEndpointIsStart = false;
    void clearHover();
    QPoint findFreePosition(const QPoint &desiredStart, const QPoint &delta, qreal step); // <-- QPoint
    QPoint m_lastPastePos;   // <-- QPoint
    bool m_hasPasted = false;
    bool m_componentMode = false;
    ComponentType m_componentType = ComponentType::Resistor;

    QVector<QPair<Wire*, bool>> m_connectedWiresAtPress;
    void collectNodeWires(const QPoint &anchor);
    Qt::CursorShape m_preferredCursor = Qt::ArrowCursor;   // по умолчанию — стрелка

    QShortcut *m_wireShortcut = nullptr;
    QShortcut *m_resistorShortcut = nullptr;
    QShortcut *m_capacitorShortcut = nullptr;
};