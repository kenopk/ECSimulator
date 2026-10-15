#pragma once

#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

#include "util/CoreTypes.h"

class QComboBox;
class QLineEdit;

// Форма параметров элемента: по одному полю на каждый ParamDef из реестра ядра.
// Число вводится в поле, а единица измерения стоит справа выпадающим списком
// (мОм / Ом / кОм …): при смене единицы физическое значение не меняется,
// пересчитывается число в поле (1000 V -> 1 kV). В поле можно писать и суффикс
// (4.7k, 100n, 2.2u) — тогда единица подберётся сама. Перечисления — списком. Форма ничего не знает о схеме: она только сообщает,
// что пользователь изменил (valueEdited), а решает владелец
// (панель свойств применяет сразу, диалог — по кнопке OK).
class ParamForm : public QWidget
{
    Q_OBJECT
public:
    ParamForm(ElementType type, const QMap<QString, double> &values, QWidget *parent = nullptr);

    bool isEmpty() const { return m_fields.isEmpty(); }
    QMap<QString, double> values() const;
    void setValues(const QMap<QString, double> &values);   // без сигналов
    void commitPending();                                  // разобрать недописанный ввод
    void resetToDefaults();                                // значения по умолчанию (шлёт valueEdited)
    void focusFirst();

    // Справка по приставкам и единицам (латиница -> по-русски).
    static QString suffixHelp();
    static QString suffixHelpShort();

signals:
    void valueEdited(const QString &key, double value);

private:
    struct Field {
        QString key;
        QString unit;
        QString shown;           // текст, который мы сами выставили в поле
        double minValue = 0;
        double maxValue = 0;
        double defaultValue = 0;
        double value = 0;
        bool integer = false;    // целое число (число входов, разрядность…)
        int exp = 0;             // степень десяти выбранной приставки (-3 = милли, 3 = кило)
        QVector<int> exps;       // доступные приставки для этой единицы
        QLineEdit *edit = nullptr;
        QComboBox *combo = nullptr;      // перечисление
        QComboBox *unitBox = nullptr;    // единица измерения справа
    };

    void commit(int index);
    void showValue(Field &f, bool autoUnit);

    QVector<Field> m_fields;
};
