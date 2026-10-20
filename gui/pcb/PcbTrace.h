#pragma once

#include <QString>

// Трасса (провод или цепочка проводов) со схемы, с которой связан раздел «Печатные платы»: калькуляторы берут
// из неё длину и ширину, а результат расчёта могут записать в комментарий провода.
struct PcbTrace {
    bool valid = false;
    QString name;              // первая строка комментария или «Провод»
    double lengthMm = 0;       // длина всей трассы по масштабу схемы
    double widthMm = 0;        // ширина проводника на плате (0 — не задана)
    int pair = 0;              // номер дифференциальной пары, в которую помечена трасса (0 — не в паре)
    int pairSize = 1;          // проводников (трасс) в паре
    double skewMm = 0;         // короче (−) или длиннее (+) остальных проводников пары, мм; 0 — длины равны

    bool operator==(const PcbTrace &o) const
    {
        return valid == o.valid && name == o.name && qFuzzyCompare(lengthMm + 1.0, o.lengthMm + 1.0)
               && qFuzzyCompare(widthMm + 1.0, o.widthMm + 1.0) && pair == o.pair && pairSize == o.pairSize
               && qFuzzyCompare(skewMm + 1.0, o.skewMm + 1.0);
    }
};
