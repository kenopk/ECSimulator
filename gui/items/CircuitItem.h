#pragma once

#include <QGraphicsItem>
#include <QMap>
#include <QPainterPath>
#include <QPointer>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include "items/NoteItem.h"
#include "items/WireNote.h"
#include "scope/ScopeSource.h"
#include "util/CoreTypes.h"

// Что сцена знает о трассе (цепочке проводов от вывода до вывода), в которую входит провод: полная длина и
// принадлежность дифференциальной паре. Пару задаёт пользователь (параметр провода «diffPair» — её номер);
// у номера k на проводе рисуется k штрихов. Трассы одной пары сравниваются по длине: короткая — красная.
struct TraceMark {
    double lengthPx = 0;                   // длина всей трассы, единицы сцены
    int pair = 0;                          // номер дифф. пары, 0 — провод не в паре
    int pairSize = 1;                      // сколько трасс (проводников) в паре
    double longestPx = 0;                  // самая длинная и самая короткая трасса пары
    double shortestPx = 0;
    bool lagging = false;                  // короче самой длинной трассы пары: рисуется красным
    bool carrier = false;                  // штрихи рисует именно этот провод (самый длинный в трассе)
    bool operator==(const TraceMark &o) const
    {
        return qFuzzyCompare(lengthPx + 1.0, o.lengthPx + 1.0) && pair == o.pair && pairSize == o.pairSize
               && qFuzzyCompare(longestPx + 1.0, o.longestPx + 1.0) && qFuzzyCompare(shortestPx + 1.0, o.shortestPx + 1.0)
               && lagging == o.lagging && carrier == o.carrier;
    }
};

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
    ~CircuitItem() override;

    int type() const override { return Type; }

    ElementType elementType() const { return m_type; }
    bool isWire() const { return m_type == ElementType::Wire; }
    bool isText() const { return m_type == ElementType::TextLabel; }
    bool isBox() const { return m_type == ElementType::Box; }
    bool isDecoration() const;                            // надпись, рамка, линия: к цепи не относятся
    int  pinCount() const;
    unsigned variant() const { return m_variant; }
    bool isSinglePin() const { return pinCount() == 1; }
    bool isMultiPin() const { return pinCount() >= 3 || pinCount() == 0; }   // тянуть за вывод нельзя — двигается весь элемент
    double toggledValue() const;                          // следующее значение переключателя
    bool isToggle() const;                                // выключатель / кнопка / переключатель
    QString toggleKey() const;
    // Есть ли у элемента окно/панель свойств. У выключателей, кнопок и переключателей его нет:
    // единственный параметр — положение, оно меняется кликом.
    bool hasProperties() const;
    // Элементы с отдельно нажимаемыми частями: клавиатура (клавиши) и DIP-переключатель.
    bool hasParts() const;
    // Карта Карно как элемент: клетки 16×16 по строкам (0, 1, 2 = X). Видны первые «высота × ширина»;
    // остальные хранятся, чтобы смена размера не стирала введённое. Формула пересчитывается при любой правке.
    static constexpr int kKarnaughStride = 16;
    bool isKarnaugh() const { return m_type == ElementType::KarnaughMap; }
    const QVector<quint8> &cells() const { return m_cells; }
    void setCells(const QVector<quint8> &cells);
    const QString &karnaughFormula() const { return m_formula; }
    // Связь с картой Карно из раздела «Карты Карно» (имя карты в том же проекте; пусто — нет связи).
    // Данные карты показываются на схеме, а правки клеток и размера на схеме уходят в карту (см. tools/KarnaughLinks).
    const QString &link() const { return m_link; }
    void setLink(const QString &name) { m_link = name; }
    const QStringList &varNames() const { return m_varNames; }          // имена переменных из связанной карты
    void setVarNames(const QStringList &names);
    int  partAt(const QPointF &scenePos) const;            // номер клавиши/переключателя под точкой (1…N), 0 — мимо
    // Параметр, который меняется колесом мыши над элементом (пустая строка — такого нет).
    // secondary — вариант с Shift (номинал у переменных элементов, частота у источников).
    QString wheelKey(bool secondary = false) const;
    // Новое значение параметра key после dir шагов колеса (dir > 0 — увеличить).
    double wheelStep(const QString &key, int dir) const;
    // Тот же шаг для любого числового параметра (используют поля свойств): ряд E12 для Ω/Ф/Гн, 5 % для процентов,
    // целые — по единице, остальное — ряд 1-1.5-2-2.5-3-4-5…; результат ограничен [minV, maxV].
    static double stepValue(const QString &unit, double minV, double maxV, bool integer, double value, int dir);
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

    // --- комментарий и длина провода -------------------------------------------------------------
    const WireNote &note() const { return m_note; }
    void setNote(const WireNote &n);                       // создаёт или убирает блок на схеме
    NoteItem *noteItem() const { return m_noteItem; }
    QPointF midpoint() const { return QPointF(m_p1 + m_p2) / 2.0; }
    const TraceMark &traceMark() const { return m_trace; }
    void setTraceMark(const TraceMark &t);                 // вызывает сцена при пересчёте трасс
    double traceLengthMm() const;                          // длина всей трассы в мм (по масштабу схемы)
    double pcbWidthMm() const { return param(QStringLiteral("pcbWidth")); }   // ширина на плате, 0 — не задана
    QString hoverTip() const;                              // подсказка при наведении (пусто — не показывать)
    int diffPair() const { return qRound(param(QStringLiteral("diffPair"))); }   // номер пары, поставленный на этом проводе (0 — нет)
    double traceSkewMm() const;                            // на сколько трасса короче (−) или длиннее (+) остальных в паре, мм
    QString noteFooter() const;                            // строка «L = … мм · дифф. пара №k · …» под текстом блока
    QString traceName() const;                             // для связи с расчётами: первая строка комментария или «Провод»

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

    // Осциллограмма: источник отсчётов (напряжение на элементе и ток) создаётся по требованию.
    // Отсчёты пишутся при каждом setElectrical; время расчёта выставляет ядро через setSimTime.
    ScopeSource *probe(bool create = false);

    // Постоянный номер элемента: по нему ядро и журнал ссылаются на элемент, не держа указатели.
    quint64 uid() const { return m_uid; }

    // --- свечение индикаторов ---------------------------------------------------------------
    // Цвет свечения задаёт пользователь (параметр «glow»), яркость выставляет расчёт: setLight(уровни 0…1).
    // У светодиода, лампы, неоновой лампы, логического выхода — один уровень; у сегментного индикатора — по числу
    // ячейкам цифры (у 7 сегментов a…g, затем DP; у нескольких разрядов — блок на каждый разряд подряд); у светодиодной
    // матрицы — по ячейкам, строка за строкой. Светодиод зажигается сам
    // по току из setElectrical.
    static bool isLightEmitter(ElementType type);
    bool isLightEmitter() const { return isLightEmitter(m_type); }
    QColor glowColor() const;
    void setLight(const QVector<double> &levels);
    // Показание для индикаторов с текстом: цифровой индикатор («12.34»), ЖК-экран (строки через \n), подпись измерительных приборов.
    void setReadout(const QString &text);
    const QString &readout() const { return m_readout; }
    void clearLight();

    // --- подсветка проблемы (остановка симуляции) ---------------------------------------------
    // Красный ореол вокруг элемента и красные кольца у выводов из маски; снимается clearProblem().
    void setProblem(quint64 pinMask, bool wholeBody);
    void clearProblem();
    bool hasProblem() const { return m_problemBody || m_problemMask != 0; }
    static void setSimTime(double seconds) { s_simTime = seconds; }
    static double simTime() { return s_simTime; }

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
    void recomputeKarnaugh();
    void updateLabelRect();
    QPointF toScene(qreal x, qreal y) const;     // локальные координаты -> сцена (с учётом flip)
    int stateValue() const;

    ElementType m_type;
    QPoint m_p1;
    QPoint m_p2;
    bool m_flip = false;
    QMap<QString, double> m_params;

    WireNote m_note;
    QPointer<NoteItem> m_noteItem;
    void retraceScene();
    TraceMark m_trace;
    void updateNoteItem();

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

    QPointer<ScopeSource> m_probe;
    quint64 m_uid = 0;
    QVector<double> m_light;
    QString m_readout;
    QVector<quint8> m_cells;               // клетки карты Карно (kKarnaughStride × kKarnaughStride)
    QString m_link;                        // имя связанной карты Карно
    QStringList m_varNames;
    void notifyKarnaugh();
    QString m_formula;                     // выбранная формула карты Карно
    quint64 m_problemMask = 0;
    bool m_problemBody = false;

    static inline qreal s_flowSpeed = 40.0;
    static inline double s_simTime = 0.0;
    static inline quint64 s_nextUid = 0;
};
