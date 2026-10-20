#pragma once
// Конструкция платы: зазоры по IPC-2221, падстеки, PDN, планарные катушки, тепловой режим,
// механические таблицы (провод, свёрла, винты) и встроенные резисторы.
#include "Common.h"

namespace govno::core::pcb {

// ---------------------------------------------------------------------------------------------
//  Минимальный зазор между проводниками (IPC-2221, таблица 6-1), мм
// ---------------------------------------------------------------------------------------------
struct SpacingDevice { const char *id; const char *text; };

inline const std::vector<SpacingDevice> &spacingDevices()
{
    static const std::vector<SpacingDevice> k = {
        { "B1", "Внутренние проводники" },
        { "B2", "Внешние, без покрытия, от уровня моря до 3050 м" },
        { "B3", "Внешние, без покрытия, выше 3050 м или в вакууме" },
        { "B4", "Внешние, под паяльной маской (любая высота)" },
        { "B5", "Внешние, с защитным покрытием (любая высота или вакуум)" },
        { "A6", "Выводы компонентов, с покрытием (любая высота или вакуум)" },
        { "A7", "Выводы компонентов, без покрытия, от уровня моря до 3050 м" },
        { "A8", "Выводы компонентов, без покрытия, выше 3050 м или в вакууме" },
    };
    return k;
}

inline const std::vector<const char *> &spacingVoltageClasses()
{
    static const std::vector<const char *> k = { "0 – 15 В", "16 – 30 В", "31 – 50 В", "51 – 100 В", "101 – 150 В",
                                                 "151 – 170 В", "171 – 250 В", "251 – 300 В", "301 – 500 В", "> 500 В" };
    return k;
}

// Номер класса напряжения (индекс spacingVoltageClasses()) для заданного напряжения между проводниками, В.
inline int spacingVoltageClassFor(double volts)
{
    static const double kLimits[] = { 15.0, 30.0, 50.0, 100.0, 150.0, 170.0, 250.0, 300.0, 500.0 };
    for (int i = 0; i < 9; ++i)
        if (volts <= kLimits[i])
            return i;
    return 9;
}

// Значения таблицы (мм) для классов 0…8; для «> 500 В» — прибавка на каждый вольт сверх 500 В (мм/В).
inline double spacingTableMm(int voltageClass, int device)
{
    static const double kTable[9][8] = {
        { 0.05, 0.1,  0.1,  0.05, 0.13, 0.13, 0.13, 0.13 },
        { 0.05, 0.1,  0.1,  0.05, 0.13, 0.13, 0.25, 0.13 },
        { 0.1,  0.6,  0.6,  0.13, 0.13, 0.13, 0.4,  0.13 },
        { 0.1,  0.6,  1.5,  0.13, 0.13, 0.13, 0.5,  0.13 },
        { 0.2,  0.6,  3.2,  0.4,  0.4,  0.4,  0.8,  0.4 },
        { 0.2,  1.25, 3.2,  0.4,  0.4,  0.4,  0.8,  0.4 },
        { 0.2,  1.25, 6.4,  0.4,  0.4,  0.4,  0.8,  0.4 },
        { 0.2,  1.25, 12.5, 0.4,  0.4,  0.4,  0.8,  0.8 },
        { 0.25, 2.5,  12.5, 0.8,  0.8,  0.8,  1.5,  0.8 },
    };
    if (voltageClass < 0 || voltageClass > 8 || device < 0 || device > 7)
        return kNaN;
    return kTable[voltageClass][device];
}
inline double spacingPerVoltMm(int device)
{
    static const double kPerVolt[8] = { 0.0025, 0.005, 0.025, 0.00305, 0.00305, 0.00305, 0.00305, 0.00305 };
    return device >= 0 && device < 8 ? kPerVolt[device] : kNaN;
}

// voltageClass 9 («> 500 В») считает по введённому напряжению.
inline double minConductorSpacingMm(int voltageClass, int device, double volts)
{
    if (voltageClass <= 8)
        return spacingTableMm(voltageClass, device);
    return spacingTableMm(8, device) + std::max(0.0, volts - 500.0) * spacingPerVoltMm(device);
}

// ---------------------------------------------------------------------------------------------
//  Падстек
// ---------------------------------------------------------------------------------------------
struct ThruHolePad {
    double externalMm = 0, internalSignalMm = 0, planeAntipadMm = 0, thermalInnerMm = 0, spokeMm = 0;
};

inline ThruHolePad thruHolePad(double holeMm, double annularRingMm, double isolationMm, bool plated)
{
    ThruHolePad p;
    p.externalMm = holeMm + 2.0 * annularRingMm;
    p.internalSignalMm = plated ? p.externalMm : holeMm + 2.0 * annularRingMm;   // при желании внутренний слой можно делать больше
    p.planeAntipadMm = p.internalSignalMm + 2.0 * isolationMm;
    p.thermalInnerMm = p.internalSignalMm;
    p.spokeMm = p.externalMm < 2.0 ? 0.254 : 0.381;                               // 10 / 15 мил
    return p;
}

// Типовые диаметры шариков BGA, мм.
inline const std::vector<double> &bgaBallDiametersMm()
{
    static const std::vector<double> k = { 0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.76, 0.89, 1.00 };
    return k;
}

struct BgaLand { double nominalMm = 0, minMm = 0, maxMm = 0; };
// Номинальная площадка ≈ 80 % диаметра шарика (NSMD) с допуском ±10 % (ориентир по IPC-7351).
inline BgaLand bgaLand(double ballMm)
{
    BgaLand l;
    l.nominalMm = 0.8 * ballMm;
    l.minMm = 0.72 * ballMm;
    l.maxMm = 0.88 * ballMm;
    return l;
}

// Сколько проводников проходит между площадками: расстояние между центрами минус диаметр площадки,
// остаётся на n проводников шириной w и (n + 1) зазоров.
struct BetweenPads { double maxPadMm = 0, annularRingMm = 0, maxWidthMm = 0; bool ringOk = true; };

inline BetweenPads conductorBetweenThruPads(double pitchMm, double widthMm, double spacingMm, double holeMm,
                                            double minRingMm, int conductors)
{
    BetweenPads r;
    r.maxPadMm = pitchMm - (conductors * widthMm + (conductors + 1) * spacingMm);
    r.annularRingMm = (r.maxPadMm - holeMm) / 2.0;
    r.ringOk = r.annularRingMm >= minRingMm;
    return r;
}

inline double maxWidthBetweenBgaLands(double pitchMm, double landMm, double spacingMm, int conductors)
{
    return (pitchMm - landMm - (conductors + 1) * spacingMm) / conductors;
}

struct CornerToCorner { double diagonalMm = 0, minDrillMm = 0, maxDrillMm = 0; };
inline CornerToCorner cornerToCorner(double aMm, double bMm)
{
    CornerToCorner r;
    r.diagonalMm = std::sqrt(aMm * aMm + bMm * bMm);
    r.minDrillMm = r.diagonalMm + 0.10;
    r.maxDrillMm = r.diagonalMm + 0.25;
    return r;
}

// Типовые диаметры сверл для готовых отверстий, мм.
inline const std::vector<double> &commonHoleDiametersMm()
{
    static const std::vector<double> k = { 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00, 1.10, 1.20, 1.30, 1.50, 1.60, 2.00, 2.50, 3.00 };
    return k;
}

// ---------------------------------------------------------------------------------------------
//  Цепь питания (PDN) и ёмкость плоскостей
// ---------------------------------------------------------------------------------------------
inline double targetPdnImpedance(double voltRail, double ripplePct, double maxCurrentA, double transientPct)
{
    const double di = maxCurrentA * transientPct / 100.0;
    return di > 0 ? voltRail * ripplePct / 100.0 / di : kNaN;
}

// C = 0,225·Er·A/d, пФ (A — кв. дюймы, d — дюймы).
inline double planeCapacitancePf(double er, double areaMm2, double distanceMm)
{
    if (!(distanceMm > 0))
        return kNaN;
    return 0.225 * er * (areaMm2 / kSqInMm2) / (distanceMm / kInchMm);
}

inline double capacitiveReactance(double freqHz, double capF)
{
    return (freqHz > 0 && capF > 0) ? 1.0 / (2.0 * kPi * freqHz * capF) : kNaN;
}

// ---------------------------------------------------------------------------------------------
//  Планарные катушки (модифицированная формула Уилера, Mohan et al.)
// ---------------------------------------------------------------------------------------------
enum class PlanarShape { Square = 0, Hexagonal, Octagonal, Circular };

struct PlanarResult {
    bool valid = false;
    double dinMm = 0;
    double fill = 0;         // ρ = (dout − din)/(dout + din)
    double inductanceH = 0;
};

inline PlanarResult planarInductor(PlanarShape shape, double turns, double widthMm, double spacingMm, double doutMm)
{
    PlanarResult r;
    if (!(turns >= 1) || !(widthMm > 0) || !(spacingMm >= 0) || !(doutMm > 0))
        return r;
    r.dinMm = doutMm - 2.0 * (turns * widthMm + (turns - 1.0) * spacingMm);
    if (!(r.dinMm > 0))
        return r;
    static const double kK[4][2] = { { 2.34, 2.75 }, { 2.33, 3.82 }, { 2.25, 3.55 }, { 2.23, 3.45 } };
    const double k1 = kK[int(shape)][0], k2 = kK[int(shape)][1];
    const double dAvg = 0.5 * (doutMm + r.dinMm) * 1e-3;
    r.fill = (doutMm - r.dinMm) / (doutMm + r.dinMm);
    r.inductanceH = k1 * kMu0 * turns * turns * dAvg / (1.0 + k2 * r.fill);
    r.valid = true;
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Тепловой режим
// ---------------------------------------------------------------------------------------------
inline double junctionTemperature(double ambientC, double thetaCW, double powerW) { return ambientC + thetaCW * powerW; }

// ---------------------------------------------------------------------------------------------
//  Встроенные резисторы (толстоплёночная полимерная паста и т. п.)
// ---------------------------------------------------------------------------------------------
enum class EmbeddedType { Additive = 0, SubtractiveNickel, Annular };

// Прямоугольный резистор: R = Rs·L/W (Rs — поверхностное сопротивление, Ом/кв.).
inline double embeddedResistorOhms(double sheetOhmsPerSq, double lengthMm, double widthMm)
{
    return widthMm > 0 ? sheetOhmsPerSq * lengthMm / widthMm : kNaN;
}
// Кольцевой резистор: R = Rs/(2π)·ln(Dout/Din).
inline double annularResistorOhms(double sheetOhmsPerSq, double outerMm, double innerMm)
{
    return (innerMm > 0 && outerMm > innerMm) ? sheetOhmsPerSq / (2.0 * kPi) * std::log(outerMm / innerMm) : kNaN;
}

// ---------------------------------------------------------------------------------------------
//  Механика: калибры проводов, сверла, винты
// ---------------------------------------------------------------------------------------------
struct WireGauge {
    int awg = 0;              // −3 = 4/0, −2 = 3/0, −1 = 2/0, 0 = 1/0 … 40
    std::string name;
    double diameterMm = 0;
    double chassisAmps = 0;   // допустимый ток при монтаже в шасси
};

inline const std::vector<WireGauge> &wireGauges()
{
    static const std::vector<WireGauge> k = [] {
        static const char *const zeros[] = { "4/0", "3/0", "2/0", "1/0" };
        static const double amps[] = { 380, 328, 283, 245, 211, 181, 158, 135, 118, 101, 89, 73, 64, 55, 47, 41, 35, 32, 28, 22,
                                       19, 16, 14, 11, 9, 7, 4.7, 3.5, 2.7, 2.2, 1.7, 1.4, 1.2, 0.86, 0.7, 0.53, 0.43, 0.33,
                                       0.27, 0.21, 0.17, 0.13, 0.11, 0.09 };
        std::vector<WireGauge> v;
        for (int n = -3; n <= 40; ++n) {
            WireGauge g;
            g.awg = n;
            g.name = n <= 0 ? zeros[n + 3] : std::to_string(n);
            g.diameterMm = 0.127 * std::pow(92.0, (36.0 - n) / 39.0);
            g.chassisAmps = amps[n + 3];
            v.push_back(g);
        }
        return v;
    }();
    return k;
}

constexpr double kWireResistivity = 1.7233e-8;     // Ом·м, медь (как в эталонной программе)

struct WireResult {
    double diameterMm = 0, areaMm2 = 0;
    double ohmsPerKm = 0;
    double chassisAmps = 0;
    double powerTxAmps = 0;       // правило 700 circular mil на ампер
    double voltageDrop = 0;
};
inline WireResult wireGaugeCalc(const WireGauge &g, double loadA, double lengthM)
{
    WireResult r;
    r.diameterMm = g.diameterMm;
    r.areaMm2 = kPi / 4.0 * g.diameterMm * g.diameterMm;
    r.ohmsPerKm = kWireResistivity / (r.areaMm2 * 1e-6) * 1000.0;
    r.chassisAmps = g.chassisAmps;
    const double dMil = g.diameterMm / kMilMm;
    r.powerTxAmps = dMil * dMil / 700.0;
    r.voltageDrop = loadA * r.ohmsPerKm * lengthM / 1000.0;
    return r;
}

struct DrillSize { std::string name; double inches; };

// Таблица сверл: номерные #97…#1 и буквенные A…Z (дюймы).
inline const std::vector<DrillSize> &drillChart()
{
    static const std::vector<DrillSize> k = [] {
        static const double numbered[] = {      // #1 … #107
            0.2280, 0.2210, 0.2130, 0.2090, 0.2055, 0.2040, 0.2010, 0.1990, 0.1960, 0.1935,
            0.1910, 0.1890, 0.1850, 0.1820, 0.1800, 0.1770, 0.1730, 0.1695, 0.1660, 0.1610,
            0.1590, 0.1570, 0.1540, 0.1520, 0.1495, 0.1470, 0.1440, 0.1405, 0.1360, 0.1285,
            0.1200, 0.1160, 0.1130, 0.1110, 0.1100, 0.1065, 0.1040, 0.1015, 0.0995, 0.0980,
            0.0960, 0.0935, 0.0890, 0.0860, 0.0820, 0.0810, 0.0785, 0.0760, 0.0730, 0.0700,
            0.0670, 0.0635, 0.0595, 0.0550, 0.0520, 0.0465, 0.0430, 0.0420, 0.0410, 0.0400,
            0.0390, 0.0380, 0.0370, 0.0360, 0.0350, 0.0330, 0.0320, 0.0310, 0.0292, 0.0280,
            0.0260, 0.0250, 0.0240, 0.0225, 0.0210, 0.0200, 0.0180, 0.0160, 0.0145, 0.0135,
            0.0130, 0.0125, 0.0120, 0.0115, 0.0110, 0.0105, 0.0100, 0.0095, 0.0091, 0.0087,
            0.0083, 0.0079, 0.0075, 0.0071, 0.0067, 0.0063, 0.0059, 0.0055, 0.0052, 0.0049,
            0.0046, 0.0043, 0.0041, 0.0039, 0.0037, 0.0035, 0.0033 };
        static const double letters[] = {        // A … Z
            0.234, 0.238, 0.242, 0.246, 0.250, 0.257, 0.261, 0.266, 0.272, 0.277, 0.281, 0.290, 0.295,
            0.302, 0.316, 0.323, 0.332, 0.339, 0.348, 0.358, 0.368, 0.377, 0.386, 0.397, 0.404, 0.413 };
        std::vector<DrillSize> v;
        for (int n = 97; n >= 1; --n)
            v.push_back({ "#" + std::to_string(n), numbered[n - 1] });
        for (int i = 0; i < 26; ++i)
            v.push_back({ std::string(1, char('A' + i)), letters[i] });
        return v;
    }();
    return k;
}

struct ScrewSize { const char *name; double mm; };
inline const std::vector<ScrewSize> &metricScrews()
{
    static const std::vector<ScrewSize> k = {
        { "M1.6", 1.6 }, { "M1.8", 1.8 }, { "M2.0", 2.0 }, { "M2.2", 2.2 }, { "M2.5", 2.5 }, { "M3.0", 3.0 }, { "M3.5", 3.5 },
        { "M4.0", 4.0 }, { "M5.0", 5.0 }, { "M6.0", 6.0 }, { "M7.0", 7.0 }, { "M8.0", 8.0 }, { "M10", 10 }, { "M12", 12 },
        { "M14", 14 }, { "M16", 16 }, { "M18", 18 }, { "M20", 20 }, { "M22", 22 }, { "M24", 24 }, { "M27", 27 }, { "M30", 30 },
    };
    return k;
}

} // namespace govno::core::pcb
