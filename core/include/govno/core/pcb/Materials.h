#pragma once
#include "Common.h"

namespace govno::core::pcb {

// Типовые подложки: диэлектрическая проницаемость и температура стеклования.
// Значения справочные (типовые по даташитам); у реального материала Er зависит от частоты и смолы.
struct Material {
    const char *name;
    double er;
    double tg;       // °C
};

inline const std::vector<Material> &materials()
{
    static const std::vector<Material> kList = {
        { "FR-4 STD",               4.6, 130.0 },
        { "FR-4 Mid Tg",            4.5, 150.0 },
        { "FR-4 High Tg",           4.4, 170.0 },
        { "FR-4 High Speed",        3.8, 180.0 },
        { "Rogers RO4003C",         3.38, 280.0 },
        { "Rogers RO4350B",         3.48, 280.0 },
        { "Rogers RT/duroid 5880",  2.2, 280.0 },
        { "Полиимид",               4.0, 250.0 },
        { "PTFE (тефлон)",          2.1, 327.0 },
        { "Керамика Al2O3",         9.8, 1000.0 },
        { "Свой материал",          4.6, 130.0 },
    };
    return kList;
}
inline int customMaterialIndex() { return int(materials().size()) - 1; }

// Толщина базовой меди (до гальваники), мкм, и толщина гальванического покрытия, мкм.
inline const std::vector<double> &baseCopperUm() { static const std::vector<double> k = { 9, 18, 35, 53, 70, 88, 106, 142, 178 }; return k; }
inline const std::vector<double> &platingUm() { static const std::vector<double> k = { 0, 18, 35, 53, 70, 88, 106 }; return k; }
// Толщина слоя-плоскости: 0,5 oz / 1 oz (35 мкм) и 2 oz (70 мкм).
inline double planeThicknessUm(bool twoOz) { return twoOz ? 70.0 : 35.0; }

// Выбор формата сечения проводника: прямоугольник или трапеция после травления.
enum class EtchFactor { None = 0, OneToOne = 1, TwoToOne = 2 };

// Площадь сечения проводника, мм² (с учётом травления: верх уже низа).
//   1:1 — подтрав с каждой стороны равен толщине, средняя ширина W − T;
//   2:1 — вдвое меньше, средняя ширина W − T/2.
inline double conductorCrossSectionMm2(double widthMm, double thicknessMm, EtchFactor etch)
{
    double avg = widthMm;
    if (etch == EtchFactor::OneToOne)
        avg = widthMm - thicknessMm;
    else if (etch == EtchFactor::TwoToOne)
        avg = widthMm - thicknessMm / 2.0;
    return std::max(0.0, avg) * thicknessMm;
}

} // namespace govno::core::pcb
