#include "pcb/PcbOptions.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

#include "pcb/PcbWidgets.h"

namespace {

QString umText(double um)
{
    return PcbFmt::trimmed(um) + QStringLiteral(" мкм");
}

QProgressBar *makeBar(int lo, int hi, QWidget *parent)
{
    auto *b = new QProgressBar(parent);
    b->setObjectName(QStringLiteral("pcbBar"));
    b->setRange(lo, hi);
    b->setTextVisible(false);
    b->setFixedHeight(8);
    return b;
}

} // namespace

PcbOptions::PcbOptions(QWidget *parent)
    : QWidget(parent)
{
    loadSettings();
    build();
    applyContextToUi();
}

void PcbOptions::build()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);
    auto *left = new QVBoxLayout;
    auto *right = new QVBoxLayout;
    left->setSpacing(8);
    right->setSpacing(8);
    root->addLayout(left, 6);
    root->addLayout(right, 4);

    // --- левый столбец: медь, покрытие, блок страницы ---------------------------------------
    m_base = new PcbChoice(QStringLiteral("Базовая медь"), this);
    for (double um : pcbcore::baseCopperUm())
        m_base->addOption(umText(um));
    m_baseManual = new PcbField(PcbField::Input, this);
    m_baseManual->setUnit(QStringLiteral("мкм"));
    m_baseManual->setMinimumWidth(60);
    static_cast<QGridLayout *>(m_base->layout())->addWidget(m_baseManual, 99, 0);
    m_base->setColumns(2);
    left->addWidget(m_base);

    m_plating = new PcbChoice(QStringLiteral("Гальваническое покрытие"), this);
    for (double um : pcbcore::platingUm())
        m_plating->addOption(um == 0 ? QStringLiteral("Нет") : umText(um));
    m_platingManual = new PcbField(PcbField::Input, this);
    m_platingManual->setUnit(QStringLiteral("мкм"));
    m_platingManual->setMinimumWidth(60);
    static_cast<QGridLayout *>(m_plating->layout())->addWidget(m_platingManual, 99, 0);
    m_plating->setColumns(2);
    left->addWidget(m_plating);

    m_slotHost = new QWidget(this);
    m_slotLayout = new QVBoxLayout(m_slotHost);
    m_slotLayout->setContentsMargins(0, 0, 0, 0);
    m_slotLayout->setSpacing(8);
    left->addWidget(m_slotHost);

    m_defaultSlot = new QWidget(m_slotHost);
    auto *dl = new QVBoxLayout(m_defaultSlot);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(8);
    m_plane = new PcbChoice(QStringLiteral("Толщина плоскости"), m_defaultSlot);
    m_plane->setOptions({ QStringLiteral("0,5 oz / 1 oz"), QStringLiteral("2 oz") });
    m_layer = new PcbChoice(QStringLiteral("Слой проводника"), m_defaultSlot);
    m_layer->setOptions({ QStringLiteral("Внутренний слой"), QStringLiteral("Внешний слой") });
    dl->addWidget(m_plane);
    dl->addWidget(m_layer);
    m_slotLayout->addWidget(m_defaultSlot);
    m_currentSlot = m_defaultSlot;
    left->addStretch(1);

    // --- правый столбец: единицы, материал, температуры, кнопки ----------------------------
    m_units = new PcbChoice(QStringLiteral("Единицы"), this);
    m_units->setOptions({ QStringLiteral("Имперские"), QStringLiteral("Метрические") });
    right->addWidget(m_units);

    m_substrate = makePcbGroup(QStringLiteral("Подложка"), this);
    auto *sg = new QGridLayout(m_substrate);
    sg->setContentsMargins(10, 8, 10, 8);
    sg->setHorizontalSpacing(8);
    sg->setVerticalSpacing(3);
    m_material = new QComboBox(m_substrate);
    for (const pcbcore::Material &m : pcbcore::materials())
        m_material->addItem(QString::fromUtf8(m.name));
    sg->addWidget(m_material, 0, 0, 1, 2);
    auto *erl = new QLabel(QStringLiteral("Er"), m_substrate);
    auto *tgl = new QLabel(QStringLiteral("Tg (°C)"), m_substrate);
    erl->setObjectName(QStringLiteral("pcbLabel"));
    tgl->setObjectName(QStringLiteral("pcbLabel"));
    sg->addWidget(erl, 1, 0);
    sg->addWidget(tgl, 1, 1);
    m_er = new PcbField(PcbField::Input, m_substrate);
    m_tg = new PcbField(PcbField::Input, m_substrate);
    m_er->setMinimumWidth(56);
    m_tg->setMinimumWidth(56);
    sg->addWidget(m_er, 2, 0);
    sg->addWidget(m_tg, 2, 1);
    right->addWidget(m_substrate);

    auto makeTemp = [this, right](const QString &title, QSpinBox *&spin, QProgressBar *&bar, QLabel *&fahr, int lo, int hi,
                           QGroupBox *&box) {
        box = makePcbGroup(title, this);
        auto *g = new QVBoxLayout(box);
        g->setContentsMargins(10, 8, 10, 8);
        g->setSpacing(4);
        spin = new QSpinBox(box);
        spin->setRange(lo, hi);
        spin->setAlignment(Qt::AlignLeft);
        bar = makeBar(lo, hi, box);
        fahr = new QLabel(box);
        fahr->setObjectName(QStringLiteral("pcbHint"));
        g->addWidget(spin);
        g->addWidget(bar);
        g->addWidget(fahr);
        right->addWidget(box);
    };
    makeTemp(QStringLiteral("Прирост темп. (°C)"), m_rise, m_riseBar, m_riseF, 1, 150, m_riseBox);
    makeTemp(QStringLiteral("Окружающая темп. (°C)"), m_amb, m_ambBar, m_ambF, -40, 130, m_ambBox);

    auto *buttons = makePcbGroup(QString(), this);
    auto *bl = new QGridLayout(buttons);
    bl->setContentsMargins(10, 10, 10, 10);
    bl->setSpacing(6);
    auto *settings = new QPushButton(QStringLiteral("Параметры…"), buttons);
    auto *print = new QPushButton(QStringLiteral("Печать"), buttons);
    auto *solve = new QPushButton(QStringLiteral("Расчёт!"), buttons);
    solve->setProperty("primary", true);
    bl->addWidget(settings, 0, 0, 1, 2);
    bl->addWidget(print, 1, 0);
    bl->addWidget(solve, 1, 1);
    right->addWidget(buttons);
    right->addStretch(1);

    connect(settings, &QPushButton::clicked, this, [this] { editSettings(); });
    connect(print, &QPushButton::clicked, this, &PcbOptions::printRequested);
    connect(solve, &QPushButton::clicked, this, &PcbOptions::solveRequested);

    auto changed = [this] {
        if (m_updating)
            return;
        readUi();
        emit contextChanged();
    };
    connect(m_base, &PcbChoice::currentChanged, this, changed);
    connect(m_plating, &PcbChoice::currentChanged, this, changed);
    connect(m_baseManual, &PcbField::edited, this, changed);
    connect(m_platingManual, &PcbField::edited, this, changed);
    connect(m_plane, &PcbChoice::currentChanged, this, changed);
    connect(m_layer, &PcbChoice::currentChanged, this, changed);
    connect(m_units, &PcbChoice::currentChanged, this, changed);
    connect(m_er, &PcbField::edited, this, [this, changed] {
        if (!m_updating) {
            QSignalBlocker b(m_material);
            m_material->setCurrentIndex(pcbcore::customMaterialIndex());
        }
        changed();
    });
    connect(m_tg, &PcbField::edited, this, [this, changed] {
        if (!m_updating) {
            QSignalBlocker b(m_material);
            m_material->setCurrentIndex(pcbcore::customMaterialIndex());
        }
        changed();
    });
    connect(m_material, &QComboBox::currentIndexChanged, this, [this, changed](int) {
        updateMaterialFromCombo();
        changed();
    });
    connect(m_rise, &QSpinBox::valueChanged, this, changed);
    connect(m_amb, &QSpinBox::valueChanged, this, changed);
}

