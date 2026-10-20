#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>
#include <QStringList>

// Все цвета интерфейса и холста — в одном месте.
struct Palette {
    // холст
    QColor bg, gridMinor, gridMajor;
    QColor wire, element, junction, openPin;
    QColor nearMiss;                       // ярко-красный: слишком близкие, но не соединённые концы
    QColor accent, onAccent;               // onAccent — цвет текста/галочки поверх акцента
    QColor focus;                          // рамка поля ввода в фокусе («лучевой» зелёный в Лаборатории)
    QColor positive, negative, current;
    // интерфейс
    QColor panel, panelAlt, border;
    QColor text, textDim;
};

enum class GridStyle { Dots = 0, Lines = 1, None = 2 };

class Theme : public QObject
{
    Q_OBJECT
public:
    static Theme &instance();

    // Шрифты: интерфейс — Geist / Inter (если не установлены, Segoe UI), номиналы, логи и графики — JetBrains Mono
    // (запасные: Cascadia Mono, Consolas). Файлы .ttf/.otf, положенные в папку «fonts» рядом с программой, подключаются сами.
    static QStringList uiFamilies();
    static QStringList monoFamilies();
    static QFont uiFont(int pixelSize);
    static QFont monoFont(int pixelSize);
    static void loadBundledFonts();

    const Palette &palette() const { return m_palette; }

    // Готовые схемы + последняя — «Свой цвет» (цвет фона выбирает пользователь).
    static QStringList presetNames();
    static int customPresetIndex();
    int  preset() const { return m_preset; }
    void setPreset(int index);

    QColor customBackground() const { return m_customBg; }
    void setCustomBackground(const QColor &color);     // и переключает на «Свой цвет»

    // Свой цвет акцента (синий по умолчанию). Невалидный — стандартный.
    QColor customAccent() const { return m_customAccent; }
    void setCustomAccent(const QColor &color);
    void resetAccent();

    bool isLight() const { return m_light; }           // светлый фон (чёрные линии на белом)

    GridStyle gridStyle() const { return m_gridStyle; }
    void setGridStyle(GridStyle style);

    bool showLabels() const { return m_showLabels; }
    void setShowLabels(bool on);

    bool showJunctions() const { return m_showJunctions; }
    void setShowJunctions(bool on);

    // Подсветка красным концов, которые стоят вплотную к чужому узлу, но не соединены.
    bool showNearMiss() const { return m_showNearMiss; }
    void setShowNearMiss(bool on);

    // Метки дифф. пар: штрихи поперёк трассы по номеру пары (1, 2, 3…) и красный цвет проводника, который короче напарника.
    bool showLengthMarks() const { return m_showLengthMarks; }
    void setShowLengthMarks(bool on);

    // Масштаб напряжения для раскраски проводов (будет задавать ядро).
    double voltageScale() const { return m_voltageScale; }
    void setVoltageScale(double volts) { m_voltageScale = volts > 0 ? volts : 1.0; }

    // Цвет провода по напряжению: зелёный (+), красный (−), серый (≈0).
    QColor voltageColor(double volts) const;

    void load();
    void save() const;

    // Fusion + тёмная палитра + QSS. Вызывается при старте и при смене темы.
    void applyToApplication();
    QString styleSheet() const;

signals:
    void changed();

private:
    Theme();
    void rebuild();
    void commit();   // rebuild + save + apply + emit changed

    Palette   m_palette;
    int       m_preset = 0;
    QColor    m_customBg = QColor("#16202e");
    QColor    m_customAccent;                // невалидный = стандартный
    bool      m_light = false;
    GridStyle m_gridStyle = GridStyle::Dots;
    bool      m_showLabels = true;
    bool      m_showJunctions = true;
    bool      m_showNearMiss = true;
    bool      m_showLengthMarks = true;
    double    m_voltageScale = 5.0;
};
