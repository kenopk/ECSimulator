#include "pin.h"

// Явный конструктор не нужен: все поля Pin уже имеют in-class инициализаторы
// (owner = nullptr, QPoint по умолчанию — (0,0)), поэтому компилятор сам
// генерирует корректный конструктор по умолчанию (Rule of Zero).

QPointF Pin::scenePos() const {
    // ВНИМАНИЕ: owner/localOffset пока не учитываются — см. TODO в pin.h.
    // Сейчас возвращается только standalonePos, независимо от owner.
    return QPointF(standalonePos.x(), standalonePos.y());
}