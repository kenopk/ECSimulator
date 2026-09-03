#ifndef PIN_H
#define PIN_H

#include <QPoint>
#include <QPointF>

class Component; // предварительное объявление — Component нам пока не нужен целиком

// Pin — простая структура данных (все поля публичные, инвариантов нет),
// поэтому объявлена как struct, а не class — как и другие такие же
// value-типы в проекте (ResistorGeometry, CopiedWireData).
struct Pin
{
    Component *owner = nullptr;   // nullptr = "плавающий" контакт без компонента
    QPoint localOffset;           // TODO(pin-ownership): пока НЕ используется —
    // scenePos() не учитывает owner/localOffset,
    // реализована только ветка "плавающего" пина
    // (см. standalonePos ниже). Прежде чем
    // присваивать owner, доделайте scenePos().
    QPoint standalonePos;         // используется всегда, пока owner не реализован

    QPointF scenePos() const;
};

#endif // PIN_H