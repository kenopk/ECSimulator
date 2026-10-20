#pragma once
// ============================================================================
//  govno::core::pcb — расчёты для печатных плат («PCB Toolkit»).
//
//  Заголовки БЕЗ зависимостей от Qt: только стандартная библиотека.
//  Единицы на входе и выходе указаны в имени параметра или в комментарии:
//  длины — миллиметры, частоты — герцы, время — секунды, если не сказано иное.
//  Формулы справочные: у каждой отмечено, откуда она взята и где применима.
// ============================================================================
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace govno::core::pcb {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC = 299792458.0;            // скорость света, м/с
constexpr double kMu0 = 4e-7 * kPi;           // магнитная постоянная, Гн/м
constexpr double kEps0 = 8.8541878128e-12;    // электрическая постоянная, Ф/м
constexpr double kEta0 = 376.730313668;       // волновое сопротивление вакуума, Ом
constexpr double kMilMm = 0.0254;             // 1 мил в миллиметрах
constexpr double kInchMm = 25.4;
constexpr double kSqInMm2 = 645.16;           // 1 кв. дюйм в мм²
constexpr double kCircularMilMm2 = 5.0670748e-4;   // 1 circular mil в мм²

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
inline bool isValid(double v) { return std::isfinite(v); }

inline double milToMm(double mil) { return mil * kMilMm; }
inline double mmToMil(double mm) { return mm / kMilMm; }
inline double celsiusToFahrenheit(double c) { return c * 9.0 / 5.0 + 32.0; }
inline double fahrenheitToCelsius(double f) { return (f - 32.0) * 5.0 / 9.0; }
inline double degToRad(double d) { return d * kPi / 180.0; }
inline double radToDeg(double r) { return r * 180.0 / kPi; }

inline double wattsToDbm(double w) { return w > 0 ? 10.0 * std::log10(w / 1e-3) : kNaN; }
inline double dbmToWatts(double dbm) { return 1e-3 * std::pow(10.0, dbm / 10.0); }
inline double voltageRatioToDb(double ratio) { return ratio > 0 ? 20.0 * std::log10(ratio) : kNaN; }
inline double dbToVoltageRatio(double db) { return std::pow(10.0, db / 20.0); }

// Удельное сопротивление меди при 20 °C, Ом·м (для толщины скин-слоя).
constexpr double kRhoCopper20 = 1.72e-8;
// Температурный коэффициент сопротивления меди, 1/°C.
constexpr double kCopperTempCoef = 0.004;
inline double copperResistivity(double tempC) { return kRhoCopper20 * (1.0 + kCopperTempCoef * (tempC - 20.0)); }
// Температура, при которой считается сопротивление постоянному току (как в эталонной программе).
constexpr double kDcResistanceTempC = 85.0;

} // namespace govno::core::pcb
