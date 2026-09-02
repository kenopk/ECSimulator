#ifndef PIN_H
#define PIN_H

#include <QPoint>
#include <QPointF>

class Component; // предварительное объявление — Component нам пока не нужен целиком

class Pin
{
public:
    Pin();

    Component *owner = nullptr;   // nullptr = "плавающий" контакт без компонента
    QPoint localOffset;          // используется, если owner != nullptr
    QPoint standalonePos;        // используется, если owner == nullptr

    QPointF scenePos() const;
};

#endif // PIN_H