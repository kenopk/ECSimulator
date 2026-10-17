#pragma once

#include <QPoint>
#include <QString>
#include <Qt>
#include <functional>

class QWidget;
class ScopeSource;

// Меню правой кнопки осциллограммы (одно и то же на схеме, в нижней панели и в отдельном окне).
// Команды, которые зависят от места показа, приходят обратными вызовами; пустой вызов — пункта нет.
namespace ScopeMenu {

struct Actions {
    std::function<void()> remove;                    // «Убрать осциллограф»
    QString moveText;                                // «Переместить осциллограф в схему» / «… в нижнюю панель»
    std::function<void()> move;
    QString windowText;                              // «Вынести в отдельное окно» / «Вернуть в нижнюю панель»
    std::function<void()> window;
    std::function<void(Qt::Orientation)> arrange;    // расположить осциллограммы нижней панели
    Qt::Orientation current = Qt::Horizontal;        // как сейчас расположены (этот пункт неактивен)
};

void exec(QWidget *parent, ScopeSource *source, const QPoint &globalPos, const Actions &actions);

} // namespace ScopeMenu
