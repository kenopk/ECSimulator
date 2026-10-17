#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class CircuitItem;

// Одна точка осциллограммы: время, напряжение на элементе (вывод 1 относительно вывода 2) и ток.
struct ScopeSample {
    double t = 0;
    double v = 0;
    double i = 0;
};

// Вертикальная шкала: автоматически по видимому участку / по максимуму за всё время / вручную.
enum class VScale { Auto = 0, AutoMax = 1, Manual = 2 };

// Свойства одной осциллограммы (как «Параметры осциллографа»): у каждой свои.
struct ScopeSettings {
    VScale vscale = VScale::Auto;
    double vMax = 20.0;            // вручную: значение у верхнего края, В
    double iMax = 1.0;             // вручную: значение у верхнего края, А

    double timePerDiv = 0.0;       // секунд на клетку; 0 — автоматически (весь накопленный участок)

    bool trigger = false;          // запуск развёртки по уровню (работает при заданной шкале времени)
    int triggerChannel = 0;        // 0 — напряжение, 1 — ток
    double triggerLevel = 0.0;
    bool triggerRising = true;

    bool showVoltage = true;       // каналы
    bool showCurrent = true;
    bool xy = false;               // режим X-Y: напряжение по X, ток по Y

    bool showMax = true;           // вывод информации
    bool infoName = true;
    bool infoCurrent = true;
    bool infoVoltage = true;
    bool infoResistance = true;
    bool infoPower = true;
    bool infoFrequency = true;

    QString name;                  // своё название (пусто — название элемента)
};

// Источник данных осциллограммы для одного элемента схемы. Элемент пишет сюда отсчёты при каждом
// CircuitItem::setElectrical (то есть когда ядро выставляет результат расчёта), а окна осциллограмм —
// на схеме (ScopeItem), в нижней панели и в отдельном окне (ScopeView) — читают их и перерисовываются.
// Свойства осциллограммы хранятся тут же, поэтому у каждого элемента они свои, где бы он ни показывался.
class ScopeSource : public QObject
{
    Q_OBJECT
public:
    static constexpr int kMaxSamples = 4000;      // сколько последних отсчётов хранить

    explicit ScopeSource(CircuitItem *target, QObject *parent = nullptr);

    CircuitItem *target() const { return m_target; }
    bool targetAlive() const { return m_target != nullptr; }

    QString defaultTitle() const { return m_title; }
    QString title() const { return m_settings.name.trimmed().isEmpty() ? m_title : m_settings.name.trimmed(); }

    const ScopeSettings &settings() const { return m_settings; }
    void setSettings(const ScopeSettings &s);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);

    const QVector<ScopeSample> &samples() const { return m_samples; }
    double maxVoltage() const { return m_maxV; }     // наибольшие по модулю значения за всё время записи
    double maxCurrent() const { return m_maxI; }

    void push(double t, double v, double i);
    void clear();
    bool exportCsv(const QString &path, QString *error = nullptr) const;

    // Вызывает сам элемент: удалён / сдвинут (для линии связи на схеме).
    void targetGone();
    void targetMoved() { emit moved(); }

signals:
    void changed();             // новые отсчёты или другие свойства: перерисоваться
    void moved();

private:
    CircuitItem *m_target = nullptr;
    QString m_title;
    ScopeSettings m_settings;
    bool m_enabled = true;
    QVector<ScopeSample> m_samples;
    double m_maxV = 0;
    double m_maxI = 0;
};
