#include "InfiniteGraphicsScene.h"
#include <QPainter>
#include <QGraphicsItem>
#include <cmath>

InfiniteGraphicsScene::InfiniteGraphicsScene(QObject *parent)
    : QGraphicsScene(parent) {}

void InfiniteGraphicsScene::drawBackground(QPainter *painter, const QRectF &rect) {
    // ... без изменений, весь код сетки остаётся как был ...
}

void InfiniteGraphicsScene::drawForeground(QPainter *painter, const QRectF &rect) {
    Q_UNUSED(painter);
    Q_UNUSED(rect);
}
