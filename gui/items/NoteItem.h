#pragma once

#include <QGraphicsObject>
#include <QSizeF>
#include <QString>

class CircuitItem;

// Блок комментария провода на схеме: карточка с текстом и пунктирной линией привязки к середине провода
// (как окошко осциллограммы). Это дочерний объект провода: он уходит со сцены и возвращается вместе с ним.
// Двигает блок CircuitView (смещение хранится в проводе — CircuitItem::note().offset).
class NoteItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum { Type = UserType + 3 };
    static constexpr qreal kMaxTextWidth = 240;
    static constexpr qreal kPad = 8;

    explicit NoteItem(CircuitItem *owner);

    int type() const override { return Type; }
    CircuitItem *owner() const { return m_owner; }

    // Пересчитать размер по тексту и перерисовать (текст, длина или положение провода изменились).
    void refresh();
    // Перенос блока при перетаскивании: линия привязки меняется, поэтому область перерисовки пересчитывается.
    void moveTo(const QPointF &scenePos);
    // Нажали на «×» в углу блока (координаты блока).
    bool closeHit(const QPointF &local) const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF box() const { return QRectF(0, 0, m_size.width(), m_size.height()); }
    QRectF closeRect() const;
    QLineF linkLine() const;

    CircuitItem *m_owner;
    QSizeF m_size = QSizeF(80, 32);
    QString m_text;
    QString m_footer;
};
