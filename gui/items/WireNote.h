#pragma once

#include <QPoint>
#include <QString>

// Комментарий провода. Показывается подсказкой при наведении или (shown) блоком на схеме с линией привязки.
struct WireNote {
    QString text;
    bool shown = false;                    // блок на схеме (иначе — только подсказка при наведении)
    QPoint offset = QPoint(32, -64);       // левый верхний угол блока относительно середины провода
    bool autoText = false;                 // текст создан расчётом (калькуляторы могут его обновлять сами)

    bool isEmpty() const { return text.trimmed().isEmpty(); }
    bool operator==(const WireNote &o) const
    {
        return text == o.text && shown == o.shown && offset == o.offset && autoText == o.autoText;
    }
    bool operator!=(const WireNote &o) const { return !(*this == o); }
};
