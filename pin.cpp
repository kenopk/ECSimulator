#include "pin.h"

Pin::Pin() {}

QPointF Pin::scenePos() const {
    return QPointF(standalonePos.x(), standalonePos.y());
}