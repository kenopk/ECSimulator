#pragma once
// Линии передачи: микрополосок, полосковые линии, копланарный волновод, дифференциальные пары,
// длина волны и граничная длина проводника.
#include "Common.h"

namespace govno::core::pcb {

// ---------------------------------------------------------------------------------------------
//  Эллиптические интегралы (метод АГС)
// ---------------------------------------------------------------------------------------------
inline double ellipticK(double k)
{
    if (k < 0.0) k = 0.0;
    if (k >= 1.0) return std::numeric_limits<double>::infinity();
    double a = 1.0, b = std::sqrt(1.0 - k * k);
    for (int i = 0; i < 40 && std::fabs(a - b) > 1e-15; ++i) {
        const double an = 0.5 * (a + b);
        b = std::sqrt(a * b);
        a = an;
    }
    return kPi / (2.0 * a);
}
inline double ellipticKp(double k) { return ellipticK(std::sqrt(std::max(0.0, 1.0 - k * k))); }

// ---------------------------------------------------------------------------------------------
//  Результат расчёта линии
// ---------------------------------------------------------------------------------------------
struct LineResult {
    bool valid = false;
    double z0 = 0;          // Ом
    double eeff = 1;        // эффективная проницаемость (на заданной частоте)
    double eeffStatic = 1;  // статическая
    double lo = 0;          // погонная индуктивность, Гн/м
    double co = 0;          // погонная ёмкость, Ф/м
    double tpd = 0;         // погонная задержка, с/м
};

namespace detail {

inline LineResult makeResult(double z0, double eeffStatic, double eeff)
{
    LineResult r;
    if (!(z0 > 0) || !(eeff > 0) || !isValid(z0) || !isValid(eeff))
        return r;
    r.valid = true;
    r.z0 = z0;
    r.eeffStatic = eeffStatic;
    r.eeff = eeff;
    r.tpd = std::sqrt(eeff) / kC;
    r.lo = z0 * r.tpd;
    r.co = r.tpd / z0;
    return r;
}

// Hammerstad–Jensen: сопротивление линии в воздухе (u = W/H).
inline double hjZ01(double u)
{
    const double f = 6.0 + (2.0 * kPi - 6.0) * std::exp(-std::pow(30.666 / u, 0.7528));
    return kEta0 / (2.0 * kPi) * std::log(f / u + std::sqrt(1.0 + 4.0 / (u * u)));
}

// Hammerstad–Jensen: эффективная проницаемость при нулевой толщине проводника.
inline double hjEeff(double u, double er)
{
    const double u4 = u * u * u * u;
    const double a = 1.0 + std::log((u4 + (u / 52.0) * (u / 52.0)) / (u4 + 0.432)) / 49.0
                     + std::log(1.0 + std::pow(u / 18.1, 3.0)) / 18.7;
    const double b = 0.564 * std::pow((er - 0.9) / (er + 3.0), 0.053);
    return (er + 1.0) / 2.0 + (er - 1.0) / 2.0 * std::pow(1.0 + 10.0 / u, -a * b);
}

// Kobayashi: дисперсия эффективной проницаемости (h — высота подложки, м).
inline double kobayashiEeff(double u, double er, double e0, double hMeters, double f)
{
    if (f <= 0 || er <= e0 + 1e-12 || e0 <= 1.0)
        return e0;
    const double fk = kC / (2.0 * kPi * hMeters * std::sqrt(er - e0))
                      * std::atan(er * std::sqrt((e0 - 1.0) / (er - e0)));
    const double f50 = fk / (0.75 + (0.75 - 0.332 * std::pow(er, -1.73)) * u);
    const double m0 = 1.0 + 1.0 / (1.0 + std::sqrt(u)) + 0.32 * std::pow(1.0 / (1.0 + std::sqrt(u)), 3.0);
    double mc = 1.0;
    if (u <= 0.7)
        mc = 1.0 + 1.4 / (1.0 + u) * (0.15 - 0.235 * std::exp(-0.45 * f / f50));
    const double m = std::min(m0 * mc, 2.32);
    return er - (er - e0) / (1.0 + std::pow(f / f50, m));
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
//  Микрополосковая линия: Hammerstad–Jensen с поправкой на толщину проводника
//  и дисперсией Kobayashi. widthMm/heightMm/thicknessMm в мм, частота в Гц.
// ---------------------------------------------------------------------------------------------
inline LineResult microstrip(double widthMm, double heightMm, double thicknessMm, double er, double freqHz)
{
    if (!(widthMm > 0) || !(heightMm > 0) || !(er >= 1.0))
        return {};
    const double u = widthMm / heightMm;
    const double tt = std::max(0.0, thicknessMm) / heightMm;

    double e0, z0;
    if (tt > 1e-6) {
        const double th = std::tanh(std::sqrt(6.517 * u));
        const double du1 = tt / kPi * std::log(1.0 + 4.0 * std::exp(1.0) * th * th / tt);
        const double dur = 0.5 * (1.0 + 1.0 / std::cosh(std::sqrt(er - 1.0))) * du1;
        const double u1 = u + du1, ur = u + dur;
        const double zr = detail::hjZ01(ur);
        e0 = detail::hjEeff(ur, er) * (detail::hjZ01(u1) / zr) * (detail::hjZ01(u1) / zr);
        z0 = zr / std::sqrt(e0);
    } else {
        e0 = detail::hjEeff(u, er);
        z0 = detail::hjZ01(u) / std::sqrt(e0);
    }
    const double ef = detail::kobayashiEeff(u, er, e0, heightMm * 1e-3, freqHz);
    return detail::makeResult(z0 * std::sqrt(e0 / ef), e0, ef);
}

// Только эффективная проницаемость (страница «Эффективная Er»).
inline double microstripEeff(double widthMm, double heightMm, double thicknessMm, double er, double freqHz)
{
    const LineResult r = microstrip(widthMm, heightMm, thicknessMm, er, freqHz);
    return r.valid ? r.eeff : kNaN;
}

// IPC-2141: Z0 = 87/√(Er+1,41)·ln(5,98·H/(0,8·W+T)). Применима при 0,1 < W/H < 2 (за пределами — менее точна).
inline double ipc2141Microstrip(double widthMm, double heightMm, double thicknessMm, double er)
{
    const double arg = 5.98 * heightMm / (0.8 * widthMm + thicknessMm);
    if (!(arg > 1.0001) || !(er > -1.41))
        return 0.0;
    return 87.0 / std::sqrt(er + 1.41) * std::log(arg);
}

// IPC-2141: микрополосок, погружённый в диэлектрик. coverMm — слой диэлектрика над проводником (B).
inline LineResult embeddedMicrostrip(double widthMm, double heightMm, double coverMm, double thicknessMm, double er)
{
    if (!(widthMm > 0) || !(heightMm > 0) || !(er >= 1.0))
        return {};
    const double eeff = er * (1.0 - std::exp(-1.55 * std::max(0.0, coverMm) / heightMm));
    const double arg = 5.98 * heightMm / (0.8 * widthMm + thicknessMm);
    if (!(arg > 1.0001) || !(eeff > 0))
        return {};
    return detail::makeResult(60.0 / std::sqrt(eeff) * std::log(arg), eeff, eeff);
}

// IPC-2141: симметричная полосковая линия. heightMm — расстояние до каждой из плоскостей, b = 2H + T.
inline LineResult stripline(double widthMm, double heightMm, double thicknessMm, double er)
{
    if (!(widthMm > 0) || !(heightMm > 0) || !(er >= 1.0))
        return {};
    const double b = 2.0 * heightMm + thicknessMm;
    const double arg = 4.0 * b / (0.67 * kPi * (0.8 * widthMm + thicknessMm));
    if (!(arg > 1.0001))
        return {};
    return detail::makeResult(60.0 / std::sqrt(er) * std::log(arg), er, er);
}

// IPC-2141: асимметричная полосковая линия. h1 — до ближней плоскости, h2 — до дальней.
inline LineResult striplineAsym(double widthMm, double h1Mm, double h2Mm, double thicknessMm, double er)
{
    if (!(widthMm > 0) || !(h1Mm > 0) || !(h2Mm > 0) || !(er >= 1.0))
        return {};
    const double h = std::min(h1Mm, h2Mm), d = std::max(h1Mm, h2Mm);
    const double arg = 1.9 * (2.0 * h + thicknessMm) / (0.8 * widthMm + thicknessMm);
    if (!(arg > 1.0001))
        return {};
    const double z = 80.0 / std::sqrt(er) * std::log(arg) * (1.0 - h / (4.0 * (h + d + thicknessMm)));
    return detail::makeResult(z, er, er);
}

// Две сигнальные полосковые линии между плоскостями: считается ближайший к плоскости слой,
// второй слой (через расстояние spacingMm) играет роль «дальней стенки».
inline LineResult dualStripline(double widthMm, double heightMm, double spacingMm, double thicknessMm, double er)
{
    const double far = spacingMm + thicknessMm + heightMm;
    return striplineAsym(widthMm, heightMm, far, thicknessMm, er);
}

// Копланарный волновод с нижней опорной плоскостью (Simons). gapMm — зазор до копланарной земли.
inline LineResult coplanarWaveguide(double widthMm, double gapMm, double heightMm, double thicknessMm, double er)
{
    if (!(widthMm > 0) || !(gapMm > 0) || !(heightMm > 0) || !(er >= 1.0))
        return {};
    double w = widthMm, s = gapMm;
    if (thicknessMm > 1e-9) {                       // поправка на толщину: проводник «шире», зазор «уже»
        const double dw = 1.25 * thicknessMm / kPi * (1.0 + std::log(4.0 * kPi * widthMm / thicknessMm));
        if (s - dw > 0.05 * s) { w += dw; s -= dw; }
    }
    const double k = w / (w + 2.0 * s);
    const double k1 = std::tanh(kPi * w / (4.0 * heightMm)) / std::tanh(kPi * (w + 2.0 * s) / (4.0 * heightMm));
    const double r1 = ellipticKp(k) / ellipticK(k);          // K'(k)/K(k)
    const double r2 = ellipticK(k1) / ellipticKp(k1);        // K(k1)/K'(k1)
    const double eeff = (1.0 + er * r1 * r2) / (1.0 + r1 * r2);
    const double z0 = 60.0 * kPi / std::sqrt(eeff) / (1.0 / r1 + r2);
    return detail::makeResult(z0, eeff, eeff);
}

// ---------------------------------------------------------------------------------------------
//  Тип линии для страницы «Импеданс проводника»
// ---------------------------------------------------------------------------------------------
enum class LineType { Microstrip = 0, MicrostripEmbed, Stripline, StriplineAsym, DualStripline, Coplanar };

struct LineInputs {
    LineType type = LineType::Microstrip;
    double widthMm = 0.4318;
    double heightMm = 0.254;     // до ближней опорной плоскости
    double thicknessMm = 0.053;
    double er = 4.6;
    double freqHz = 500e6;
    double extraMm = 0.254;      // зависит от типа: слой диэлектрика над проводником / расстояние до дальней плоскости /
                                 // расстояние между сигнальными слоями / зазор до копланарной земли
};

inline LineResult solveLine(const LineInputs &in)
{
    switch (in.type) {
    case LineType::Microstrip:      return microstrip(in.widthMm, in.heightMm, in.thicknessMm, in.er, in.freqHz);
    case LineType::MicrostripEmbed: return embeddedMicrostrip(in.widthMm, in.heightMm, in.extraMm, in.thicknessMm, in.er);
    case LineType::Stripline:       return stripline(in.widthMm, in.heightMm, in.thicknessMm, in.er);
    case LineType::StriplineAsym:   return striplineAsym(in.widthMm, in.heightMm, in.extraMm, in.thicknessMm, in.er);
    case LineType::DualStripline:   return dualStripline(in.widthMm, in.heightMm, in.extraMm, in.thicknessMm, in.er);
    case LineType::Coplanar:        return coplanarWaveguide(in.widthMm, in.extraMm, in.heightMm, in.thicknessMm, in.er);
    }
    return {};
}

// ---------------------------------------------------------------------------------------------
//  Дифференциальные пары и перекрёстные помехи (NEXT)
// ---------------------------------------------------------------------------------------------
enum class DiffLayer { EdgeExternal = 0, EdgeInternalSym, EdgeInternalAsym, EdgeEmbedded, BroadsideShielded, BroadsideUnshielded };

struct DiffInputs {
    DiffLayer layer = DiffLayer::EdgeExternal;
    double widthMm = 0.254, spacingMm = 0.127, heightMm = 0.381, thicknessMm = 0.053;
    double er = 4.6;
    double extraMm = 0.381;       // асимметрия: расстояние до дальней плоскости; погружённая: слой над проводниками
    double appliedV = 1.0;
    double coupledLenMm = 2.54;
    double riseTimeS = 1e-9;
};

struct DiffResult {
    bool valid = false;
    bool inRange = true;          // 0,1 < W/H < 3 и 0,1 < S/H < 3
    double zo = 0, zdiff = 0, zodd = 0, zeven = 0;
    double eeff = 1;
    double kbTerm = 0, kbUnterm = 0;          // коэффициенты связи
    double kbTermDb = 0, kbUntermDb = 0;
    double nextTerm = 0, nextUnterm = 0;      // напряжение наводки, В
    double lsatMm = 0;
    double wh = 0, sh = 0;
};

inline DiffResult solveDiffPair(const DiffInputs &in)
{
    DiffResult r;
    if (!(in.widthMm > 0) || !(in.spacingMm > 0) || !(in.heightMm > 0) || !(in.er >= 1.0))
        return r;
    const double W = in.widthMm, S = in.spacingMm, H = in.heightMm, T = in.thicknessMm, er = in.er;
    double zo = 0, zdiff = 0, eeff = er;

    switch (in.layer) {
    case DiffLayer::EdgeExternal: {
        zo = ipc2141Microstrip(W, H, T, er);
        if (zo <= 0)
            zo = microstrip(W, H, T, er, 0).z0;
        eeff = detail::hjEeff(W / H, er);
        zdiff = 2.0 * zo * (1.0 - 0.48 * std::exp(-0.96 * S / H));
        break;
    }
    case DiffLayer::EdgeEmbedded: {
        const LineResult l = embeddedMicrostrip(W, H, in.extraMm, T, er);
        zo = l.z0;
        eeff = l.eeff;
        zdiff = 2.0 * zo * (1.0 - 0.48 * std::exp(-0.96 * S / H));
        break;
    }
    case DiffLayer::EdgeInternalSym: {
        zo = stripline(W, H, T, er).z0;
        zdiff = 2.0 * zo * (1.0 - 0.347 * std::exp(-2.9 * S / (2.0 * H + T)));
        break;
    }
    case DiffLayer::EdgeInternalAsym: {
        zo = striplineAsym(W, H, in.extraMm, T, er).z0;
        zdiff = 2.0 * zo * (1.0 - 0.347 * std::exp(-2.9 * S / (H + in.extraMm + T)));
        break;
    }
    case DiffLayer::BroadsideShielded:
    case DiffLayer::BroadsideUnshielded: {
        // Две полоски друг над другом на расстоянии S: в нечётной моде середина зазора — виртуальная земля,
        // каждая полоска ведёт себя как линия над плоскостью на высоте S/2 в однородном диэлектрике.
        const double eps = in.layer == DiffLayer::BroadsideShielded ? er : (er + 1.0) / 2.0;
        const double weff = W + T / kPi * (1.0 + std::log(std::max(1.0001, 4.0 * kPi * W / std::max(T, 1e-6))));
        const double zodd = detail::hjZ01(std::max(0.05, weff / (S / 2.0))) / std::sqrt(eps);
        zdiff = 2.0 * zodd;
        zo = zdiff / (2.0 * (1.0 - 0.35));        // оценка одиночной линии из дифф. сопротивления (связь ~35 %)
        eeff = eps;
        break;
    }
    }
    if (!(zo > 0) || !(zdiff > 0))
        return r;

    r.valid = true;
    r.zo = zo;
    r.zdiff = zdiff;
    r.zodd = zdiff / 2.0;
    r.zeven = zo * zo / r.zodd;
    r.eeff = eeff;
    r.wh = W / H;
    r.sh = S / H;
    r.inRange = r.wh > 0.1 && r.wh < 3.0 && r.sh > 0.1 && r.sh < 3.0;

    // Коэффициенты связи: Kb' = (Ze − Zodd)/(Ze + Zodd) — линия без согласования, Kb = (Ze − Zo)/(Ze + Zo) — согласованная.
    r.kbUnterm = (r.zeven - r.zodd) / (r.zeven + r.zodd);
    r.kbTerm = (r.zeven - zo) / (r.zeven + zo);
    r.kbTermDb = voltageRatioToDb(r.kbTerm);
    r.kbUntermDb = voltageRatioToDb(r.kbUnterm);

    // Длина насыщения: Lsat = tr·v/2, v = c/√Eeff. Наводка растёт с длиной связи до Lsat и дальше не меняется.
    r.lsatMm = in.riseTimeS * kC / std::sqrt(eeff) * 1000.0 / 2.0;
    const double ratio = r.lsatMm > 0 ? std::min(1.0, in.coupledLenMm / r.lsatMm) : 1.0;
    r.nextTerm = in.appliedV * r.kbTerm * ratio;
    r.nextUnterm = in.appliedV * r.kbUnterm * ratio;
    return r;
}

// Целевые дифференциальные сопротивления популярных интерфейсов, Ом (0 — своё значение).
struct DiffProtocol { const char *name; double zdiff; };
inline const std::vector<DiffProtocol> &diffProtocols()
{
    static const std::vector<DiffProtocol> k = {
        { "Своё значение", 0 },
        { "DDR2 CLK/DQS", 100 },
        { "DDR3 CLK/DQS", 100 },
        { "DDR4 CLK/DQS", 85 },
        { "USB 2.0", 90 },
        { "USB 3.x", 90 },
        { "HDMI", 100 },
        { "DisplayPort", 100 },
        { "LVDS", 100 },
        { "MIPI D-PHY", 100 },
        { "PCI Express", 85 },
        { "SATA / SAS", 100 },
        { "Ethernet (10/100/1000BASE-T)", 100 },
        { "CAN", 120 },
        { "RS-485", 120 },
        { "FireWire (IEEE 1394)", 110 },
        { "PECL / CML", 100 },
    };
    return k;
}

// ---------------------------------------------------------------------------------------------
//  Длина волны и граничная длина проводника
// ---------------------------------------------------------------------------------------------
// Длина волны, м (eeff = 1 — в воздухе).
inline double wavelengthM(double freqHz, double eeff)
{
    if (!(freqHz > 0) || !(eeff > 0))
        return kNaN;
    return kC / (freqHz * std::sqrt(eeff));
}

struct BandwidthResult {
    double bandwidthHz = 0;         // BW = 0,35 / tr
    double speed = 0;               // c/√Er, м/с
    double wavelengthAirM = 0;      // c / BW
    double ipcLengthMm = 0;         // метод IPC-2251
    double freqLengthMm = 0;        // частотный метод
};

// microstrip — тип линии для метода IPC-2251; srFactor — доля длины фронта («0,25 Sr»); lambdaDivide — на сколько делим λ.
inline BandwidthResult bandwidthAndLength(double riseTimeS, double er, bool microstripLine, double srFactor, double lambdaDivide)
{
    BandwidthResult r;
    if (!(riseTimeS > 0) || !(er >= 1.0))
        return r;
    r.bandwidthHz = 0.35 / riseTimeS;
    r.speed = kC / std::sqrt(er);
    r.wavelengthAirM = kC / r.bandwidthHz;
    const double eeff = microstripLine ? 0.475 * er + 0.67 : er;      // IPC-2141
    const double sr = riseTimeS * kC / std::sqrt(eeff);               // длина фронта сигнала в линии, м
    r.ipcLengthMm = srFactor * sr * 1000.0;
    r.freqLengthMm = r.wavelengthAirM / std::max(1.0, lambdaDivide) * 1000.0;
    return r;
}

} // namespace govno::core::pcb
