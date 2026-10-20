#pragma once

#include <QColor>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QVector>

#include "util/CoreTypes.h"

class QPainter;

// Условные графические обозначения элементов (ГОСТ 2.721-74 для общих знаков,
// ГОСТ 2.728 / 2.730 / 2.723 / 2.743 / 2.755 / 2.759 — для остальных).
//
// Локальная система координат элемента: ось X идёт от первого вывода (0,0) ко
// второму ((length,0)); ось Y — поперёк оси (в «клетках» сетки это поле v у вывода).
// Вызывающий код делает translate/rotate (и scale(1,-1) для отражения).
namespace SymbolPainter {

constexpr qreal kGrid = 16.0;
constexpr int kMaxPins = 64;       // максимум выводов у одного элемента (матрица 32×32 — 64)

// Положение вывода в локальной системе: u — доля длины (0, 0.5, 1), v — клетки поперёк оси.
struct PinSpec {
    qreal u = 0;
    qreal v = 0;
    qreal du = 0;             // добавка к u·длина в единицах сцены (выводы сверху/снизу — смещение от центра)
    int   side = 0;           // 0 — слева, 1 — справа, 2 — сверху, 3 — снизу (куда «смотрит» вывод внутрь)
};

struct Spec {
    qreal body = 0;           // длина тела вдоль оси (без выводов)
    qreal halfHeight = 14;    // на сколько тело выступает поперёк оси (для области выбора)
    int   minGrid = 4;        // минимальная длина по оси, в клетках
    int   stepGrid = 1;       // шаг изменения длины, в клетках
    bool  axisOnly = false;   // true — только горизонталь/вертикаль
    int   pinCount = 2;
    PinSpec pin[kMaxPins];
};

// «Вариант» (variant) задаёт раскладку выводов у элементов с переменным числом выводов
// (число входов вентиля, объём памяти, размер матрицы, тип индикатора); см. govno::core::variantOf.
// Для остальных элементов variant не нужен (0).
Spec spec(ElementType type, unsigned variant = 0);
int  pinCount(ElementType type, unsigned variant = 0);

// Элементы с одним выводом (земля, шины, метки): куда по умолчанию смотрит вывод-«ножка».
bool   hasSinglePin(ElementType type);
QPoint defaultDirection(ElementType type);          // (0,1) — вниз, (0,-1) — вверх

// --- геометрия -----------------------------------------------------------------
qreal  minLength(ElementType type, unsigned variant = 0);        // в единицах сцены
qreal  defaultLength(ElementType type, unsigned variant = 0);    // длина при простом клике
QPointF pinLocal(ElementType type, int pin, qreal length, bool flip, unsigned variant = 0);
// Направление, в котором уходит вывод внутрь символа (единичный вектор, локальные координаты).
QPointF pinInward(ElementType type, int pin, unsigned variant = 0);
// Клавиша клавиатуры / переключатель DIP под точкой (локальные координаты без учёта отражения):
// номер 1…N, 0 — мимо или у элемента нет таких частей.
int partAt(ElementType type, unsigned variant, qreal length, const QPointF &local);

// Допустима ли пара (a, b) для этого типа без исправлений.
bool isValidGeometry(ElementType type, const QPoint &a, const QPoint &b, unsigned variant = 0);
// Приводит b к допустимому положению относительно a (ось, длина, шаг).
QPoint normalizeEnd(ElementType type, const QPoint &a, const QPoint &b, unsigned variant = 0);

// --- рисование -----------------------------------------------------------------
struct PaintContext {
    QPainter *p = nullptr;
    ElementType type = ElementType::Resistor;
    qreal length = 0;           // длина оси p1 -> p2
    qreal angle = 0;            // поворот оси в градусах (чтобы текст оставался горизонтальным)
    bool flip = false;          // зеркально относительно оси
    int  state = 0;             // положение коммутатора / форма сигнала
    unsigned variant = 0;       // раскладка выводов (см. spec)
    qreal width = 2.0;          // толщина линий
    qreal textScale = 1.0;      // масштаб надписей внутри символа
    QColor body;                // цвет тела
    QColor glow;                // цвет свечения (параметр элемента), если элемент светится
    QVector<double> light;      // яркость каждой светящейся части 0…1 (сегменты, ячейки матрицы); пусто — не светится
    QString readout;            // показание для индикаторов с текстом: цифровой индикатор («12.34»), ЖК-экран (строки через \n)
    int digits = 0;             // число знаков цифрового индикатора
    QVector<quint8> cells;      // клетки карты Карно 16×16 по строкам (0, 1, 2 = X); пусто — все нули
    QString result;             // формула, которую показывает карта Карно под таблицей
    QStringList names;          // имена переменных карты Карно (пусто — A, B, …)
    QColor pin[kMaxPins];       // цвета выводов (по напряжению)
};

// Рисует выводы и тело (кроме провода — его рисует сам элемент).
void paint(const PaintContext &ctx);

// Иконки.
QPixmap icon(ElementType type, int size, const QColor &color);
QPixmap selectIcon(int size, const QColor &color);

} // namespace SymbolPainter