void PcbOptions::updateMaterialFromCombo()
{
    const int i = m_material->currentIndex();
    if (i < 0 || i >= int(pcbcore::materials().size()) || i == pcbcore::customMaterialIndex())
        return;
    const pcbcore::Material &m = pcbcore::materials()[i];
    const bool was = m_updating;
    m_updating = true;
    m_er->setValue(m.er);
    m_tg->setValue(m.tg);
    m_updating = was;
}

void PcbOptions::readUi()
{
    PcbContext c = m_ctx;
    c.manualCopper = m_ctx.manualCopper;
    if (c.manualCopper) {
        if (m_baseManual->hasValue()) c.baseCuUm = std::max(0.0, m_baseManual->value());
        if (m_platingManual->hasValue()) c.platingUm = std::max(0.0, m_platingManual->value());
    } else {
        c.baseCuUm = pcbcore::baseCopperUm().at(std::max(0, m_base->currentIndex()));
        c.platingUm = pcbcore::platingUm().at(std::max(0, m_plating->currentIndex()));
    }
    c.planeUm = pcbcore::planeThicknessUm(m_plane->currentIndex() == 1);
    c.externalLayer = m_layer->currentIndex() == 1;
    c.imperial = m_units->currentIndex() == 0;
    c.materialIndex = m_material->currentIndex();
    if (m_er->hasValue() && m_er->value() >= 1.0) c.er = m_er->value();
    if (m_tg->hasValue() && m_tg->value() > 0) c.tg = m_tg->value();
    c.tempRise = m_rise->value();
    c.ambient = m_amb->value();
    m_ctx = c;
    updateTempLabels();
    saveSettings();
}

