#include "widgets/SimulationPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

namespace {

double speedFactor(int v)   { return std::pow(2.0, (v - 50) / 10.0); }   // 1/32 .. 32
double currentFactor(int v) { return std::pow(2.0, (v - 50) / 20.0); }   // ~0.18 .. 5.7

QString factorText(double f)
{
    return QStringLiteral("%1×").arg(f, 0, 'g', 2);
}

} // namespace

SimulationPanel::SimulationPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("simPanel"));
    setAttribute(Qt::WA_StyledBackground, true);

    QSettings s;
    const int speed   = s.value("sim/speed", 50).toInt();
    const int current = s.value("sim/current", 50).toInt();
    const int power   = s.value("sim/power", 50).toInt();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(10);

    auto *reset = new QPushButton(QStringLiteral("Сброс"), this);
    m_run = new QPushButton(QStringLiteral("Старт"), this);
    m_run->setCheckable(true);
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(reset, 1);
    buttons->addWidget(m_run, 1);
    root->addLayout(buttons);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("simStatus"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    setStatus(State::Idle, QStringLiteral("Готова к запуску"));

    m_speed   = addSlider(root, QStringLiteral("Скорость симуляции"), &m_speedValue, speed);
    m_current = addSlider(root, QStringLiteral("Скорость тока"),      &m_currentValue, current);
    m_power   = addSlider(root, QStringLiteral("Яркость мощности"),   &m_powerValue, power);

    auto *note = new QLabel(QStringLiteral(
        "Расчёт цепи появится вместе с ядром (core). Сейчас работает только интерфейс."), this);
    note->setObjectName(QStringLiteral("hint"));
    note->setWordWrap(true);
    root->addWidget(note);

    updateLabels();

    connect(reset, &QPushButton::clicked, this, &SimulationPanel::resetClicked);
    connect(m_run, &QPushButton::toggled, this, [this](bool on) {
        m_run->setText(on ? QStringLiteral("Стоп") : QStringLiteral("Старт"));
        setStatus(on ? State::Running : State::Idle,
                  on ? QStringLiteral("Работает") : QStringLiteral("Остановлена"));
        emit runningChanged(on);
    });
    connect(m_speed, &QSlider::valueChanged, this, [this](int) {
        updateLabels();
        QSettings().setValue("sim/speed", m_speed->value());
        emit simulationSpeedChanged(simulationSpeed());
    });
    connect(m_current, &QSlider::valueChanged, this, [this](int) {
        updateLabels();
        QSettings().setValue("sim/current", m_current->value());
        emit currentSpeedChanged(currentSpeed());
    });
    connect(m_power, &QSlider::valueChanged, this, [this](int) {
        updateLabels();
        QSettings().setValue("sim/power", m_power->value());
        emit powerBrightnessChanged(powerBrightness());
    });
}

void SimulationPanel::setStatus(State state, const QString &text)
{
    const char *name = state == State::Running ? "running" : state == State::Error ? "error" : "idle";
    m_status->setProperty("state", QString::fromLatin1(name));
    m_status->setText(QStringLiteral("● ") + text);
    m_status->style()->unpolish(m_status);                 // перечитать цвет по свойству state
    m_status->style()->polish(m_status);
}

void SimulationPanel::setStopped(const QString &reason)
{
    {
        const QSignalBlocker block(m_run);
        m_run->setChecked(false);
        m_run->setText(QStringLiteral("Старт"));
    }
    setStatus(State::Error, QStringLiteral("Остановлена: ") + reason);
}

QSlider *SimulationPanel::addSlider(QVBoxLayout *lay, const QString &title, QLabel **valueLabel, int value)
{
    auto *row = new QHBoxLayout;
    auto *name = new QLabel(title, this);
    *valueLabel = new QLabel(this);
    (*valueLabel)->setObjectName(QStringLiteral("valueLabel"));
    (*valueLabel)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(name, 1);
    row->addWidget(*valueLabel);
    lay->addLayout(row);

    auto *slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(0, 100);
    slider->setValue(value);
    lay->addWidget(slider);
    return slider;
}

void SimulationPanel::updateLabels()
{
    m_speedValue->setText(factorText(simulationSpeed()));
    m_currentValue->setText(factorText(currentSpeed()));
    m_powerValue->setText(QStringLiteral("%1%").arg(qRound(powerBrightness() * 100)));
}

bool SimulationPanel::isRunning() const { return m_run->isChecked(); }
double SimulationPanel::simulationSpeed() const { return speedFactor(m_speed->value()); }
double SimulationPanel::currentSpeed() const { return currentFactor(m_current->value()); }
double SimulationPanel::powerBrightness() const { return m_power->value() / 100.0; }

void SimulationPanel::setRunning(bool on)
{
    m_run->setChecked(on);
}
