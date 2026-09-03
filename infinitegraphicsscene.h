#pragma once
#include <QGraphicsScene>
#include <QColor>

class InfiniteGraphicsScene : public QGraphicsScene {
public:
    explicit InfiniteGraphicsScene(QObject *parent = nullptr);

    // Цвет фона задаётся здесь, а не через QGraphicsView::setBackgroundBrush() —
    // поскольку drawBackground() переопределён, именно сцена отвечает за
    // отрисовку фона, и backgroundBrush() у view при наличии сцены не используется.
    void setBackgroundColor(const QColor &color);
    QColor backgroundColor() const { return m_backgroundColor; }

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    // drawForeground — метод-"близнец" drawBackground, но вызывается
    // Qt В ДРУГОЙ МОМЕНТ последовательности отрисовки кадра: сначала
    // drawBackground() (самый низ, сетка), потом рисуются ВСЕ объекты
    // сцены по очереди (провода через свой paint()), и только В САМОМ
    // КОНЦЕ вызывается drawForeground() — то, что мы нарисуем здесь,
    // окажется НАД абсолютно всеми объектами сцены, как прозрачная
    // накладка сверху. Идеальное место для общей рамки выделения —
    // она должна быть видна целиком, не перекрытая ни одним проводом.

private:
    QColor m_backgroundColor = QColor(30, 30, 46);
};