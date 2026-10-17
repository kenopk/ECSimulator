#include "scope/ScopeMenu.h"

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

#include "scope/ScopeDialog.h"
#include "scope/ScopeSource.h"

namespace ScopeMenu {

void exec(QWidget *parent, ScopeSource *source, const QPoint &globalPos, const Actions &a)
{
    if (!source)
        return;

    QMenu menu(parent);
    QAction *remove = a.remove ? menu.addAction(QStringLiteral("Убрать осциллограф")) : nullptr;
    QAction *move = (a.move && !a.moveText.isEmpty()) ? menu.addAction(a.moveText) : nullptr;
    QAction *window = (a.window && !a.windowText.isEmpty()) ? menu.addAction(a.windowText) : nullptr;
    QAction *maxScale = menu.addAction(QStringLiteral("Макс. масштаб"));

    QAction *stack = nullptr;
    QAction *row = nullptr;
    if (a.arrange) {
        menu.addSeparator();
        stack = menu.addAction(QStringLiteral("Расположить осциллограммы друг над другом"));
        row = menu.addAction(QStringLiteral("Расположить осциллограммы друг рядом с другом"));
        stack->setEnabled(a.current != Qt::Vertical);
        row->setEnabled(a.current != Qt::Horizontal);
    }
    menu.addSeparator();
    QAction *toggle = menu.addAction(source->enabled() ? QStringLiteral("Отключить канал")
                                                       : QStringLiteral("Включить канал"));
    QAction *reset = menu.addAction(QStringLiteral("Сбросить"));
    QAction *csv = menu.addAction(QStringLiteral("Экспорт CSV…"));
    csv->setEnabled(!source->samples().isEmpty());
    menu.addSeparator();
    QAction *props = menu.addAction(QStringLiteral("Параметры…"));

    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;

    if (chosen == remove) {
        a.remove();
    } else if (chosen == move) {
        a.move();
    } else if (chosen == window) {
        a.window();
    } else if (chosen == maxScale) {
        ScopeSettings s = source->settings();
        s.vscale = VScale::AutoMax;
        source->setSettings(s);
    } else if (chosen == stack) {
        a.arrange(Qt::Vertical);
    } else if (chosen == row) {
        a.arrange(Qt::Horizontal);
    } else if (chosen == toggle) {
        source->setEnabled(!source->enabled());
    } else if (chosen == reset) {
        source->clear();
    } else if (chosen == csv) {
        const QString path = QFileDialog::getSaveFileName(parent, QStringLiteral("Экспорт осциллограммы"),
                                                          QStringLiteral("oscillogram.csv"),
                                                          QStringLiteral("CSV (*.csv)"));
        if (!path.isEmpty()) {
            QString err;
            if (!source->exportCsv(path, &err))
                QMessageBox::warning(parent, QStringLiteral("Экспорт CSV"),
                                     QStringLiteral("Не удалось сохранить файл:\n%1").arg(err));
        }
    } else if (chosen == props) {
        ScopeDialog::edit(parent, source);
    }
}

} // namespace ScopeMenu
