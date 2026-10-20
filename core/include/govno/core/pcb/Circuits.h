#pragma once
// Электрические расчёты общего назначения: закон Ома, светодиод, цепочки R/L/C, аттенюаторы,
// кварц и PPM, реактивное сопротивление, конвертеры.
#include "Common.h"

namespace govno::core::pcb {

// ---------------------------------------------------------------------------------------------
//  Закон Ома
// ---------------------------------------------------------------------------------------------
enum class OhmSolve { Volts = 0, Amps, Ohms };

struct OhmResult { bool valid = false; double volts = 0, amps = 0, ohms = 0, watts = 0; };

inline OhmResult ohmsLaw(OhmSolve solve, double volts, double amps, double ohms)
{
    OhmResult r;
    switch (solve) {
    case OhmSolve::Volts: volts = amps * ohms; break;
    case OhmSolve::Amps:  if (!(ohms > 0)) return r; amps = volts / ohms; break;
    case OhmSolve::Ohms:  if (!(amps != 0)) return r; ohms = volts / amps; break;
    }
    r.valid = true;
    r.volts = volts; r.amps = amps; r.ohms = ohms;
    r.watts = volts * amps;
    return r;
}

// Токоограничивающий резистор светодиода.
struct LedBias { bool valid = false; double ohms = 0, watts = 0, amps = 0; };

inline LedBias ledResistor(double supplyV, double ledV, double ledA)
{
    LedBias r;
    if (!(ledA > 0) || !(supplyV > ledV))
        return r;
    r.valid = true;
    r.amps = ledA;
    r.ohms = (supplyV - ledV) / ledA;
    r.watts = (supplyV - ledV) * ledA;
    return r;
}
// Ток светодиода при заданном резисторе.
inline LedBias ledCurrent(double supplyV, double ledV, double ohms)
{
    LedBias r;
    if (!(ohms > 0) || !(supplyV > ledV))
        return r;
    r.valid = true;
    r.ohms = ohms;
    r.amps = (supplyV - ledV) / ohms;
    r.watts = (supplyV - ledV) * r.amps;
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Цепочки одинаковых по виду элементов
// ---------------------------------------------------------------------------------------------
inline double sumOf(const std::vector<double> &v) { double s = 0; for (double x : v) s += x; return s; }
inline double reciprocalSumOf(const std::vector<double> &v)
{
    double s = 0;
    for (double x : v) {
        if (!(x > 0)) return kNaN;
        s += 1.0 / x;
    }
    return s > 0 ? 1.0 / s : kNaN;
}
// Резисторы/индуктивности: последовательно — сумма, параллельно — обратная сумма обратных.
// Для конденсаторов наоборот.
inline double seriesSum(const std::vector<double> &v) { return sumOf(v); }
inline double parallelSum(const std::vector<double> &v) { return reciprocalSumOf(v); }

struct ResistorBranch { double ohms = 0, amps = 0, watts = 0, volts = 0; };
struct ResistorNetwork { bool valid = false; double totalOhms = 0, totalAmps = 0, totalWatts = 0; std::vector<ResistorBranch> branches; };

inline ResistorNetwork resistorNetwork(const std::vector<double> &r, bool series, double volts)
{
    ResistorNetwork n;
    n.totalOhms = series ? seriesSum(r) : parallelSum(r);
    if (!(n.totalOhms > 0) || r.empty())
        return n;
    n.valid = true;
    n.totalAmps = volts / n.totalOhms;
    n.totalWatts = volts * n.totalAmps;
    for (double ri : r) {
        ResistorBranch b;
        b.ohms = ri;
        b.volts = series ? n.totalAmps * ri : volts;
        b.amps = series ? n.totalAmps : (ri > 0 ? volts / ri : 0.0);
        b.watts = b.volts * b.amps;
        n.branches.push_back(b);
    }
    return n;
}

// ---------------------------------------------------------------------------------------------
//  Аттенюаторы (Т- и П-образные) при заданных входном и выходном сопротивлении
// ---------------------------------------------------------------------------------------------
struct Attenuator {
    bool valid = false;
    double r1 = 0, r2 = 0, r3 = 0;    // T: r1 — последовательное у входа, r2 — у выхода, r3 — шунт
                                      // П: r1, r2 — шунты у входа/выхода, r3 — последовательное
    double minDb = 0;                 // меньшее затухание такая пара импедансов не позволяет
};

inline double attenuatorMinDb(double zin, double zout)
{
    // минимальные потери согласующего аттенюатора: 20·log10(√(Z1/Z2) + √(Z1/Z2 − 1)), Z1 — большее
    const double hi = std::max(zin, zout), lo = std::min(zin, zout);
    const double x = hi / lo;
    return 20.0 * std::log10(std::sqrt(x) + std::sqrt(std::max(0.0, x - 1.0)));
}

inline Attenuator tAttenuator(double db, double zin, double zout)
{
    Attenuator a;
    if (!(db > 0) || !(zin > 0) || !(zout > 0))
        return a;
    a.minDb = attenuatorMinDb(zin, zout);
    const double n = std::pow(10.0, db / 10.0);            // потери по мощности
    a.r3 = 2.0 * std::sqrt(zin * zout * n) / (n - 1.0);
    a.r1 = zin * (n + 1.0) / (n - 1.0) - a.r3;
    a.r2 = zout * (n + 1.0) / (n - 1.0) - a.r3;
    a.valid = a.r1 > 0 && a.r2 > 0 && a.r3 > 0;
    return a;
}

inline Attenuator piAttenuator(double db, double zin, double zout)
{
    Attenuator a;
    if (!(db > 0) || !(zin > 0) || !(zout > 0))
        return a;
    a.minDb = attenuatorMinDb(zin, zout);
    const double n = std::pow(10.0, db / 10.0);
    a.r3 = (n - 1.0) * std::sqrt(zin * zout) / (2.0 * std::sqrt(n));
    const double g1 = (n + 1.0) / (zin * (n - 1.0)) - 1.0 / a.r3;
    const double g2 = (n + 1.0) / (zout * (n - 1.0)) - 1.0 / a.r3;
    if (g1 > 0 && g2 > 0) {
        a.r1 = 1.0 / g1;
        a.r2 = 1.0 / g2;
        a.valid = true;
    }
    return a;
}

// ---------------------------------------------------------------------------------------------
//  Кварц: нагрузочные конденсаторы и PPM
// ---------------------------------------------------------------------------------------------
// Ёмкость, которую видит кварц: C1·C2/(C1 + C2) + Cпаразитная.
inline double xtalLoadFromCaps(double c1, double c2, double stray)
{
    return (c1 + c2) > 0 ? c1 * c2 / (c1 + c2) + stray : kNaN;
}
// Первое приближение для C1 = C2: 2·(Cнагр − Cпаразитная).
inline double xtalRuleOfThumb(double load, double stray) { return 2.0 * (load - stray); }

struct PpmFromHz { double variationHz = 0, ppm = 0; };
inline PpmFromHz hzToPpm(double centerHz, double maxHz)
{
    PpmFromHz r;
    r.variationHz = maxHz - centerHz;
    r.ppm = centerHz > 0 ? r.variationHz / centerHz * 1e6 : kNaN;
    return r;
}
struct HzFromPpm { double variationHz = 0, maxHz = 0, minHz = 0; };
inline HzFromPpm ppmToHz(double centerHz, double ppm)
{
    HzFromPpm r;
    r.variationHz = centerHz * ppm * 1e-6;
    r.maxHz = centerHz + r.variationHz;
    r.minHz = centerHz - r.variationHz;
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Реактивные сопротивления и резонанс
// ---------------------------------------------------------------------------------------------
struct Reactance { double xc = kNaN, xl = kNaN, resonanceHz = kNaN; };
inline Reactance reactance(double freqHz, double capF, double indH)
{
    Reactance r;
    if (freqHz > 0 && capF > 0) r.xc = 1.0 / (2.0 * kPi * freqHz * capF);
    if (freqHz > 0 && indH > 0) r.xl = 2.0 * kPi * freqHz * indH;
    if (capF > 0 && indH > 0) r.resonanceHz = 1.0 / (2.0 * kPi * std::sqrt(indH * capF));
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Конвертеры
// ---------------------------------------------------------------------------------------------
struct PolarForm { double magnitude = 0, angleDeg = 0; };
inline PolarForm rectToPolar(double re, double im)
{
    return { std::hypot(re, im), radToDeg(std::atan2(im, re)) };
}
inline void polarToRect(double magnitude, double angleDeg, double *re, double *im)
{
    *re = magnitude * std::cos(degToRad(angleDeg));
    *im = magnitude * std::sin(degToRad(angleDeg));
}

// Типовая схема стенда: dBm на выходе = вход + усиление − аттенюатор.
inline double dbmOut(double dbmIn, double gainDb, double attenuatorDb) { return dbmIn + gainDb - attenuatorDb; }

// Сопротивление по цветным полосам: цифры (2 или 3) и десятичный множитель.
inline double resistorFromBands(const std::vector<int> &digits, int multiplierExp)
{
    double v = 0;
    for (int d : digits)
        v = v * 10.0 + d;
    return v * std::pow(10.0, multiplierExp);
}

} // namespace govno::core::pcb
