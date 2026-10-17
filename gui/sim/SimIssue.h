#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

// Причина, по которой симуляцию нельзя запускать или пришлось остановить.
// Одна и та же структура используется и для проверок интерфейса (резистор перед выводом индикатора),
// и для ошибок ядра (нет земли, вырожденная матрица, не сходится…): ядро формирует список SimIssue,
// интерфейс останавливает симуляцию, подсвечивает виновников красным и пишет журнал —
// см. EditorWindow::stopSimulationWithIssues().
struct SimIssue {
    enum class Severity { Info = 0, Warning = 1, Error = 2 };

    Severity severity = Severity::Error;
    QString code;              // стабильный код причины: "no_series_resistor", "no_ground", …
    QString text;              // что случилось — одной строкой, для баннера и журнала
    QString hint;              // что делать — для журнала
    quint64 itemUid = 0;       // элемент-виновник (CircuitItem::uid()); 0 — проблема всей схемы
    QVector<int> pins;         // выводы-виновники; пусто — подсвечивается весь элемент

    bool isError() const { return severity == Severity::Error; }
};

inline bool hasErrors(const QVector<SimIssue> &issues)
{
    for (const SimIssue &i : issues)
        if (i.isError())
            return true;
    return false;
}
