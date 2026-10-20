#pragma once
// Ток, нагрев и сопротивление проводников, ток плавления, переходные отверстия.
#include "Common.h"
#include "Materials.h"

namespace govno::core::pcb {

// ---------------------------------------------------------------------------------------------
//  Допустимый ток проводника по приросту температуры
// ---------------------------------------------------------------------------------------------
// Ipc2221          — I = k·ΔT^0,44·A^0,725 (A в мил²; k = 0,048 наружный, 0,024 внутренний слой);
// Ipc2152          — тот же вид зависимости с коэффициентом, подогнанным под кривые IPC-2152
//                    (наружный k ≈ 0,0406, внутренний на 10 % меньше). Это инженерная аппроксимация, не сам график;
// Ipc2152Modifiers — то же, умноженное на упрощённые поправки: толщина платы, плоскость рядом, параллельные проводники.
enum class IpcMode { Ipc2221 = 0, Ipc2152 = 1, Ipc2152Modifiers = 2 };

constexpr double kMm2ToMil2 = 1550.0031;
constexpr double kTempExponent = 0.44;
constexpr double kAreaExponent = 0.725;
constexpr double kIpc2152KExternal = 0.0405919;
constexpr double kIpc2152InternalRatio = 0.9;
constexpr double kNoPlaneModifier = 0.85455;      // подгонка под контрольный пример эталонной программы

struct CurrentModifiers {
    double boardMm = 1.5748;       // толщина платы
    bool plane = false;            // рядом есть плоскость меди
    double planeUm = 35.0;         // толщина плоскости
    double planeDistMm = 0.2;      // расстояние до неё
    int parallelCount = 1;         // число одинаковых параллельных проводников рядом
};

inline double currentModifierFactor(const CurrentModifiers &m)
{
    const double board = std::clamp(std::pow(std::max(0.05, m.boardMm) / 1.5748, 0.1), 0.8, 1.2);
    double plane = kNoPlaneModifier;
    if (m.plane)
        plane = std::min(1.5, kNoPlaneModifier + 0.55 * std::sqrt(std::max(0.0, m.planeUm) / 35.0)
                                                  * std::exp(-std::max(0.0, m.planeDistMm) / 0.4));
    const double par = 1.0 / (1.0 + 0.08 * std::max(0, m.parallelCount - 1));
    return board * plane * par;
}

inline double currentForArea(IpcMode mode, bool external, double areaMm2, double tempRise, const CurrentModifiers &mod = {})
{
    if (!(areaMm2 > 0) || !(tempRise > 0))
        return 0.0;
    double k;
    if (mode == IpcMode::Ipc2221)
        k = external ? 0.048 : 0.024;
    else
        k = external ? kIpc2152KExternal : kIpc2152KExternal * kIpc2152InternalRatio;
    double i = k * std::pow(tempRise, kTempExponent) * std::pow(areaMm2 * kMm2ToMil2, kAreaExponent);
    if (mode == IpcMode::Ipc2152Modifiers)
        i *= currentModifierFactor(mod);
    return i;
}

// Толщина скин-слоя меди, мкм (бесконечность на постоянном токе).
inline double skinDepthUm(double freqHz)
{
    if (!(freqHz > 0))
        return std::numeric_limits<double>::infinity();
    return std::sqrt(kRhoCopper20 / (kPi * freqHz * kMu0)) * 1e6;
}

struct ConductorInputs {
    IpcMode mode = IpcMode::Ipc2152Modifiers;
    bool external = true;
    EtchFactor etch = EtchFactor::TwoToOne;
    double widthMm = 1.27;
    double lengthMm = 25.4;
    double thicknessMm = 0.053;     // полная толщина меди (база + гальваника)
    double freqHz = 1e6;            // 0 — постоянный ток
    double loadA = 5.0;
    double tempRise = 20.0;
    double ambient = 22.0;
    double tg = 130.0;
    CurrentModifiers modifiers;
    bool solveWidth = false;        // true: по току loadA найти ширину (иначе по ширине найти ток)
};

struct ConductorResult {
    bool valid = false;
    double widthMm = 0;             // ширина, использованная в расчёте (при solveWidth — найденная)
    double crossMm2 = 0;
    double rDc = 0;                 // Ом (при 85 °C, как в эталонной программе)
    double maxCurrentA = 0;         // ток, вызывающий заданный нагрев
    double powerW = 0, powerDbm = 0;
    double vDrop = 0, vDropLoaded = 0;
    double skinDepthUm = 0, skinPercent = 100;
    double tempC = 0;               // температура проводника при токе maxCurrentA (окружение + прирост)
    double loadedTempC = 0;         // температура при токе нагрузки
    double currentDensity = 0;      // А/мм²
    bool overTg = false;
};

inline double conductorMaxCurrent(const ConductorInputs &in, double widthMm)
{
    const double a = conductorCrossSectionMm2(widthMm, in.thicknessMm, in.etch);
    CurrentModifiers m = in.modifiers;
    return currentForArea(in.mode, in.external, a, in.tempRise, m);
}

inline ConductorResult solveConductor(const ConductorInputs &in)
{
    ConductorResult r;
    if (!(in.thicknessMm > 0) || !(in.tempRise > 0) || !(in.lengthMm >= 0))
        return r;

    double w = in.widthMm;
    if (in.solveWidth) {
        if (!(in.loadA > 0))
            return r;
        double lo = in.thicknessMm * 0.6, hi = 2000.0;            // мм
        if (conductorMaxCurrent(in, hi) < in.loadA)
            return r;
        for (int i = 0; i < 80; ++i) {
            const double mid = 0.5 * (lo + hi);
            (conductorMaxCurrent(in, mid) < in.loadA ? lo : hi) = mid;
        }
        w = 0.5 * (lo + hi);
    }
    if (!(w > 0))
        return r;

    r.widthMm = w;
    r.crossMm2 = conductorCrossSectionMm2(w, in.thicknessMm, in.etch);
    if (!(r.crossMm2 > 0))
        return r;
    r.valid = true;
    r.maxCurrentA = conductorMaxCurrent(in, w);
    const double rho = copperResistivity(kDcResistanceTempC);
    r.rDc = rho * (in.lengthMm * 1e-3) / (r.crossMm2 * 1e-6);
    r.vDrop = r.maxCurrentA * r.rDc;
    r.vDropLoaded = in.loadA * r.rDc;
    r.powerW = r.maxCurrentA * r.maxCurrentA * r.rDc;
    r.powerDbm = wattsToDbm(r.powerW);
    r.skinDepthUm = skinDepthUm(in.freqHz);
    r.skinPercent = std::isinf(r.skinDepthUm) ? 100.0 : std::min(100.0, r.skinDepthUm / (in.thicknessMm * 1000.0) * 100.0);
    r.tempC = in.ambient + in.tempRise;
    const double ratio = r.maxCurrentA > 0 ? std::max(0.0, in.loadA) / r.maxCurrentA : 0.0;
    r.loadedTempC = in.ambient + in.tempRise * std::pow(ratio, 1.0 / kTempExponent);
    r.currentDensity = r.maxCurrentA / r.crossMm2;
    r.overTg = in.tg > 0 && std::max(r.tempC, r.loadedTempC) > in.tg;
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Ток плавления проводника (уравнение Ондердонка)
// ---------------------------------------------------------------------------------------------
constexpr double kCopperMeltC = 1084.62;

struct FusingResult {
    bool valid = false;
    double crossMm2 = 0;
    double crossCmil = 0;
    double currentA = 0;
};

inline FusingResult fusingCurrent(double widthMm, double thicknessMm, EtchFactor etch, double timeS,
                                  double ambientC, double multiplier)
{
    FusingResult r;
    if (!(widthMm > 0) || !(thicknessMm > 0) || !(timeS > 0))
        return r;
    r.crossMm2 = conductorCrossSectionMm2(widthMm, thicknessMm, etch);
    r.crossCmil = r.crossMm2 / kCircularMilMm2;
    const double x = std::log10(1.0 + (kCopperMeltC - ambientC) / (234.0 + ambientC)) / (33.0 * timeS);
    r.currentA = multiplier * r.crossCmil * std::sqrt(std::max(0.0, x));
    r.valid = r.crossCmil > 0;
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Переходное отверстие
// ---------------------------------------------------------------------------------------------
enum class ViaLayerSet { TwoLayer = 0, MultiLayer = 1, Microvia = 2 };

constexpr double kViaCopperThermalK = 394.0;     // Вт/(м·К)

struct ViaInputs {
    ViaLayerSet layers = ViaLayerSet::MultiLayer;
    double holeMm = 0.254;           // диаметр готового отверстия
    double padMm = 0.508;            // диаметр внутренней площадки
    double antipadMm = 1.016;        // диаметр окна в опорной плоскости
    double heightMm = 1.575;
    double platingMm = 0.0254;
    double er = 4.6;
    double tempRise = 20.0;
    double ambient = 22.0;
    int count = 10;                  // число переходных (для теплового сопротивления)
    IpcMode mode = IpcMode::Ipc2152;
    bool disableHeightEffect = false;
};

struct ViaResult {
    bool valid = false;
    bool hasAc = false;              // ёмкость, импеданс и резонанс — только для многослойной платы и микропереходов
    double capPf = 0, indNh = 0, zOhm = 0, fresHz = 0, stepPs = 0;
    double crossMm2 = 0, rDc = 0, currentA = 0, powerW = 0, powerDbm = 0, vDrop = 0;
    double rthCW = 0, rthPerViaCW = 0, tempC = 0, aspect = 0, density = 0;
};

inline ViaResult solveVia(const ViaInputs &in)
{
    ViaResult r;
    if (!(in.holeMm > 0) || !(in.heightMm > 0) || !(in.platingMm > 0))
        return r;
    r.valid = true;
    const double hIn = in.heightMm / kInchMm, dIn = in.holeMm / kInchMm;

    // Индуктивность (Howard Johnson): L = 5,08·h·(ln(4h/d) + 1), нГн при размерах в дюймах.
    const double arg = 4.0 * hIn / dIn;
    r.indNh = 5.08 * hIn * (std::log(std::max(1.0001, arg)) + 1.0);

    r.hasAc = in.layers != ViaLayerSet::TwoLayer && in.antipadMm > in.padMm && in.padMm > 0;
    if (r.hasAc) {
        // Ёмкость: C = 1,41·Er·T·D1/(D2 − D1), пФ при размерах в дюймах.
        const double d1 = in.padMm / kInchMm, d2 = in.antipadMm / kInchMm;
        r.capPf = 1.41 * in.er * hIn * d1 / (d2 - d1);
        const double lc = r.indNh * 1e-9 * r.capPf * 1e-12;
        r.zOhm = std::sqrt(r.indNh * 1e-9 / (r.capPf * 1e-12));
        r.fresHz = 1.0 / (2.0 * kPi * std::sqrt(lc));
        r.stepPs = 1.1 * std::sqrt(lc) * 1e12;      // T10–90 ≈ 2,2·C·Z/2 на линии 50 Ом
    }

    r.crossMm2 = kPi * in.platingMm * (in.holeMm + in.platingMm);
    r.rDc = copperResistivity(kDcResistanceTempC) * (in.heightMm * 1e-3) / (r.crossMm2 * 1e-6);

    double hf = 1.0;
    if (!in.disableHeightEffect)
        hf = std::clamp(std::pow(in.heightMm / 1.5748, 0.3), 0.5, 1.3);
    r.currentA = currentForArea(in.mode == IpcMode::Ipc2221 ? IpcMode::Ipc2221 : IpcMode::Ipc2152, true,
                                r.crossMm2, in.tempRise) * hf;
    r.powerW = r.currentA * r.currentA * r.rDc;
    r.powerDbm = wattsToDbm(r.powerW);
    r.vDrop = r.currentA * r.rDc;
    r.rthCW = (in.heightMm * 1e-3) / (kViaCopperThermalK * r.crossMm2 * 1e-6);
    r.rthPerViaCW = r.rthCW / std::max(1, in.count);
    r.tempC = in.ambient + in.tempRise;
    r.aspect = in.heightMm / in.holeMm;
    r.density = r.currentA / r.crossMm2;
    return r;
}

// ---------------------------------------------------------------------------------------------
//  Дифференциальные переходные и длина «пенька» (stub)
//  Оценочная модель: пара коаксиалов (переходное в окне плоскости) с поправкой на связь по расстоянию.
// ---------------------------------------------------------------------------------------------
struct ViaStubInputs {
    double drillMm = 0.3;            // диаметр сверла
    double platingMm = 0.025;
    double openingHMm = 0.9;         // окно в плоскости (высота / ширина)
    double openingWMm = 0.9;
    bool fullOpening = false;        // общее окно для обоих отверстий
    double spacingMm = 1.0;          // расстояние между центрами
    double anisotropy = 1.0;         // εxy/εz
    double er = 4.6;
    double baudRate = 10e9;          // бод
};

struct ViaStubResult {
    bool valid = false;
    double zSingle = 0, zdiff = 0, zodd = 0, zeven = 0;
    double maxStubMm = 0;
    double stubResonanceGHz = 0;     // резонанс «пенька» максимальной длины
};

inline ViaStubResult solveViaStub(const ViaStubInputs &in)
{
    ViaStubResult r;
    const double dv = in.drillMm;                                    // внешний диаметр «стакана» — по сверлу
    if (!(dv > 0) || !(in.openingHMm > dv) || !(in.openingWMm > dv) || !(in.spacingMm > dv) || !(in.er >= 1.0)
        || !(in.baudRate > 0))
        return r;
    const double epsXY = in.er * std::max(0.5, in.anisotropy);       // в радиальном направлении поле идёт в плоскости слоя
    const double da = std::sqrt(in.openingHMm * in.openingWMm);      // эквивалентный диаметр окна
    double zc = 60.0 / std::sqrt(epsXY) * std::log(da / dv);         // коаксиал: переходное в окне плоскости
    double zdiff;
    if (in.fullOpening)
        zdiff = 120.0 / std::sqrt(epsXY) * std::acosh(std::max(1.0001, in.spacingMm / dv));   // двухпроводная линия
    else
        zdiff = 2.0 * zc * (1.0 - 0.48 * std::exp(-0.96 * in.spacingMm / std::max(1e-6, da - dv)));
    if (!(zc > 0) || !(zdiff > 0))
        return r;
    r.valid = true;
    r.zSingle = zc;
    r.zdiff = zdiff;
    r.zodd = zdiff / 2.0;
    r.zeven = zc * zc / r.zodd;
    // Ответвление длиннее λ/20 на частоте, равной скорости передачи, заметно искажает сигнал.
    const double v = kC / std::sqrt(in.er);
    r.maxStubMm = v / (20.0 * in.baudRate) * 1000.0;
    r.stubResonanceGHz = v / (4.0 * r.maxStubMm * 1e-3) / 1e9;
    return r;
}

} // namespace govno::core::pcb
