#include "InfiniteGraphicsScene.h"
#include <QPainter>
#include <QGraphicsItem>
#include <QPen>
#include <cmath>

InfiniteGraphicsScene::InfiniteGraphicsScene(QObject *parent)
    : QGraphicsScene(parent) {}

void InfiniteGraphicsScene::setBackgroundColor(const QColor &color) {
    if (m_backgroundColor == color) return;
    m_backgroundColor = color;
    update();   // перерисовать фон немедленно (например, при смене темы в настройках)
}

void InfiniteGraphicsScene::drawBackground(QPainter *painter, const QRectF &rect) {
    painter->fillRect(rect, m_backgroundColor);

    constexpr qreal minorStep  = 5.0;   // совпадает с InfiniteGraphicsView::kGridSize
    constexpr int   majorEvery = 10;    // каждая 10-я линия — "жирная"
    const qreal majorStep = minorStep * majorEvery;
    Q_UNUSED(majorStep);

    // Защита от слишком мелкой сетки при сильном отдалении (муар + лаги)
    const bool tooDense = (rect.width() / minorStep) > 3000;

    // Работаем с ЦЕЛОЧИСЛЕННЫМИ индексами клеток, а не с плавающей точкой:
    // так «каждая 10-я линия — жирная» проверяется через (i % majorEvery == 0),
    // без риска, что std::fmod(x, majorStep) из-за погрешности округления
    // на больших координатах сцены (у нас sceneRect до ±10000) вернёт
    // что-то вроде 0.0000000003 вместо ровного нуля и линия «моргнёт».
    const qint64 firstIndex  = static_cast<qint64>(std::floor(rect.left()   / minorStep));
    const qint64 lastIndex   = static_cast<qint64>(std::ceil (rect.right()  / minorStep));
    const qint64 firstIndexY = static_cast<qint64>(std::floor(rect.top()    / minorStep));
    const qint64 lastIndexY  = static_cast<qint64>(std::ceil (rect.bottom() / minorStep));

    auto isMajor = [](qint64 index) {
        // Целочисленный остаток корректно работает и для отрицательных индексов
        return ((index % majorEvery) + majorEvery) % majorEvery == 0;
    };

    if (!tooDense) {
        QPen minorPen(QColor(255, 255, 255, 18));
        minorPen.setCosmetic(true);   // толщина не зависит от зума
        painter->setPen(minorPen);
        for (qint64 i = firstIndex; i <= lastIndex; ++i) {
            if (isMajor(i)) continue;   // жирные линии рисуем отдельно, ниже
            const qreal x = i * minorStep;
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qint64 i = firstIndexY; i <= lastIndexY; ++i) {
            if (isMajor(i)) continue;
            const qreal y = i * minorStep;
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
    }

    QPen majorPen(QColor(255, 255, 255, 45));
    majorPen.setCosmetic(true);
    painter->setPen(majorPen);
    for (qint64 i = firstIndex; i <= lastIndex; ++i) {
        if (!isMajor(i)) continue;
        const qreal x = i * minorStep;
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    for (qint64 i = firstIndexY; i <= lastIndexY; ++i) {
        if (!isMajor(i)) continue;
        const qreal y = i * minorStep;
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
}

void InfiniteGraphicsScene::drawForeground(QPainter *painter, const QRectF &rect) {
    Q_UNUSED(rect);

    // Общая рамка выделения: рисуется ПОСЛЕ всех объектов сцены (см. комментарий
    // в .h), поэтому видна целиком, не перекрытая ни одним проводом/компонентом.
    const QList<QGraphicsItem*> selected = selectedItems();
    if (selected.isEmpty()) return;

    QRectF unionRect;
    for (QGraphicsItem *item : selected) {
        const QRectF r = item->sceneBoundingRect();
        unionRect = unionRect.isNull() ? r : unionRect.united(r);
    }

    QPen pen(QColor(255, 200, 0, 220));
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true);   // толщина рамки не зависит от зума, как и у сетки выше

    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(unionRect.adjusted(-4, -4, 4, 4));
}