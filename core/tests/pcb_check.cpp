// Проверка расчётов PCB Toolkit по контрольным значениям (скриншоты эталонной программы).
// Сборка и запуск: см. core/CMakeLists.txt (цель pcb_check, ctest) или  g++ -std=c++17 -I core/include core/tests/pcb_check.cpp
#include <cmath>
#include <cstdio>
#include <govno/core/PcbToolkit.h>

using namespace govno::core::pcb;

static int g_fail = 0, g_total = 0;

// relTol — допустимое относительное расхождение; shown — значение на экране эталона.
static void check(const char *what, double got, double shown, double relTol)
{
    ++g_total;
    const double err = shown != 0 ? std::fabs(got - shown) / std::fabs(shown) : std::fabs(got);
    const bool ok = err <= relTol;
    if (!ok)
        ++g_fail;
    std::printf("%-4s %-44s получено %-14.6g эталон %-12.6g откл. %.3f%%\n", ok ? "ok" : "FAIL", what, got, shown, err * 100.0);
}

int main()
{
    // ---- Полоса и длина проводника
    {
        const BandwidthResult b = bandwidthAndLength(1e-9, 4.6, true, 0.25, 7);
        check("BW, МГц", b.bandwidthHz / 1e6, 350.0, 1e-6);
        check("скорость c/√Er, м/с", b.speed, 139778954.280, 1e-8);
        check("λ в воздухе, см", b.wavelengthAirM * 100, 85.65499, 1e-6);
        check("длина по частоте λ/7, см", b.freqLengthMm / 10, 12.23641, 1e-5);
        check("длина IPC-2251, см (приблизительно)", b.ipcLengthMm / 10, 4.50141, 0.02);
    }
    // ---- Импеданс микрополоска
    {
        const LineResult l = microstrip(0.4318, 0.254, 0.053, 4.6, 500e6);
        check("Zo, Ом (приблизительно)", l.z0, 50.7558, 0.012);
        check("Er eff", l.eeff, 3.2809, 0.002);
        check("Lo, нГн/см", l.lo * 1e9 / 100, 3.0674, 0.02);
        check("Co, пФ/см", l.co * 1e12 / 100, 1.1907, 0.02);
        check("Tpd, пс/см", l.tpd * 1e12 / 100, 60.4339, 0.002);
    }
    // ---- Ток проводника
    {
        ConductorInputs in;
        const ConductorResult r = solveConductor(in);
        check("сечение, мм²", r.crossMm2, 0.0659, 0.002);
        check("сопротивление, Ом", r.rDc, 0.00835, 0.002);
        check("ток, А", r.maxCurrentA, 3.7097, 0.002);
        check("напряжение при токе, В", r.vDrop, 0.0310, 0.005);
        check("напряжение при нагрузке, В", r.vDropLoaded, 0.0418, 0.005);
        check("мощность, Вт", r.powerW, 0.11491, 0.005);
        check("скин-слой, мкм", r.skinDepthUm, 66.00620, 1e-5);
        check("температура при нагрузке, °C", r.loadedTempC, 61.4, 0.005);
    }
    // ---- Конвертер
    {
        check("усиление в разах", dbToVoltageRatio(23), 14.1254, 1e-4);
        check("dBm out", dbmOut(-60, 23, 10), -47.0, 1e-9);
        const PolarForm p = rectToPolar(50, 0);
        check("модуль", p.magnitude, 50.0, 1e-9);
        check("рад из 1°", degToRad(1), 0.0175, 0.003);
        check("°F из 72 °C", celsiusToFahrenheit(72), 161.6, 1e-9);
    }
    // ---- Дифференциальная пара
    {
        const DiffResult d = solveDiffPair(DiffInputs());
        check("Zo, Ом", d.zo, 77.551, 1e-4);
        check("Zdiff, Ом", d.zdiff, 101.041, 1e-4);
        check("Zodd, Ом", d.zodd, 50.520, 1e-4);
        check("Zeven, Ом", d.zeven, 119.043, 1e-4);
        check("Kb (согл.)", d.kbTerm, 0.2111, 5e-4);
        check("Kb' (несогл.)", d.kbUnterm, 0.4041, 5e-4);
        check("Kb, дБ", d.kbTermDb, -13.512, 1e-3);
        check("Kb', дБ", d.kbUntermDb, -7.870, 1e-3);
        check("NEXT согл., В", d.nextTerm, 0.0064, 0.01);
        check("NEXT несогл., В", d.nextUnterm, 0.0123, 0.01);
        check("Lsat, мм (приблизительно)", d.lsatMm, 83.55, 0.002);
    }
    // ---- PDN
    {
        check("целевое Z PDN", targetPdnImpedance(5, 5, 2, 50), 0.25, 1e-9);
        const double c = planeCapacitancePf(4.6, 3225.8065, 0.0508);
        check("ёмкость плоскостей, пФ", c, 2587.50, 1e-4);
        check("Xc, Ом", capacitiveReactance(1e6, c * 1e-12), 61.5092, 1e-4);
    }
    // ---- Планарная катушка
    {
        const PlanarResult p = planarInductor(PlanarShape::Square, 5, 0.254, 0.254, 8.890);
        check("внутренний диаметр, мм", p.dinMm, 4.318, 1e-9);
        check("коэффициент заполнения", p.fill, 0.3462, 1e-3);
        check("индуктивность, нГн", p.inductanceH * 1e9, 248.5936, 1e-3);
    }
    // ---- Кварц
    {
        check("по C1–C2, пФ", xtalLoadFromCaps(14, 14, 3), 10.0, 1e-9);
        check("правило, пФ", xtalRuleOfThumb(10, 3), 14.0, 1e-9);
        check("PPM", hzToPpm(32000, 32001).ppm, 31.25, 1e-9);
        const HzFromPpm h = ppmToHz(50e6, 25);
        check("вариация, Гц", h.variationHz, 1250.0, 1e-9);
        check("макс., Гц", h.maxHz, 50001250.0, 1e-9);
        check("мин., Гц", h.minHz, 49998750.0, 1e-9);
    }
    // ---- Тепло
    {
        check("Tj, °C", junctionTemperature(22, 3, 5), 37.0, 1e-9);
        check("Tj радиатора, °C", junctionTemperature(22, 25, 5), 147.0, 1e-9);
    }
    // ---- Переходное отверстие
    {
        const ViaResult v = solveVia(ViaInputs());
        check("C, пФ", v.capPf, 0.4022, 2e-3);
        check("L, нГн", v.indNh, 1.3265, 2e-3);
        check("Z, Ом", v.zOhm, 57.429, 2e-3);
        check("fрез, МГц", v.fresHz / 1e6, 6890.682, 3e-3);
        check("фронт, пс", v.stepPs, 25.4069, 3e-3);
        check("сечение, мм²", v.crossMm2, 0.0223, 3e-3);
        check("R, Ом", v.rDc, 0.00153, 3e-3);
        check("ток, А", v.currentA, 1.9785, 1e-3);
        check("мощность, Вт", v.powerW, 0.00599, 5e-3);
        check("Rth, °C/Вт", v.rthCW, 179.3, 3e-3);
        check("аспект", v.aspect, 6.20, 3e-3);
        check("падение, мВ", v.vDrop * 1e3, 3.0282, 3e-3);
        check("P, дБм", v.powerDbm, 7.7751, 3e-3);
    }
    // ---- Длина волны, реактивности
    {
        check("λ, см", wavelengthM(1.0 / 10e-9, 4.0) * 100, 149.89623, 1e-7);
        const Reactance r = reactance(1e6, 1e-6, 1e-3);
        check("Xc", r.xc, 0.1592, 5e-4);
        check("Xl", r.xl, 6283.18, 1e-6);
        check("fрез", r.resonanceHz, 5032.9255, 1e-5);
    }
    // ---- Ток плавления
    {
        const FusingResult f = fusingCurrent(0.254, 0.053, EtchFactor::TwoToOne, 1.0, 22, 1.0);
        check("сечение, мм²", f.crossMm2, 0.0121, 5e-3);
        check("сечение, cmil", f.crossCmil, 23.80, 2e-3);
        check("ток плавления, А", f.currentA, 3.4949, 1e-3);
    }
    // ---- Провод и механика
    {
        const WireGauge &g = wireGauges().front();
        const WireResult w = wireGaugeCalc(g, 5, 10);
        check("диаметр 4/0, мм", w.diameterMm, 11.6840, 1e-4);
        check("Ом/км", w.ohmsPerKm, 0.16072, 1e-4);
        check("ампераж шасси", w.chassisAmps, 380.0, 1e-9);
        check("700 cmil/А", w.powerTxAmps, 302.286, 1e-5);
        check("падение, В", w.voltageDrop, 0.0080, 5e-3);
        check("число сверл", double(drillChart().size()), 97.0 + 26.0, 1e-9);
        check("сверло #97, дюйм", drillChart().front().inches, 0.0059, 1e-9);
    }
    // ---- Падстек
    {
        const ThruHolePad p = thruHolePad(0.8128, 0.3048, 0.3048, true);
        check("внешний слой, мм", p.externalMm, 1.4224, 1e-9);
        check("плоскость, мм", p.planeAntipadMm, 2.0320, 1e-9);
    }
    // ---- Зазоры
    {
        check("B1, 0–15 В, мм", minConductorSpacingMm(0, 0, 0), 0.05, 1e-9);
    }
    // ---- Закон Ома, светодиод
    {
        const OhmResult o = ohmsLaw(OhmSolve::Volts, 0, 1, 12);
        check("U = I·R", o.volts, 12.0, 1e-9);
        check("P", o.watts, 12.0, 1e-9);
        const LedBias l = ledResistor(12, 2, 0.010);
        check("R светодиода", l.ohms, 1000.0, 1e-9);
        check("P резистора", l.watts, 0.1, 1e-9);
    }
    // ---- Встроенный резистор
    {
        check("R = Rs·L/W", embeddedResistorOhms(25, 2.54, 1.27), 50.0, 1e-9);
    }
    // ---- Аттенюаторы: согласованные 50 Ом, 10 дБ
    {
        const Attenuator t = tAttenuator(10, 50, 50), p = piAttenuator(10, 50, 50);
        check("T: R1", t.r1, 50 * (std::pow(10, 0.5) - 1) / (std::pow(10, 0.5) + 1), 1e-9);
        check("Pi: R3", p.r3, 50 * (10 - 1) / (2 * std::sqrt(10.0)), 1e-9);
    }
    // ---- Обратная задача: ширина по току, сверка прямой и обратной
    {
        ConductorInputs in;
        in.mode = IpcMode::Ipc2152;
        in.loadA = 3.0;
        in.solveWidth = true;
        const ConductorResult r = solveConductor(in);
        check("ширина по току 3 А: ток при найденной ширине, А", r.maxCurrentA, 3.0, 1e-6);
        in.mode = IpcMode::Ipc2221;
        in.solveWidth = false;
        in.widthMm = 1.27;
        in.etch = EtchFactor::None;
        in.thicknessMm = 0.035;
        const ConductorResult r2 = solveConductor(in);
        // IPC-2221, внешний слой, 1 oz, 50 мил, +20 °C: порядка 3,9 А
        check("IPC-2221 внешний 50 мил/1 oz/20 °C, А", r2.maxCurrentA, 3.9, 0.03);
    }
    // ---- Зазоры выше 500 В
    {
        check("B1 501 В, мм", minConductorSpacingMm(9, 0, 501), 0.25 + 0.0025, 1e-9);
        check("A7 150 В, мм", minConductorSpacingMm(4, 6, 0), 0.8, 1e-9);
        check("класс напряжения: 12 В → 0", spacingVoltageClassFor(12.0), 0, 0);
        check("класс напряжения: 150 В → 4", spacingVoltageClassFor(150.0), 4, 0);
        check("класс напряжения: 230 В → 6", spacingVoltageClassFor(230.0), 6, 0);
        check("класс напряжения: 501 В → 9", spacingVoltageClassFor(501.0), 9, 0);
    }
    // ---- Аттенюатор с разными импедансами требует минимального затухания
    {
        const Attenuator bad = tAttenuator(1.0, 50, 200);
        check("T-аттенюатор 1 дБ при 50/200 Ом невозможен", bad.valid ? 1.0 : 0.0, 0.0, 1e-9);
        const Attenuator ok = tAttenuator(20, 50, 200);
        check("T-аттенюатор 20 дБ 50/200 Ом возможен", ok.valid ? 1.0 : 0.0, 1.0, 1e-9);
    }
    // ---- Копланарный волновод — осмысленность (опорные 50 Ом, W=0.3, S=0.15, H=0.2)
    {
        const LineResult c = coplanarWaveguide(0.3, 0.15, 0.2, 0.0, 4.6);
        std::printf("CPWG W=0.3 S=0.15 H=0.2: Z0=%.2f eeff=%.3f\n", c.z0, c.eeff);
        ++g_total;
        if (!(c.z0 > 20 && c.z0 < 120)) ++g_fail;
        const LineResult s = stripline(0.15, 0.2, 0.035, 4.2);
        std::printf("stripline W=0.15 H=0.2: Z0=%.2f\n", s.z0);
    }

    std::printf("\nПроверок: %d, не прошло: %d\n", g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}
