#pragma once

#include <QDialog>

#include "items/WireNote.h"

class CircuitItem;
class QCheckBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QSpinBox;

// Окно «Провод: комментарий и дифф. пара»: текст, показ блоком на схеме, ширина проводника на плате и
// принадлежность дифференциальной паре (галочка + номер пары). Изменения применяет вызывающий код после OK (с отменой).
class WireNoteDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WireNoteDialog(CircuitItem *wire, QWidget *parent = nullptr);

    WireNote note() const;            // с прежним положением блока и пометкой «создан расчётом», если текст не правили
    double widthMm() const;           // ширина на плате, мм (0 — не задана)
    int pairNumber() const;           // номер дифф. пары; 0 — галочка снята

private:
    WireNote m_original;
    QPlainTextEdit *m_text = nullptr;
    QCheckBox *m_shown = nullptr;
    QDoubleSpinBox *m_width = nullptr;
    QCheckBox *m_pair = nullptr;
    QSpinBox *m_pairNumber = nullptr;
};
