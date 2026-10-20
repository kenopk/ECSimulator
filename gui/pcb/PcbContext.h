#pragma once

#include "pcb/PcbCore.h"

// Общие настройки раздела «Печатные платы»: то, что стоит на правой панели (медь, материал, температуры,
// единицы) и в окне «Параметры». Страницы получают их целиком и сами выбирают нужное.
struct PcbContext {
    bool imperial = false;               // имперские единицы (мил, дюймы)
    bool useMicrons = false;             // в метрическом режиме показывать микроны вместо миллиметров
    bool manualCopper = false;           // толщину меди вводят числом, а не выбирают из ряда

    double baseCuUm = 18.0;              // базовая медь
    double platingUm = 35.0;             // гальваническое покрытие
    double planeUm = 35.0;               // плоскость: 0,5/1 oz или 2 oz
    bool externalLayer = true;           // проводник на наружном слое

    int materialIndex = 0;
    double er = 4.6;
    double tg = 130.0;

    double tempRise = 20.0;              // допустимый прирост температуры, °C
    double ambient = 22.0;               // температура окружающей среды, °C

    pcbcore::IpcMode ipcMode = pcbcore::IpcMode::Ipc2152Modifiers;
    pcbcore::EtchFactor etch = pcbcore::EtchFactor::TwoToOne;
    bool disableViaHeight = false;       // не учитывать высоту переходного при оценке тока
    double throughAspectLimit = 10.0;    // допустимое отношение высоты к диаметру, X:1
    double microviaAspectLimit = 1.0;

    // Режим единиц для PcbField::setUnitMode: 0 — метрическая, 1 — имперская, 2 — метрическая в микронах.
    int unitMode() const { return imperial ? 1 : (useMicrons ? 2 : 0); }
    double totalCuUm() const { return baseCuUm + platingUm; }
    double totalCuMm() const { return totalCuUm() / 1000.0; }
};

// Что из общих настроек имеет значение на странице; остальное на панели серое.
struct PcbPageConfig {
    bool base = false;
    bool plating = false;
    bool planeThickness = false;         // «Толщина плоскости» (в общем блоке, если страница не даёт свой)
    bool conductorLayer = false;         // «Слой проводника»
    bool er = false;
    bool tg = false;
    bool tempRise = false;
    bool ambient = false;
    bool units = false;
};
