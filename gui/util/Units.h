#pragma once
#include <QString>

// Инженерные единицы: «4.7 kΩ» <-> 4700.0
namespace Units {

// Форматирует значение с SI-приставкой: format(4700, "Ω") -> "4.7 kΩ".
// Для "%", "°" и безразмерных значений (unit == "") приставки не применяются.
QString format(double value, const QString &unit);

// Разбирает ввод пользователя: "4.7k", "10u", "100 nF", "1e-6", "2,2 мкФ" не
// поддерживается, но "2,2u" и "5 V" — да. Возвращает false при ошибке.
bool parse(const QString &text, const QString &unit, double *out);

} // namespace Units
