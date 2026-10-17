#include "scope/ScopeDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "scope/ScopePlot.h"
#include "util/Units.h"

namespace {

int nearestTimeIndex(double perDiv)
{
    const QVector<double> &steps = ScopePlot::timeSteps();
    int best = 0;
    double bestD = 1e300;
    for (int i = 0; i < steps.size(); ++i) {
        const double d = std::fabs(std::log(steps[i]) - std::log(std::max(perDiv, 1e-9)));
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

// Разбор числа с единицей; при ошибке остаётся прежнее значение.
double parseValue(const QLineEdit *edit, const QString &unit, double fallback)
{
    double v = 0;
    return Units::parse(edit->text(), unit, &v) ? v : fallback;
}

} // namespace

ScopeDialog::ScopeDialog(ScopeSource *source, QWidget *parent)
    : QDialog(parent), m_source(source)
{
    setWindowTitle(QStringLiteral("Параметры осциллографа — ") + source->title());
    setMinimumWidth(440);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(6);

    // --- вертикальная шкала
    auto *vBody = new QWidget(this);
    auto *vLay = new QVBoxLayout(vBody);
    vLay->setContentsMargins(18, 0, 0, 0);
    auto *radios = new QHBoxLayout;
    m_auto = new QRadioButton(QStringLiteral("Автоматически"), vBody);
    m_autoMax = new QRadioButton(QStringLiteral("Автоматически (макс. масштаб)"), vBody);
    m_manual = new QRadioButton(QStringLiteral("Вручную"), vBody);
    radios->addWidget(m_auto);
    radios->addWidget(m_autoMax);
    radios->addWidget(m_manual);
    radios->addStretch(1);
    vLay->addLayout(radios);
    auto *form = new QFormLayout;
    m_vMax = new QLineEdit(vBody);
    m_iMax = new QLineEdit(vBody);
    form->addRow(QStringLiteral("Максимальное значение (V)"), m_vMax);
    form->addRow(QStringLiteral("Максимальный ток (A)"), m_iMax);
    vLay->addLayout(form);
    root->addWidget(section(QStringLiteral("Вертикальная шкала"), vBody, true));

    // --- горизонтальная шкала
    auto *hBody = new QWidget(this);
    auto *hLay = new QVBoxLayout(hBody);
    hLay->setContentsMargins(18, 0, 0, 0);
    m_timeAuto = new QCheckBox(QStringLiteral("Автоматически (весь накопленный участок)"), hBody);
    hLay->addWidget(m_timeAuto);
    auto *row = new QHBoxLayout;
    m_timeSlider = new QSlider(Qt::Horizontal, hBody);
    m_timeSlider->setRange(0, ScopePlot::timeSteps().size() - 1);
    m_timeLabel = new QLabel(hBody);
    m_timeLabel->setMinimumWidth(90);
    row->addWidget(m_timeSlider, 1);
    row->addWidget(m_timeLabel);
    hLay->addLayout(row);
    root->addWidget(section(QStringLiteral("Горизонтальная шкала"), hBody, true));

    // --- триггер
    auto *tBody = new QWidget(this);
    auto *tLay = new QVBoxLayout(tBody);
    tLay->setContentsMargins(18, 0, 0, 0);
    m_trigger = new QCheckBox(QStringLiteral("Запускать развёртку по уровню (нужна заданная шкала времени)"), tBody);
    tLay->addWidget(m_trigger);
    auto *tForm = new QFormLayout;
    m_triggerChannel = new QComboBox(tBody);
    m_triggerChannel->addItems({ QStringLiteral("Напряжение"), QStringLiteral("Ток") });
    m_triggerEdge = new QComboBox(tBody);
    m_triggerEdge->addItems({ QStringLiteral("Фронт (рост)"), QStringLiteral("Спад") });
    m_triggerLevel = new QLineEdit(tBody);
    tForm->addRow(QStringLiteral("Канал"), m_triggerChannel);
    tForm->addRow(QStringLiteral("Перепад"), m_triggerEdge);
    tForm->addRow(QStringLiteral("Уровень (В или А)"), m_triggerLevel);
    tLay->addLayout(tForm);
    root->addWidget(section(QStringLiteral("Триггер"), tBody, false));

    // --- каналы
    auto *cBody = new QWidget(this);
    auto *cLay = new QVBoxLayout(cBody);
    cLay->setContentsMargins(18, 0, 0, 0);
    m_showV = new QCheckBox(QStringLiteral("Показывать напряжение"), cBody);
    m_showI = new QCheckBox(QStringLiteral("Показывать ток"), cBody);
    cLay->addWidget(m_showV);
    cLay->addWidget(m_showI);
    root->addWidget(section(QStringLiteral("Параметры каналов"), cBody, false));

    // --- X-Y
    auto *xBody = new QWidget(this);
    auto *xLay = new QVBoxLayout(xBody);
    xLay->setContentsMargins(18, 0, 0, 0);
    m_xy = new QCheckBox(QStringLiteral("Режим X-Y: напряжение по оси X, ток по оси Y"), xBody);
    xLay->addWidget(m_xy);
    root->addWidget(section(QStringLiteral("X-Y каналы"), xBody, false));

    // --- вывод информации
    auto *iBody = new QWidget(this);
    auto *iLay = new QVBoxLayout(iBody);
    iLay->setContentsMargins(18, 0, 0, 0);
    m_showMax = new QCheckBox(QStringLiteral("Максимум на видимом участке (Max=)"), iBody);
    m_infoName = new QCheckBox(QStringLiteral("Название"), iBody);
    m_infoI = new QCheckBox(QStringLiteral("Ток (I)"), iBody);
    m_infoV = new QCheckBox(QStringLiteral("Падение напряжения (Vd)"), iBody);
    m_infoR = new QCheckBox(QStringLiteral("Сопротивление (R)"), iBody);
    m_infoP = new QCheckBox(QStringLiteral("Мощность (P)"), iBody);
    m_infoF = new QCheckBox(QStringLiteral("Частота (f₀)"), iBody);
    for (QCheckBox *c : { m_showMax, m_infoName, m_infoI, m_infoV, m_infoR, m_infoP, m_infoF })
        iLay->addWidget(c);
    root->addWidget(section(QStringLiteral("Вывод информации"), iBody, false));

    // --- название
    auto *nBody = new QWidget(this);
    auto *nLay = new QVBoxLayout(nBody);
    nLay->setContentsMargins(18, 0, 0, 0);
    m_name = new QLineEdit(nBody);
    m_name->setPlaceholderText(source->defaultTitle());
    nLay->addWidget(m_name);
    root->addWidget(section(QStringLiteral("Название осциллограммы"), nBody, false));

    // --- кнопки
    auto *buttons = new QDialogButtonBox(this);
    QPushButton *ok = buttons->addButton(QStringLiteral("OK"), QDialogButtonBox::AcceptRole);
    QPushButton *cancel = buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    QPushButton *defaults = buttons->addButton(QStringLiteral("Сделать по умолчанию"), QDialogButtonBox::ResetRole);
    ok->setDefault(true);
    root->addWidget(buttons);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(defaults, &QPushButton::clicked, this, [this] { load(ScopeSettings()); });

    connect(m_timeSlider, &QSlider::valueChanged, this, [this] { updateTimeLabel(); });
    connect(m_timeAuto, &QCheckBox::toggled, this, [this] { syncEnabled(); });
    connect(m_manual, &QRadioButton::toggled, this, [this] { syncEnabled(); });
    connect(m_trigger, &QCheckBox::toggled, this, [this] { syncEnabled(); });
    connect(m_xy, &QCheckBox::toggled, this, [this] { syncEnabled(); });

    load(source->settings());
    layout()->activate();
    resize(width(), minimumSizeHint().height());
}

QWidget *ScopeDialog::section(const QString &title, QWidget *body, bool expanded)
{
    auto *box = new QWidget(this);
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    auto *head = new QToolButton(box);
    head->setObjectName(QStringLiteral("sectionHead"));
    head->setCheckable(true);
    head->setChecked(expanded);
    head->setToolButtonStyle(Qt::ToolButtonTextOnly);
    QFont f = head->font();
    f.setBold(true);
    head->setFont(f);
    head->setAutoRaise(true);
    auto update = [this, head, body, title](bool on) {
        head->setText((on ? QStringLiteral("[−]  ") : QStringLiteral("[+]  ")) + title);
        body->setVisible(on);
        // Окно подгоняется под содержимое: после сворачивания раздела высота уменьшается, а не остаётся прежней.
        QTimer::singleShot(0, this, [this] {
            if (layout()) {
                layout()->activate();
                resize(width(), minimumSizeHint().height());
            }
        });
    };
    connect(head, &QToolButton::toggled, this, update);
    update(expanded);
    lay->addWidget(head, 0, Qt::AlignLeft);
    lay->addWidget(body);
    return box;
}

void ScopeDialog::load(const ScopeSettings &s)
{
    m_auto->setChecked(s.vscale == VScale::Auto);
    m_autoMax->setChecked(s.vscale == VScale::AutoMax);
    m_manual->setChecked(s.vscale == VScale::Manual);
    m_vMax->setText(Units::format(s.vMax, QStringLiteral("V")));
    m_iMax->setText(Units::format(s.iMax, QStringLiteral("A")));

    m_timeAuto->setChecked(s.timePerDiv <= 0.0);
    m_timeSlider->setValue(nearestTimeIndex(s.timePerDiv > 0 ? s.timePerDiv : 0.01));

    m_trigger->setChecked(s.trigger);
    m_triggerChannel->setCurrentIndex(s.triggerChannel == 1 ? 1 : 0);
    m_triggerEdge->setCurrentIndex(s.triggerRising ? 0 : 1);
    m_triggerLevel->setText(Units::format(s.triggerLevel, QStringLiteral("V")));

    m_showV->setChecked(s.showVoltage);
    m_showI->setChecked(s.showCurrent);
    m_xy->setChecked(s.xy);

    m_showMax->setChecked(s.showMax);
    m_infoName->setChecked(s.infoName);
    m_infoI->setChecked(s.infoCurrent);
    m_infoV->setChecked(s.infoVoltage);
    m_infoR->setChecked(s.infoResistance);
    m_infoP->setChecked(s.infoPower);
    m_infoF->setChecked(s.infoFrequency);

    m_name->setText(s.name);
    updateTimeLabel();
    syncEnabled();
}

ScopeSettings ScopeDialog::collect() const
{
    const ScopeSettings old = m_source->settings();
    ScopeSettings s = old;
    s.vscale = m_manual->isChecked() ? VScale::Manual : m_autoMax->isChecked() ? VScale::AutoMax : VScale::Auto;
    s.vMax = std::max(1e-12, parseValue(m_vMax, QStringLiteral("V"), old.vMax));
    s.iMax = std::max(1e-12, parseValue(m_iMax, QStringLiteral("A"), old.iMax));

    s.timePerDiv = m_timeAuto->isChecked() ? 0.0 : ScopePlot::timeSteps().value(m_timeSlider->value(), 0.01);

    s.trigger = m_trigger->isChecked();
    s.triggerChannel = m_triggerChannel->currentIndex();
    s.triggerRising = m_triggerEdge->currentIndex() == 0;
    s.triggerLevel = parseValue(m_triggerLevel, QStringLiteral("V"), old.triggerLevel);

    s.showVoltage = m_showV->isChecked();
    s.showCurrent = m_showI->isChecked();
    s.xy = m_xy->isChecked();
    if (!s.showVoltage && !s.showCurrent)
        s.showVoltage = true;                 // хотя бы один канал должен быть виден

    s.showMax = m_showMax->isChecked();
    s.infoName = m_infoName->isChecked();
    s.infoCurrent = m_infoI->isChecked();
    s.infoVoltage = m_infoV->isChecked();
    s.infoResistance = m_infoR->isChecked();
    s.infoPower = m_infoP->isChecked();
    s.infoFrequency = m_infoF->isChecked();

    s.name = m_name->text().trimmed();
    return s;
}

void ScopeDialog::updateTimeLabel()
{
    const double t = ScopePlot::timeSteps().value(m_timeSlider->value(), 0.01);
    m_timeLabel->setText(QStringLiteral("%1/кл").arg(Units::format(t, QStringLiteral("с"))));
}

void ScopeDialog::syncEnabled()
{
    const bool manual = m_manual->isChecked();
    m_vMax->setEnabled(manual);
    m_iMax->setEnabled(manual);

    const bool timed = !m_timeAuto->isChecked();
    m_timeSlider->setEnabled(timed && !m_xy->isChecked());
    m_timeLabel->setEnabled(timed && !m_xy->isChecked());

    const bool trig = m_trigger->isChecked();
    m_trigger->setEnabled(timed && !m_xy->isChecked());
    m_triggerChannel->setEnabled(trig && timed);
    m_triggerEdge->setEnabled(trig && timed);
    m_triggerLevel->setEnabled(trig && timed);
}

bool ScopeDialog::edit(QWidget *parent, ScopeSource *source)
{
    if (!source)
        return false;
    ScopeDialog dlg(source, parent);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    source->setSettings(dlg.collect());
    return true;
}