void PcbOptions::applyContextToUi()
{
    m_updating = true;
    const auto &bases = pcbcore::baseCopperUm();
    const auto &plats = pcbcore::platingUm();
    const auto b = std::find_if(bases.begin(), bases.end(), [this](double v) { return std::fabs(v - m_ctx.baseCuUm) < 1e-6; });
    const auto p = std::find_if(plats.begin(), plats.end(), [this](double v) { return std::fabs(v - m_ctx.platingUm) < 1e-6; });
    m_base->setCurrentIndex(b == bases.end() ? 1 : int(b - bases.begin()));
    m_plating->setCurrentIndex(p == plats.end() ? 2 : int(p - plats.begin()));
    m_baseManual->setValue(m_ctx.baseCuUm);
    m_platingManual->setValue(m_ctx.platingUm);
    m_base->setVisible(true);
    for (int i = 0; i < m_base->count(); ++i) m_base->setOptionVisible(i, !m_ctx.manualCopper);
    for (int i = 0; i < m_plating->count(); ++i) m_plating->setOptionVisible(i, !m_ctx.manualCopper);
    m_baseManual->setVisible(m_ctx.manualCopper);
    m_platingManual->setVisible(m_ctx.manualCopper);
    m_plane->setCurrentIndex(m_ctx.planeUm > 50.0 ? 1 : 0);
    m_layer->setCurrentIndex(m_ctx.externalLayer ? 1 : 0);
    m_units->setCurrentIndex(m_ctx.imperial ? 0 : 1);
    m_material->setCurrentIndex(std::clamp(m_ctx.materialIndex, 0, int(pcbcore::materials().size()) - 1));
    m_er->setValue(m_ctx.er);
    m_tg->setValue(m_ctx.tg);
    m_rise->setValue(int(std::lround(m_ctx.tempRise)));
    m_amb->setValue(int(std::lround(m_ctx.ambient)));
    updateTempLabels();
    m_updating = false;
}

void PcbOptions::updateTempLabels()
{
    m_riseBar->setValue(m_rise->value());
    m_ambBar->setValue(m_amb->value());
    m_riseF->setText(QStringLiteral("Прирост (°F) = %1").arg(PcbFmt::fixed(m_rise->value() * 1.8, 1)));
    m_ambF->setText(QStringLiteral("Температура (°F) = %1").arg(PcbFmt::fixed(m_amb->value() * 1.8 + 32.0, 1)));
}

