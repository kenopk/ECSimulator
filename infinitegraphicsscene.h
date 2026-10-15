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

private:
    QColor m_backgroundColor = QColor(30, 30, 46);
};