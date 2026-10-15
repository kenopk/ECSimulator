#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QPoint>
#include <QVector>

class CircuitItem;

// Сцена схемы: сетка на фоне, точки соединений и «красные» близкие концы поверх элементов.
class CircuitScene : public QGraphicsScene
{
    Q_OBJECT
public:
    static constexpr int kGrid = 16;           // шаг сетки, единиц сцены

    explicit CircuitScene(QObject *parent = nullptr);

    static QPoint snap(const QPointF &pos);

    QList<CircuitItem *> circuitItems() const;
    QRectF contentRect() const;                // охватывающий прямоугольник всех элементов

    // Связность: сколько выводов сходится в точке (для узлов) + поиск «почти касаний».
    // Вызывается элементами при любом изменении геометрии; пересчёт ленивый.
    void invalidateConnectivity();
    int connectionCount(const QPoint &p) const;

    // Шаг анимации тока (вызывает CircuitView по таймеру).
    void tickFlow(qreal dt);
    void clearElectricalState();

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    struct Node {
        QPoint pt;
        int count = 0;
    };

    static qint64 key(const QPoint &p);
    void ensureConnectivity() const;
    void rebuild() const;
    void refresh();

    mutable QHash<qint64, Node> m_nodes;       // только размещённые элементы (без превью)
    mutable QVector<QPoint> m_nearPoints;      // свободные концы, слишком близкие к чужим
    mutable bool m_connectivityDirty = true;
    bool m_refreshQueued = false;
};
