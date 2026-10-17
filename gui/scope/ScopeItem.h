#pragma once

#include <QGraphicsObject>
#include <QPointer>
#include <QRectF>

#include "scope/ScopeSource.h"

class QTimer;

// Осциллограмма на схеме: окошко с графиком, привязанное пунктирной линией к элементу.
// Это обычный объект сцены: его можно двигать мышью, выделять и удалять (Del или «×» в заголовке).
class ScopeItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum { Type = UserType + 2 };
    static constexpr qreal kWidth = 340;
    static constexpr qreal kHeight = 190;
    static constexpr qreal kTitle = 20;

    explicit ScopeItem(ScopeSource *source, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }
    ScopeSource *source() const { return m_source; }

    // Нажали на «×» в заголовке (координаты элемента).
    bool closeHit(const QPointF &local) const;
    // Нажали на «шестерёнку» — параметры осциллографа.
    bool gearHit(const QPointF &local) const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF box() const { return QRectF(0, 0, kWidth, kHeight); }
    QRectF closeRect() const;
    QRectF plotRect() const { return QRectF(0, kTitle, kWidth, kHeight - kTitle); }
    QLineF linkLine() const;          // от окошка к элементу, в координатах окошка
    void linkChanged();

    QPointer<ScopeSource> m_source;
    QTimer *m_timer = nullptr;
    bool m_dirty = true;
};
