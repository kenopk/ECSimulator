#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QMap>
#include <QString>

class CircuitItem;
class ParamForm;

// Окно «Свойства элемента» (двойной клик по элементу): сведения о положении и
// выводах + ВСЕ параметры элемента из реестра ядра (ёмкость, индуктивность,
// допуск, напряжение и т. д.). Изменения применяет вызывающий код после OK.
class ElementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ElementDialog(CircuitItem *item, QWidget *parent = nullptr);

    // Значения всех параметров после редактирования.
    QMap<QString, double> values() const;
    bool hasText() const { return m_textEdit != nullptr; }
    QString text() const { return m_textEdit ? m_textEdit->text() : QString(); }

public slots:
    void accept() override;

private:
    ParamForm *m_form = nullptr;
    QLineEdit *m_textEdit = nullptr;
};
