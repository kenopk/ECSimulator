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

    // --- длина проводов -----------------------------------------------------------------------------
    // Масштаб схемы как платы: сколько миллиметров в одной клетке сетки (хранится в проекте у каждой схемы).
    double mmPerCell() const { return m_mmPerCell; }
    void setMmPerCell(double mm);

    // Трасса — цепочка проводов, которые стыкуются концами по двое (без выводов элементов и ответвлений);
    // «проводник» дифф. пары: хоть прямой, хоть квадратом из четырёх проводов. Пару задаёт пользователь —
    // параметр проводов «diffPair» (номер пары, у трассы берётся наименьший ненулевой). Трассы с одним номером —
    // одна пара: их длины сравниваются, самые короткие помечаются lagging (рисуются красным); у пары k на схеме k штрихов.
    struct Trace {
        QVector<CircuitItem *> wires;
        double lengthPx = 0;
        int pair = 0;                  // 0 — не в паре
        int pairSize = 1;              // трасс в паре
        double longestPx = 0;          // самая длинная и самая короткая трасса пары
        double shortestPx = 0;
        bool lagging = false;
    };
    Trace traceOf(const CircuitItem *wire) const;          // пустая трасса, если провода нет на схеме

    struct PairInfo {
        int number = 0;
        int traces = 0;
        double longestPx = 0;
        double shortestPx = 0;
    };
    QVector<PairInfo> diffPairs() const;                   // пары, которые есть на схеме, по возрастанию номера
    int nextFreePair() const;                              // наименьший свободный номер пары
    static constexpr double kLengthTolerancePx = 0.5;      // разница длин меньше этого — длины равны

signals:
    void topologyChanged();                    // изменилась схема (геометрия, состав): можно перепроверить правила
    void scaleChanged();                       // изменился масштаб «мм на клетку»
    void karnaughChanged(CircuitItem *item);   // у элемента «Карта Карно» поменялись клетки или размер
    void karnaughAttached(CircuitItem *item);  // элемент со связью оказался на сцене (вставка, отмена удаления)

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
    void rebuildTraces() const;
    void refresh();

    mutable QHash<qint64, Node> m_nodes;       // только размещённые элементы (без превью)
    mutable QVector<QPoint> m_nearPoints;      // свободные концы, слишком близкие к чужим
    mutable QVector<Trace> m_traces;
    mutable bool m_connectivityDirty = true;
    bool m_refreshQueued = false;
    double m_mmPerCell = 1.0;
};
