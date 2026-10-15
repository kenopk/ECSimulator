#pragma once

#include <QGraphicsItem>
#include <QMap>
#include <QPainterPath>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QVector>

#include "util/CoreTypes.h"

// Один графический элемент схемы: провод или компонент любой сложности.
//
// Геометрия хранится в координатах сцены (целые, по сетке):
//   p1, p2 — две опорные точки, задающие ось элемента;
//   flip   — зеркальное отражение относительно оси.
// Положение выводов (их может быть 1..4) выводится из p1/p2 по таблице
// SymbolPainter::spec(): у двухвыводных элементов выводы лежат в p1 и p2,
// у многовыводных (транзистор, ОУ, трансформатор…) — по краям вокруг оси.
// У земли один вывод (p1), а p2 задаёт направление «стержня».
//
// Параметры хранятся как карта «ключ -> значение в СИ». Ключи и значения по
// умолчанию задаёт ядро (govno::core::elementInfo).
class CircuitItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };

    explicit CircuitItem(ElementType type);

    int type() const override { return Type; }

    ElementType elementType() const { return m_type; }
    bool isWire() const { return m_type == ElementType::Wire; }
    bool isText() const { return m_type == ElementType::TextLabel; }
    bool isBox() const { return m_type == ElementType::Box; }
    bool isDecoration() const;                            // надпись, рамка, линия: к цепи не относятся
    int  pinCount() const;
    unsigned variant() const { return m_variant; }
    bool isSinglePin() const { return pinCount() == 1; }
    bool isMultiPin() const { return pinCount() >= 3; }   // тянуть за вывод нельзя — двигается весь элемент
    double toggledValue() const;                          // следующее значение переключателя
    bool isToggle() const;                                // выключатель / кнопка / переключатель
    QString toggleKey() const;
    // Есть ли у элемента окно/панель свойств. У выключателей, кнопок и переключателей его нет:
    // единственный параметр — положение, оно меняется кликом.
    bool hasProperties() const;
    // Элементы с отдельно нажимаемыми частями: клавиатура (клавиши) и DIP-переключатель.
    bool hasParts() const;
    int  partAt(const QPointF &scenePos) const;            // номер клавиши/переключателя под точкой (1…N), 0 — мимо
    // Параметр, который меняется колесом мыши над элементом (пустая строка — такого нет).
    // secondary — вариант с Shift (номинал у переменных элементов, частота у источников).
    QString wheelKey(bool secondary = false) const;
    // Новое значение параметра key после dir шагов колеса (dir > 0 — увеличить).
    double wheelStep(const QString &key, int dir) const;
    QString displayName() const;

    // --- геометрия -------------------------------------------------------
    QPoint p1() const { return m_p1; }
    QPoint p2() const { return m_p2; }
    bool flipped() const { return m_flip; }

    // Задаёт геометрию; пара (a, b) приводится к допустимой для типа (ось, длина, шаг).
    void setGeometry(const QPoint &a, const QPoint &b, bool flip);
    void setEndpoints(const QPoint &a, const QPoint &b) { setGeometry(a, b, m_flip); }
    void setFlip(bool flip) { setGeometry(m_p1, m_p2, flip); }
    // Двигает «ручку» (0 — p1, 1 — p2). У земли: 0 — перенос целиком, 1 — поворот.
    void setEndpoint(int index, const QPoint &p);
    QPoint endpointPoint(int index) const { return index == 0 ? m_p1 : m_p2; }
    bool acceptsEndpoint(int index, const QPoint &p) const;

    QVector<QPoint> connectionPoints() const;            // положения выводов (узлы схемы)
    QPoint pinPoint(int pin) const;
    qreal length() const;
    qreal minLength() const;
    qreal angleDegrees() const;
    // Индекс ручки (0/1) рядом с точкой или -1. У многовыводных элементов ручек нет.
    int endpointAt(const QPointF &scenePos, qreal tolerance) const;

    // --- параметры -------------------------------------------------------
    double param(const QString &key) const { return m_params.value(key); }
    void setParam(const QString &key, double value);
    const QMap<QString, double> &params() const { return m_params; }
    void setParams(const QMap<QString, double> &p);

    // Текст надписи (только у элемента «Надпись»).
    const QString &text() const { return m_text; }
    void setText(const QString &t);

    // --- состояние отображения ------------------------------------------
    void setPreview(bool on);
    bool isPreview() const { return m_preview; }
    void setHover(bool body, int endpoint);

    // Маска выводов, подсвеченных красным (слишком близкие, но не соединённые концы).
    void setNearMiss(quint64 mask);
    quint64 nearMissMask() const { return m_nearMask; }

    // Результаты расчёта (их будет выставлять ядро через gui-слой).
    void setElectrical(const QVector<double> &pinVoltages, double current);
    void clearElectrical();
    bool advanceFlow(qreal dt);   // true, если точки тока анимируются
    static void setFlowSpeed(qreal pixelsPerSecond) { s_flowSpeed = pixelsPerSecond; }

    // --- QGraphicsItem ---------------------------------------------------
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void notifyScene();
    QString computeLabel() const;
    unsigned computeVariant() const;
    void refreshVariant();
    void updateLabelRect();
    QPointF toScene(qreal x, qreal y) const;     // локальные координаты -> сцена (с учётом flip)
    int stateValue() const;

    ElementType m_type;
    QPoint m_p1;
    QPoint m_p2;
    bool m_flip = false;
    QMap<QString, double> m_params;

    QString m_text = QStringLiteral("Надпись");
    QString m_label;
    QRectF  m_labelRect;

    bool m_preview = false;
    bool m_hoverBody = false;
    int  m_hoverEndpoint = -1;
    quint64 m_nearMask = 0;
    unsigned m_variant = 0;                // вариант раскладки выводов (см. govno::core::variantOf)

    bool   m_hasElectrical = false;
    QVector<double> m_pinV;
    double m_current = 0;
    qreal  m_flowPhase = 0;

    mutable QPainterPath m_shapeCache;
    mutable bool m_shapeDirty = true;

    static inline qreal s_flowSpeed = 40.0;
};
