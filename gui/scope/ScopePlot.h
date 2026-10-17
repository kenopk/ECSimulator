#pragma once

#include <QRectF>
#include <QString>
#include <QVector>

#include "scope/ScopeSource.h"

class QPainter;

// Рисование осциллограммы: сетка 10×8 клеток, напряжение и ток своими цветами, шкалы и информация
// по свойствам осциллограммы (ScopeSettings). Общая для нижней панели, отдельного окна и окошка на схеме.
namespace ScopePlot {

struct Options {
    double hover = -1;            // положение курсора по горизонтали внутри графика, 0…1 (или < 0 — нет)
    bool gearHover = false;       // курсор над значком «шестерёнка»
};

void paint(QPainter *p, const QRectF &rect, const ScopeSource *source, const Options &opt);

// Прямоугольник значка «шестерёнка» (параметры осциллографа) в левом нижнем углу графика.
QRectF gearRect(const QRectF &rect);

// Значения шкалы «времени на клетку» (1-2-5), для ползунка в окне свойств.
const QVector<double> &timeSteps();

} // namespace ScopePlot
