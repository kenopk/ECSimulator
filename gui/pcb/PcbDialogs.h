#pragma once

#include <QString>

class QWidget;

// Справочные окна раздела «Печатные платы»: таблицы и пояснения (всё считается и хранится локально, без интернета).
namespace PcbDialogs {

void info(QWidget *parent, const QString &title, const QString &html);

void siPrefixes(QWidget *parent);
void dbmChart(QWidget *parent);
void capacitorChart(QWidget *parent);
void resistorCalculator(QWidget *parent);
void designRules(QWidget *parent);
void spacingTable(QWidget *parent);

// Пояснения к страницам.
void speedOfLight(QWidget *parent);
void wavelengthInfo(QWidget *parent);
void erEffectiveInfo(QWidget *parent);
void onderdonkInfo(QWidget *parent);
void wireInfo(QWidget *parent);
void differentialInfo(QWidget *parent);
void conductorInfo(QWidget *parent);
void xtalInfo(QWidget *parent);

// Число с приставкой СИ: 2,2e-8 Вт → «22 нВт».
QString engineering(double value, const QString &unit, int significant = 4);

} // namespace PcbDialogs