void PcbOptions::setPageConfig(const PcbPageConfig &cfg, QWidget *pageOptions)
{
    m_base->setEnabled(cfg.base);
    m_baseManual->setEnabled(cfg.base);
    m_plating->setEnabled(cfg.plating);
    m_platingManual->setEnabled(cfg.plating);
    m_units->setEnabled(cfg.units);
    m_substrate->setEnabled(cfg.er || cfg.tg);
    m_er->setEnabled(cfg.er);
    m_tg->setEnabled(cfg.tg);
    m_riseBox->setEnabled(cfg.tempRise);
    m_ambBox->setEnabled(cfg.ambient);
    m_plane->setEnabled(cfg.planeThickness);
    m_layer->setEnabled(cfg.conductorLayer);

    QWidget *wanted = pageOptions ? pageOptions : m_defaultSlot;
    if (wanted != m_currentSlot) {
        if (m_currentSlot) {
            m_slotLayout->removeWidget(m_currentSlot);
            m_currentSlot->hide();
        }
        m_slotLayout->addWidget(wanted);
        wanted->show();
        m_currentSlot = wanted;
    }
}

void PcbOptions::setPlaneThicknessEnabled(bool on)
{
    m_plane->setEnabled(on);
}

void PcbOptions::editSettings()
{
    PcbSettingsDialog dlg(m_ctx, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const PcbContext r = dlg.result();
    m_ctx.ipcMode = r.ipcMode;
    m_ctx.etch = r.etch;
    m_ctx.disableViaHeight = r.disableViaHeight;
    m_ctx.throughAspectLimit = r.throughAspectLimit;
    m_ctx.microviaAspectLimit = r.microviaAspectLimit;
    m_ctx.useMicrons = r.useMicrons;
    m_ctx.manualCopper = r.manualCopper;
    applyContextToUi();
    readUi();
    emit contextChanged();
}

void PcbOptions::loadSettings()
{
    QSettings s;
    PcbContext c;
    c.imperial = s.value("pcb/imperial", false).toBool();
    c.useMicrons = s.value("pcb/microns", false).toBool();
    c.manualCopper = s.value("pcb/manualCopper", false).toBool();
    c.baseCuUm = s.value("pcb/baseCu", c.baseCuUm).toDouble();
    c.platingUm = s.value("pcb/plating", c.platingUm).toDouble();
    c.planeUm = s.value("pcb/plane", c.planeUm).toDouble();
    c.externalLayer = s.value("pcb/external", true).toBool();
    c.materialIndex = s.value("pcb/material", 0).toInt();
    c.er = s.value("pcb/er", c.er).toDouble();
    c.tg = s.value("pcb/tg", c.tg).toDouble();
    c.tempRise = s.value("pcb/rise", c.tempRise).toDouble();
    c.ambient = s.value("pcb/ambient", c.ambient).toDouble();
    c.ipcMode = static_cast<pcbcore::IpcMode>(std::clamp(s.value("pcb/ipc", 2).toInt(), 0, 2));
    c.etch = static_cast<pcbcore::EtchFactor>(std::clamp(s.value("pcb/etch", 2).toInt(), 0, 2));
    c.disableViaHeight = s.value("pcb/viaHeightOff", false).toBool();
    c.throughAspectLimit = s.value("pcb/aspectThrough", c.throughAspectLimit).toDouble();
    c.microviaAspectLimit = s.value("pcb/aspectMicro", c.microviaAspectLimit).toDouble();
    if (!(c.er >= 1.0)) c.er = 4.6;
    if (!(c.tg > 0)) c.tg = 130.0;
    m_ctx = c;
}

void PcbOptions::saveSettings() const
{
    QSettings s;
    s.setValue("pcb/imperial", m_ctx.imperial);
    s.setValue("pcb/microns", m_ctx.useMicrons);
    s.setValue("pcb/manualCopper", m_ctx.manualCopper);
    s.setValue("pcb/baseCu", m_ctx.baseCuUm);
    s.setValue("pcb/plating", m_ctx.platingUm);
    s.setValue("pcb/plane", m_ctx.planeUm);
    s.setValue("pcb/external", m_ctx.externalLayer);
    s.setValue("pcb/material", m_ctx.materialIndex);
    s.setValue("pcb/er", m_ctx.er);
    s.setValue("pcb/tg", m_ctx.tg);
    s.setValue("pcb/rise", m_ctx.tempRise);
    s.setValue("pcb/ambient", m_ctx.ambient);
    s.setValue("pcb/ipc", int(m_ctx.ipcMode));
    s.setValue("pcb/etch", int(m_ctx.etch));
    s.setValue("pcb/viaHeightOff", m_ctx.disableViaHeight);
    s.setValue("pcb/aspectThrough", m_ctx.throughAspectLimit);
    s.setValue("pcb/aspectMicro", m_ctx.microviaAspectLimit);
}

// ================================================================================================
//  Окно «Параметры»
// ================================================================================================
PcbSettingsDialog::PcbSettingsDialog(const PcbContext &ctx, QWidget *parent)
    : QDialog(parent), m_ctx(ctx)
{
    setWindowTitle(QStringLiteral("Параметры раздела «Печатные платы»"));
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);

    m_ipc = new QComboBox(this);
    m_ipc->addItems({ QStringLiteral("IPC-2221"), QStringLiteral("IPC-2152 без поправок (рекомендуется)"),
                      QStringLiteral("IPC-2152 с поправками") });
    m_ipc->setCurrentIndex(int(ctx.ipcMode));
    form->addRow(QStringLiteral("Расчёт тока проводника:"), m_ipc);

    m_manual = new QCheckBox(QStringLiteral("Вводить толщину меди числом (базовая медь и покрытие)"), this);
    m_manual->setChecked(ctx.manualCopper);
    form->addRow(QString(), m_manual);

    m_etch = new QComboBox(this);
    m_etch->addItems({ QStringLiteral("Нет (прямоугольное сечение)"), QStringLiteral("1:1"), QStringLiteral("2:1") });
    m_etch->setCurrentIndex(int(ctx.etch));
    form->addRow(QStringLiteral("Подтрав при травлении:"), m_etch);

    m_viaHeight = new QCheckBox(QStringLiteral("Не учитывать высоту переходного отверстия при расчёте тока"), this);
    m_viaHeight->setChecked(ctx.disableViaHeight);
    form->addRow(QString(), m_viaHeight);

    m_aspectThrough = new QDoubleSpinBox(this);
    m_aspectThrough->setRange(1.0, 30.0);
    m_aspectThrough->setDecimals(1);
    m_aspectThrough->setSuffix(QStringLiteral(" : 1"));
    m_aspectThrough->setValue(ctx.throughAspectLimit);
    form->addRow(QStringLiteral("Предел аспекта, сквозное:"), m_aspectThrough);

    m_aspectMicro = new QDoubleSpinBox(this);
    m_aspectMicro->setRange(0.2, 5.0);
    m_aspectMicro->setDecimals(1);
    m_aspectMicro->setSuffix(QStringLiteral(" : 1"));
    m_aspectMicro->setValue(ctx.microviaAspectLimit);
    form->addRow(QStringLiteral("Предел аспекта, микропереходное:"), m_aspectMicro);

    m_microns = new QCheckBox(QStringLiteral("В метрическом режиме показывать микроны вместо миллиметров"), this);
    m_microns->setChecked(ctx.useMicrons);
    form->addRow(QString(), m_microns);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *ok = buttons->addButton(QStringLiteral("OK"), QDialogButtonBox::AcceptRole);
    QPushButton *cancel = buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(buttons);
}

PcbContext PcbSettingsDialog::result() const
{
    PcbContext c = m_ctx;
    c.ipcMode = static_cast<pcbcore::IpcMode>(m_ipc->currentIndex());
    c.manualCopper = m_manual->isChecked();
    c.etch = static_cast<pcbcore::EtchFactor>(m_etch->currentIndex());
    c.disableViaHeight = m_viaHeight->isChecked();
    c.throughAspectLimit = m_aspectThrough->value();
    c.microviaAspectLimit = m_aspectMicro->value();
    c.useMicrons = m_microns->isChecked();
    return c;
}
