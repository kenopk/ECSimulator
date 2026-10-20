#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QMap>
#include <QString>
#include <QStringList>

class CircuitItem;
class QComboBox;
class ParamForm;

// Окно «Свойства элемента» (двойной клик по элементу): сведения о положении и
// выводах + ВСЕ параметры элемента из реестра ядра (ёмкость, индуктивность,
// допуск, напряжение и т. д.). Изменения применяет вызывающий код после OK.
class ElementDialog : public QDialog
{
    Q_OBJECT
public:
    // mapNames — карты Карно проекта, с которыми можно связать элемент «Карта Карно».
    explicit ElementDialog(CircuitItem *item, QWidget *parent = nullptr, const QStringList &mapNames = QStringList());

    // Значения всех параметров после редактирования.
    QMap<QString, double> values() const;
    bool hasText() const { return m_textEdit != nullptr; }
    QString text() const { return m_textEdit ? m_textEdit->text() : QString(); }
    bool hasLink() const { return m_linkBox != nullptr; }
    QString link() const;                      // имя выбранной карты, пусто — без связи

public slots:
    void accept() override;

private:
    ParamForm *m_form = nullptr;
    QLineEdit *m_textEdit = nullptr;
    QComboBox *m_linkBox = nullptr;
};
