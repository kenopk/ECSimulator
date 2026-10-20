#pragma once

#include <QFrame>
#include <QGroupBox>
#include <QString>
#include <QVector>
#include <QWidget>
#include <cmath>
#include <limits>

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSlider;

// Общие мелкие виджеты раздела «Печатные платы»: поле «значение + единица», группа переключателей,
// ползунок с подписью и схемы-иллюстрации.

// ---------------------------------------------------------------------------------------------
//  Форматирование чисел. Вывод — в системной локали (запятая или точка), ввод принимает и то и другое.
// ---------------------------------------------------------------------------------------------
namespace PcbFmt {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QString fixed(double v, int decimals);        // фиксированное число знаков после запятой; NaN → «—», ∞ → «∞»
QString trimmed(double v);                    // до 6 значащих цифр без хвостовых нулей (для полей ввода)
bool parse(const QString &text, double *out); // «1,5», «1.5», «1e-3»
QString info(const QString &label, const QString &value, bool warn = false);   // строка панели «Информация»

} // namespace PcbFmt

// Единица измерения поля: подпись и множитель к базовой единице (база = показанное × factor).
struct PcbUnit {
    QString label;
    double factor = 1.0;
};

// Наборы единиц. Режим единиц: 0 — метрическая, 1 — имперская, 2 — метрическая в микронах.
namespace PcbUnits {
QVector<PcbUnit> lengthMm();      // база: мм → мм | мил | мкм
QVector<PcbUnit> lengthUm();      // база: мм → мкм | мил
QVector<PcbUnit> lengthCm();      // база: мм → см | дюйм
QVector<PcbUnit> areaMm2();       // база: мм² → мм² | дюйм²
QVector<PcbUnit> areaSmall();     // база: мм² → мм² | мил²
QVector<PcbUnit> lengthM();       // база: м → м | фут
}

class PcbField : public QFrame
{
    Q_OBJECT
public:
    enum Kind { Input, Output };

    explicit PcbField(Kind kind = Input, QWidget *parent = nullptr);

    // Постоянная подпись единицы («нс», «Ом»…).
    void setUnit(const QString &unit);
    // Единицы привязаны к режиму (мм ↔ мил): значение не меняется, меняется только показ.
    void setUnits(const QVector<PcbUnit> &units);
    void setUnitMode(int mode);
    // Единицы выбирает пользователь (список справа): число остаётся, меняется его смысл.
    void setUnitChoices(const QVector<PcbUnit> &units, int index = 0);
    int unitIndex() const { return m_index; }
    void setUnitIndex(int index);

    void setDecimals(int decimals) { m_decimals = decimals; refresh(); }
    void setEditable(bool on);
    void setPlaceholder(const QString &text);
    // Поле ввода превращается в поле вывода и обратно (для «поменять местами»).
    void setOutputMode(bool output);

    void setValue(double base);          // в базовых единицах поля
    double value() const { return m_base; }     // NaN, если текст не разобрать
    bool hasValue() const { return std::isfinite(m_base); }
    void setText(const QString &text);   // свой текст вместо числа (например «Н/Д»)
    QString text() const;
    QLineEdit *lineEdit() const { return m_edit; }
    bool isOutput() const { return m_kind == Output; }

signals:
    void edited();

private:
    void refresh();
    void onTextEdited();
    void onChoiceChanged(int index);
    double factor() const { return m_units.isEmpty() ? 1.0 : m_units[std::clamp(m_index, 0, int(m_units.size()) - 1)].factor; }

    Kind m_kind;
    QLineEdit *m_edit = nullptr;
    QLabel *m_unitLabel = nullptr;
    QComboBox *m_unitCombo = nullptr;
    QVector<PcbUnit> m_units;
    bool m_choiceMode = false;
    int m_index = 0;
    int m_decimals = 4;
    double m_base = std::numeric_limits<double>::quiet_NaN();
    bool m_textOverride = false;
};

// Группа с переключателями (одно из нескольких): рамка с заголовком, как в исходной программе.
class PcbChoice : public QGroupBox
{
    Q_OBJECT
public:
    explicit PcbChoice(const QString &title, QWidget *parent = nullptr);

    void addOption(const QString &text);
    void setOptions(const QStringList &texts);
    int currentIndex() const;
    void setCurrentIndex(int index);
    void setOptionEnabled(int index, bool on);
    void setOptionVisible(int index, bool on);
    int count() const { return m_buttons.size(); }
    // Раскладка в несколько столбцов (по умолчанию один).
    void setColumns(int columns);

signals:
    void currentChanged(int index);

private:
    void relayout();

    QButtonGroup *m_group = nullptr;
    QVector<QRadioButton *> m_buttons;
    int m_columns = 1;
};

// Ползунок со значением, которое выбирается из списка («0,25 Sr», «1/7 λ»).
class PcbStepSlider : public QWidget
{
    Q_OBJECT
public:
    explicit PcbStepSlider(QWidget *parent = nullptr);

    void setSteps(const QVector<double> &values, const QStringList &labels, int index);
    int index() const;
    double value() const;
    void setIndex(int index);
    PcbField *display() const { return m_display; }

signals:
    void changed();

private:
    QSlider *m_slider = nullptr;
    PcbField *m_display = nullptr;
    QVector<double> m_values;
    QStringList m_labels;
};

// Рамка с заголовком и вертикальной раскладкой; везде, где в оригинале стояла «группа».
QGroupBox *makePcbGroup(const QString &title, QWidget *parent = nullptr);
// Подпись над полем.
QWidget *pcbLabeled(const QString &label, QWidget *field, QWidget *parent = nullptr);

// ---------------------------------------------------------------------------------------------
//  Схемы-иллюстрации (рисуются кодом, цвета платы и меди фиксированные — как на светлых карточках оригинала)
// ---------------------------------------------------------------------------------------------
class PcbDiagram : public QWidget
{
    Q_OBJECT
public:
    enum Kind {
        None, RiseTime, Period, Microstrip, MicrostripEmbed, Stripline, StriplineAsym, DualStripline, Coplanar,
        DiffPair, Via, Planar, Padstack, Pdn, Crystal, ThermalChip, HeatSink, Fusing, EmbeddedResistor,
        Reactance, Spacing, LedBias, OhmWheel
    };

    explicit PcbDiagram(Kind kind = None, QWidget *parent = nullptr);
    void setKind(Kind kind);
    Kind kind() const { return m_kind; }
    QSize sizeHint() const override { return QSize(240, 140); }
    QSize minimumSizeHint() const override { return QSize(160, 96); }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    Kind m_kind;
};
