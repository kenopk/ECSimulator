#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;

// Управление симуляцией (как правая панель Falstad): Сброс, Старт/Стоп и ползунки
// «Скорость симуляции», «Скорость тока», «Яркость мощности».
// Панель только испускает сигналы — расчёт подключит ядро (core).
// Значения ползунков запоминаются между запусками.
class SimulationPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SimulationPanel(QWidget *parent = nullptr);

    bool isRunning() const;
    double simulationSpeed() const;     // множитель: 1.0 = нормальная скорость
    double currentSpeed() const;        // множитель скорости бегущих точек тока
    double powerBrightness() const;     // 0..1

    void setRunning(bool on);

signals:
    void resetClicked();
    void runningChanged(bool running);
    void simulationSpeedChanged(double factor);
    void currentSpeedChanged(double factor);
    void powerBrightnessChanged(double value);

private:
    QSlider *addSlider(QVBoxLayout *lay, const QString &title, QLabel **valueLabel, int value);
    void updateLabels();

    QPushButton *m_run = nullptr;
    QSlider *m_speed = nullptr;
    QSlider *m_current = nullptr;
    QSlider *m_power = nullptr;
    QLabel *m_speedValue = nullptr;
    QLabel *m_currentValue = nullptr;
    QLabel *m_powerValue = nullptr;
};
