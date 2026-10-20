// Страницы тока и конструкции проводников: свойства проводника, ток плавления, переходные отверстия, зазоры.
#include <QCheckBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "pcb/PcbDialogs.h"
#include "pcb/PcbPagesInternal.h"

namespace {

QString modeName(pcbcore::IpcMode m)
{
    switch (m) {
    case pcbcore::IpcMode::Ipc2221: return QStringLiteral("IPC-2221");
    case pcbcore::IpcMode::Ipc2152: return QStringLiteral("IPC-2152 без поправок");
    case pcbcore::IpcMode::Ipc2152Modifiers: return QStringLiteral("IPC-2152 с поправками");
    }
    return QString();
}

// ================================================================================================
//  Свойства проводника
// ================================================================================================
class PropertiesPage : public PcbPage
{
public:
    explicit PropertiesPage(QWidget *parent)
        : PcbPage(QStringLiteral("Свойства проводника"), QStringLiteral("Характеристики проводника"), parent)
    {
        QGridLayout *g = pcbGrid(frame());

        m_solveFor = new PcbChoice(QStringLiteral("Решить для"), frame());
        m_solveFor->setOptions({ QStringLiteral("Ток"), QStringLiteral("Ширина проводника") });
        auto *help = pcbButton(QStringLiteral("?  Справка"), frame());
        connect(help, &QPushButton::clicked, this, [this] { PcbDialogs::conductorInfo(this); });
        m_plane = new PcbChoice(QStringLiteral("Плоскость рядом?"), frame());
        m_plane->setOptions({ QStringLiteral("Нет"), QStringLiteral("Да") });
        m_parallel = new PcbChoice(QStringLiteral("Параллельные проводники?"), frame());
        m_parallel->setOptions({ QStringLiteral("Нет"), QStringLiteral("Да") });

        auto *left = new QGridLayout;
        left->setHorizontalSpacing(10);
        left->addWidget(m_solveFor, 0, 0);
        left->addWidget(help, 0, 1, Qt::AlignTop);
        left->addWidget(m_plane, 1, 0);
        left->addWidget(m_parallel, 2, 0);
        g->addLayout(left, 0, 0);

        m_load = makeInput(5.0, QStringLiteral("А"));
        m_width = makeInput(1.27);
        m_length = makeInput(25.4);
        m_board = makeInput(1.5748);
        m_planeDist = makeInput(0.2);
        for (PcbField *f : { m_width, m_length, m_board, m_planeDist })
            f->setUnits(PcbUnits::lengthMm());
        m_freq = new PcbField(PcbField::Input, frame());
        m_freq->setUnit(QStringLiteral("МГц"));
        m_freq->setValue(1.0);
        connect(m_freq, &PcbField::edited, this, [this] { recalc(); });
        m_dc = new QCheckBox(QStringLiteral("DC"), frame());
        connect(m_dc, &QCheckBox::toggled, this, [this](bool) { recalc(); });
        m_count = new QSpinBox(frame());
        m_count->setRange(2, 20);
        m_count->setValue(2);
        connect(m_count, &QSpinBox::valueChanged, this, [this](int) { recalc(); });

        auto *right = new QGridLayout;
        right->setHorizontalSpacing(10);
        right->setVerticalSpacing(6);
        right->addWidget(pcbLabeled(QStringLiteral("Ток нагрузки"), m_load, frame()), 0, 0);
        right->addWidget(pcbLabeled(QStringLiteral("Ширина проводника"), m_width, frame()), 0, 1);
        right->addWidget(pcbLabeled(QStringLiteral("Длина проводника"), m_length, frame()), 1, 1);
        right->addWidget(pcbLabeled(QStringLiteral("Толщина платы"), m_board, frame()), 2, 1);
        auto *fbox = new QWidget(frame());
        auto *fl = new QVBoxLayout(fbox);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(3);
        auto *frow = new QHBoxLayout;
        auto *flab = new QLabel(QStringLiteral("Частота"), fbox);
        flab->setObjectName(QStringLiteral("pcbLabel"));
        frow->addWidget(flab, 1);
        frow->addWidget(m_dc);
        fl->addLayout(frow);
        fl->addWidget(m_freq);
        right->addWidget(fbox, 3, 1);
        m_planeDistBox = pcbLabeled(QStringLiteral("Расстояние до плоскости"), m_planeDist, frame());
        right->addWidget(m_planeDistBox, 1, 0);
        m_countBox = pcbLabeled(QStringLiteral("Число параллельных проводников"), m_count, frame());
        right->addWidget(m_countBox, 2, 0);
        g->addLayout(right, 0, 1);

        m_diagram = new PcbDiagram(PcbDiagram::Microstrip, frame());
        m_diagram->setMinimumHeight(140);
        g->addWidget(m_diagram, 1, 0);
        m_modeLabel = pcbNote(QString(), frame());
        g->addWidget(m_modeLabel, 2, 0);

        m_skin = makeOutput(QStringLiteral("мкм"), 5);
        m_skinPct = makeOutput(QStringLiteral("%"), 0);
        m_vLoaded = makeOutput(QStringLiteral("В"), 4);
        m_p = makeOutput(QStringLiteral("Вт"), 5);
        m_pDbm = makeOutput(QStringLiteral("дБм"), 4);
        m_v = makeOutput(QStringLiteral("В"), 4);
        m_r = makeOutput(QStringLiteral("Ом"), 5);
        m_area = makeOutput(QString(), 4);
        m_area->setUnits(PcbUnits::areaSmall());
        m_i = makeOutput(QStringLiteral("А"), 4);
        auto *outs = new QGridLayout;
        outs->setHorizontalSpacing(10);
        outs->setVerticalSpacing(6);
        const struct { const char *t; PcbField *f; } items[] = {
            { "Толщина скин-слоя", m_skin }, { "Рассеиваемая мощность", m_p }, { "Сопротивление постоянному току", m_r },
            { "Доля скин-слоя", m_skinPct }, { "Мощность, дБм", m_pDbm }, { "Сечение проводника", m_area },
            { "Падение при токе нагрузки", m_vLoaded }, { "Падение напряжения", m_v }, { "Ток проводника", m_i } };
        for (int i = 0; i < 9; ++i)
            outs->addWidget(pcbLabeled(QString::fromUtf8(items[i].t), items[i].f, frame()), i / 3, i % 3);
        g->addLayout(outs, 3, 0, 1, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(4, 1);

        connect(m_solveFor, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
        connect(m_plane, &PcbChoice::currentChanged, this, [this](int) { updateVisibility(); recalc(); });
        connect(m_parallel, &PcbChoice::currentChanged, this, [this](int) { updateVisibility(); recalc(); });
        updateVisibility();
    }

    PcbPageConfig config() const override
    {
        PcbPageConfig c;
        c.base = c.plating = c.planeThickness = c.conductorLayer = c.tg = c.tempRise = c.ambient = c.units = true;
        return c;
    }
    bool planeThicknessEnabled() const override { return m_plane->currentIndex() == 1; }

    bool usesTrace() const override { return true; }
    void setTrace(const PcbTrace &t) override
    {
        if (t.valid) {
            m_length->setValue(t.lengthMm);
            if (t.widthMm > 0 && m_solveFor->currentIndex() == 0)
                m_width->setValue(t.widthMm);
        }
        recalc();
    }
    QString traceSummary() const override
    {
        if (!m_res.valid)
            return QString();
        return QStringLiteral("Ток при нагреве на %1 °C: %2 А (W = %3 мм, L = %4 мм), R = %5 мОм; нагрузка %6 А — %7 °C")
            .arg(PcbFmt::trimmed(context().tempRise), PcbFmt::fixed(m_res.maxCurrentA, 2), PcbFmt::fixed(m_res.widthMm, 3),
                 PcbFmt::fixed(m_length->value(), 1), PcbFmt::fixed(m_res.rDc * 1000.0, 2), PcbFmt::trimmed(m_load->value()),
                 PcbFmt::fixed(m_res.loadedTempC, 0));
    }

    QStringList infoItems() const override
    {
        QStringList l;
        l << PcbFmt::info(QStringLiteral("Материал Tg"), PcbFmt::fixed(context().tg, 1) + QStringLiteral(" °C"), m_res.overTg);
        l << PcbFmt::info(QStringLiteral("Температура при токе нагрузки"), PcbFmt::fixed(m_res.loadedTempC, 1) + QStringLiteral(" °C"),
                          m_res.overTg && m_res.loadedTempC > context().tg);
        l << PcbFmt::info(QStringLiteral("Температура проводника"), PcbFmt::fixed(m_res.tempC, 1) + QStringLiteral(" °C"),
                          m_res.overTg && m_res.tempC > context().tg);
        l << PcbFmt::info(QStringLiteral("Плотность тока J"), PcbFmt::fixed(m_res.currentDensity, 2) + QStringLiteral(" А/мм²"));
        if (m_res.overTg)
            l << PcbFmt::info(QStringLiteral("Внимание"), QStringLiteral("превышена температура стеклования материала"), true);
        return l;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_width, m_length, m_board, m_planeDist })
            f->setUnitMode(context().unitMode());
        m_area->setUnitMode(context().imperial ? 1 : 0);
        const bool modifiers = context().ipcMode == pcbcore::IpcMode::Ipc2152Modifiers;
        m_solveFor->setEnabled(!modifiers);
        if (modifiers)
            m_solveFor->setCurrentIndex(0);
        m_modeLabel->setText(QStringLiteral("%1 · травление %2").arg(modeName(context().ipcMode),
                                 context().etch == pcbcore::EtchFactor::None ? QStringLiteral("нет")
                                 : context().etch == pcbcore::EtchFactor::OneToOne ? QStringLiteral("1:1") : QStringLiteral("2:1")));
    }

    void solve() override
    {
        const PcbContext &c = context();
        pcbcore::ConductorInputs in;
        in.mode = c.ipcMode;
        in.external = c.externalLayer;
        in.etch = c.etch;
        in.widthMm = m_width->value();
        in.lengthMm = m_length->hasValue() ? m_length->value() : 0.0;
        in.thicknessMm = c.totalCuMm();
        in.freqHz = (m_dc->isChecked() || !m_freq->hasValue()) ? 0.0 : m_freq->value() * 1e6;
        in.loadA = m_load->hasValue() ? m_load->value() : 0.0;
        in.tempRise = c.tempRise;
        in.ambient = c.ambient;
        in.tg = c.tg;
        in.modifiers.boardMm = m_board->hasValue() ? m_board->value() : 1.5748;
        in.modifiers.plane = m_plane->currentIndex() == 1;
        in.modifiers.planeUm = c.planeUm;
        in.modifiers.planeDistMm = m_planeDist->hasValue() ? m_planeDist->value() : 0.2;
        in.modifiers.parallelCount = m_parallel->currentIndex() == 1 ? m_count->value() : 1;
        in.solveWidth = m_solveFor->currentIndex() == 1 && c.ipcMode != pcbcore::IpcMode::Ipc2152Modifiers;

        m_res = pcbcore::solveConductor(in);
        m_width->setEnabled(!in.solveWidth);
        m_load->setEnabled(true);
        const double nan = PcbFmt::kNaN;
        if (in.solveWidth && m_res.valid)
            m_width->setValue(m_res.widthMm);
        m_skin->setValue(m_res.valid ? m_res.skinDepthUm : nan);
        m_skinPct->setValue(m_res.valid ? m_res.skinPercent : nan);
        m_vLoaded->setValue(m_res.valid ? m_res.vDropLoaded : nan);
        m_p->setValue(m_res.valid ? m_res.powerW : nan);
        m_pDbm->setValue(m_res.valid ? m_res.powerDbm : nan);
        m_v->setValue(m_res.valid ? m_res.vDrop : nan);
        m_r->setValue(m_res.valid ? m_res.rDc : nan);
        m_area->setValue(m_res.valid ? m_res.crossMm2 : nan);
        m_i->setValue(m_res.valid ? m_res.maxCurrentA : nan);
        m_diagram->setKind(c.externalLayer ? PcbDiagram::Microstrip : PcbDiagram::Stripline);
    }

private:
    void updateVisibility()
    {
        m_planeDistBox->setVisible(m_plane->currentIndex() == 1);
        m_countBox->setVisible(m_parallel->currentIndex() == 1);
    }

    PcbChoice *m_solveFor, *m_plane, *m_parallel;
    PcbField *m_load, *m_width, *m_length, *m_board, *m_planeDist, *m_freq;
    PcbField *m_skin, *m_skinPct, *m_vLoaded, *m_p, *m_pDbm, *m_v, *m_r, *m_area, *m_i;
    QCheckBox *m_dc;
    QSpinBox *m_count;
    QWidget *m_planeDistBox, *m_countBox;
    PcbDiagram *m_diagram;
    QLabel *m_modeLabel;
    pcbcore::ConductorResult m_res;
};

// ================================================================================================
//  Ток плавления
// ================================================================================================
class FusingPage : public PcbPage
{
public:
    explicit FusingPage(QWidget *parent)
        : PcbPage(QStringLiteral("Ток плавления"), QStringLiteral("Ток плавления проводника"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        g->addWidget(pcbNote(QStringLiteral("Расчёт по уравнению Ондердонка для голого медного проводника. Формула не проверена для плат "
                                            "и применима для коротких импульсов (до ≈5 с)."), frame()), 0, 0, 1, 2);
        m_w = makeInput(0.254);
        m_w->setUnits(PcbUnits::lengthMm());
        m_t = makeInput(1.0, QStringLiteral("с"));
        m_mult = makeInput(1.0);
        g->addWidget(pcbLabeled(QStringLiteral("Ширина проводника"), m_w, frame()), 1, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Время"), m_t, frame()), 2, 1);
        auto *d = new PcbDiagram(PcbDiagram::Fusing, frame());
        d->setMinimumHeight(170);
        g->addWidget(d, 1, 0, 4, 1);
        auto *btn = pcbButton(QStringLiteral("Множитель Ондердонка"), frame());
        connect(btn, &QPushButton::clicked, this, [this] { PcbDialogs::onderdonkInfo(this); });
        g->addWidget(btn, 3, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Множитель"), m_mult, frame()), 4, 1);

        m_text = new QLabel(frame());
        m_text->setTextFormat(Qt::RichText);
        m_text->setWordWrap(true);
        g->addWidget(m_text, 5, 0);
        m_area = makeOutput(QString(), 4);
        m_area->setUnits(PcbUnits::areaSmall());
        m_i = makeOutput(QStringLiteral("А"), 4);
        auto *outs = new QVBoxLayout;
        outs->addWidget(pcbLabeled(QStringLiteral("Сечение проводника"), m_area, frame()));
        outs->addWidget(pcbLabeled(QStringLiteral("Ток плавления"), m_i, frame()));
        g->addLayout(outs, 5, 1);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(6, 1);
    }

    PcbPageConfig config() const override
    {
        PcbPageConfig c;
        c.base = c.plating = c.ambient = c.units = true;
        return c;
    }

    bool usesTrace() const override { return true; }
    void setTrace(const PcbTrace &t) override
    {
        if (t.valid && t.widthMm > 0)
            m_w->setValue(t.widthMm);
        recalc();
    }
    QString traceSummary() const override
    {
        if (!m_i->hasValue() && !std::isfinite(m_lastCurrent))
            return QString();
        return QStringLiteral("Ток плавления: %1 А (W = %2 мм, t = %3 с)")
            .arg(PcbFmt::fixed(m_lastCurrent, 2), PcbFmt::fixed(m_w->value(), 3), PcbFmt::trimmed(m_t->value()));
    }

    void contextChanged() override
    {
        m_w->setUnitMode(context().unitMode());
        m_area->setUnitMode(context().imperial ? 1 : 0);
    }

    void solve() override
    {
        const auto r = pcbcore::fusingCurrent(m_w->value(), context().totalCuMm(), context().etch, m_t->value(), context().ambient,
                                              m_mult->hasValue() ? m_mult->value() : 1.0);
        m_lastCurrent = r.valid ? r.currentA : PcbFmt::kNaN;
        m_area->setValue(r.valid ? r.crossMm2 : PcbFmt::kNaN);
        m_i->setValue(r.valid ? r.currentA : PcbFmt::kNaN);
        m_text->setText(QStringLiteral("Температура плавления меди: <b>%1 °C</b><br>Сечение в круговых милах: <b>%2</b><br>"
                                       "<i>I = A·√( lg(1 + (T<sub>пл</sub> − T<sub>окр</sub>)/(234 + T<sub>окр</sub>)) / (33·t) )</i>")
                            .arg(PcbFmt::fixed(pcbcore::kCopperMeltC, 2), r.valid ? PcbFmt::fixed(r.crossCmil, 2) : QStringLiteral("—")));
    }

private:
    PcbField *m_w, *m_t, *m_mult, *m_area, *m_i;
    QLabel *m_text;
    double m_lastCurrent = PcbFmt::kNaN;
};

// ================================================================================================
//  Переходные отверстия
// ================================================================================================
class ViaPage : public PcbPage
{
public:
    explicit ViaPage(QWidget *parent)
        : PcbPage(QStringLiteral("Переходные отверстия"), QStringLiteral("Характеристики переходного отверстия"), parent)
    {
        auto *outer = new QVBoxLayout(frame());
        outer->setContentsMargins(12, 20, 12, 12);
        m_stack = new QStackedWidget(frame());
        outer->addWidget(m_stack, 1);

        // --- страница 0: свойства ----------------------------------------------------------------
        auto *p0 = new QWidget(m_stack);
        auto *g = new QGridLayout(p0);
        g->setContentsMargins(0, 0, 0, 0);
        g->setHorizontalSpacing(12);
        g->setVerticalSpacing(8);
        m_hole = makeInput(0.254);
        m_pad = makeInput(0.508);
        m_anti = makeInput(1.016);
        m_height = makeInput(1.575);
        m_plating = makeInput(0.0254);
        for (PcbField *f : { m_hole, m_pad, m_anti, m_height, m_plating })
            f->setUnits(PcbUnits::lengthMm());
        auto *d = new PcbDiagram(PcbDiagram::Via, p0);
        d->setMinimumHeight(200);
        g->addWidget(d, 0, 0, 5, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Диаметр отверстия"), m_hole, p0), 0, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Диаметр внутренней площадки"), m_pad, p0), 1, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Диаметр окна в плоскости"), m_anti, p0), 2, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Высота переходного"), m_height, p0), 3, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Толщина металлизации"), m_plating, p0), 4, 1);

        m_cap = makeOutput(QStringLiteral("пФ"), 4);
        m_res = makeOutput(QStringLiteral("Ом"), 5);
        m_pow = makeOutput(QStringLiteral("Вт"), 5);
        m_ind = makeOutput(QStringLiteral("нГн"), 4);
        m_fres = makeOutput(QStringLiteral("МГц"), 3);
        m_area = makeOutput(QString(), 4);
        m_area->setUnits(PcbUnits::areaSmall());
        m_z = makeOutput(QStringLiteral("Ом"), 3);
        m_step = makeOutput(QStringLiteral("пс"), 4);
        m_cur = makeOutput(QStringLiteral("А"), 4);
        const struct { const char *t; PcbField *f; } items[] = {
            { "Ёмкость", m_cap }, { "Сопротивление постоянному току", m_res }, { "Рассеиваемая мощность", m_pow },
            { "Индуктивность", m_ind }, { "Резонансная частота", m_fres }, { "Сечение проводника", m_area },
            { "Импеданс переходного", m_z }, { "Отклик на ступеньку (T10–90)", m_step }, { "Ток переходного", m_cur } };
        auto *outs = new QGridLayout;
        outs->setHorizontalSpacing(10);
        outs->setVerticalSpacing(6);
        for (int i = 0; i < 9; ++i)
            outs->addWidget(pcbLabeled(QString::fromUtf8(items[i].t), items[i].f, p0), i / 3, i % 3);
        g->addLayout(outs, 5, 0, 1, 2);
        m_count = new QSpinBox(p0);
        m_count->setRange(1, 999);
        m_count->setValue(10);
        connect(m_count, &QSpinBox::valueChanged, this, [this](int) { recalc(); });
        g->addWidget(pcbLabeled(QStringLiteral("Число тепловых переходных"), m_count, p0), 6, 0);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(7, 1);
        m_stack->addWidget(p0);

        // --- страница 1: дифференциальные переходные и «пенёк» ---------------------------------------
        auto *p1 = new QWidget(m_stack);
        auto *s = new QGridLayout(p1);
        s->setContentsMargins(0, 0, 0, 0);
        s->setHorizontalSpacing(12);
        s->setVerticalSpacing(8);
        m_drill = makeInput(0.3);
        m_openH = makeInput(0.9);
        m_openW = makeInput(0.9);
        m_spacing = makeInput(1.0);
        for (PcbField *f : { m_drill, m_openH, m_openW, m_spacing })
            f->setUnits(PcbUnits::lengthMm());
        m_aniso = makeInput(1.0);
        m_baud = makeInput(10.0, QStringLiteral("Гбод"));
        m_full = new QCheckBox(QStringLiteral("Общее окно в плоскости для обоих отверстий"), p1);
        connect(m_full, &QCheckBox::toggled, this, [this](bool) { recalc(); });
        s->addWidget(pcbLabeled(QStringLiteral("Диаметр сверла"), m_drill, p1), 0, 0);
        s->addWidget(pcbLabeled(QStringLiteral("Окно в плоскости, высота (H)"), m_openH, p1), 1, 0);
        s->addWidget(pcbLabeled(QStringLiteral("Окно в плоскости, ширина (W)"), m_openW, p1), 2, 0);
        s->addWidget(m_full, 3, 0, 1, 2);
        s->addWidget(pcbLabeled(QStringLiteral("Расстояние между отверстиями"), m_spacing, p1), 4, 0);
        s->addWidget(pcbLabeled(QStringLiteral("Анизотропия подложки"), m_aniso, p1), 0, 1);
        s->addWidget(pcbLabeled(QStringLiteral("Скорость передачи"), m_baud, p1), 1, 1);
        m_zd = makeOutput(QStringLiteral("Ом"), 3);
        m_zodd = makeOutput(QStringLiteral("Ом"), 3);
        m_zeven = makeOutput(QStringLiteral("Ом"), 3);
        m_zs = makeOutput(QStringLiteral("Ом"), 3);
        m_stub = makeOutput(QString(), 3);
        m_stub->setUnits(PcbUnits::lengthMm());
        s->addWidget(pcbLabeled(QStringLiteral("Zdiff пары переходных"), m_zd, p1), 5, 0);
        s->addWidget(pcbLabeled(QStringLiteral("Zodd"), m_zodd, p1), 5, 1);
        s->addWidget(pcbLabeled(QStringLiteral("Zeven"), m_zeven, p1), 6, 0);
        s->addWidget(pcbLabeled(QStringLiteral("Z одиночного переходного"), m_zs, p1), 6, 1);
        s->addWidget(pcbLabeled(QStringLiteral("Максимальная длина «пенька»"), m_stub, p1), 7, 0);
        s->addWidget(pcbNote(QStringLiteral("Оценочная модель: пара коаксиалов в окнах плоскости. «Пенёк» — часть отверстия за "
                                            "последним слоем, который использует сигнал; он не должен превышать λ/20 на частоте сигнала. "
                                            "Используйте диаметр сверла, а не готового отверстия."), p1), 8, 0, 1, 2);
        s->setRowStretch(9, 1);
        s->setColumnStretch(0, 1);
        s->setColumnStretch(1, 1);
        m_stack->addWidget(p1);

        // --- блок параметров справа ------------------------------------------------------------------
        m_options = new QWidget(this);
        auto *ol = new QVBoxLayout(m_options);
        ol->setContentsMargins(0, 0, 0, 0);
        ol->setSpacing(8);
        m_mode = new PcbChoice(QStringLiteral("Выбор свойств"), m_options);
        m_mode->setOptions({ QStringLiteral("Свойства переходного"), QStringLiteral("Длина «пенька»") });
        m_layers = new PcbChoice(QStringLiteral("Набор слоёв"), m_options);
        m_layers->setOptions({ QStringLiteral("2 слоя"), QStringLiteral("Многослойная плата"), QStringLiteral("Микропереходное") });
        m_layers->setCurrentIndex(1);
        ol->addWidget(m_mode);
        ol->addWidget(m_layers);
        m_options->hide();
        connect(m_mode, &PcbChoice::currentChanged, this, [this](int i) { m_stack->setCurrentIndex(i); recalc(); });
        connect(m_layers, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
    }

    PcbPageConfig config() const override
    {
        PcbPageConfig c;
        c.er = c.tempRise = c.ambient = c.units = true;
        c.tg = true;
        return c;
    }
    QWidget *optionsWidget() override { return m_options; }

    QStringList infoItems() const override
    {
        QStringList l;
        if (m_mode->currentIndex() == 1) {
            if (std::isfinite(m_targetZ))
                l << PcbFmt::info(QStringLiteral("Целевое Zdiff"), PcbFmt::fixed(m_targetZ, 1) + QStringLiteral(" Ом"));
            return l;
        }
        const int layers = m_layers->currentIndex();
        const double limit = layers == 2 ? context().microviaAspectLimit : context().throughAspectLimit;
        l << PcbFmt::info(QStringLiteral("Мощность, дБм"), PcbFmt::fixed(m_via.powerDbm, 4));
        l << PcbFmt::info(QStringLiteral("Тепловое сопротивление"), PcbFmt::fixed(m_via.rthCW, 1) + QStringLiteral(" °C/Вт"));
        l << PcbFmt::info(QStringLiteral("Аспект"), PcbFmt::fixed(m_via.aspect, 2) + QStringLiteral(":1"), m_via.aspect > limit);
        l << PcbFmt::info(QStringLiteral("Тепловых переходных: %1").arg(m_count->value()),
                          PcbFmt::fixed(m_via.rthPerViaCW, 1) + QStringLiteral(" °C/Вт на группу"));
        l << PcbFmt::info(QStringLiteral("Плотность тока J"), PcbFmt::fixed(m_via.density, 2) + QStringLiteral(" А/мм²"));
        l << PcbFmt::info(QStringLiteral("Падение напряжения"), PcbFmt::fixed(m_via.vDrop * 1000.0, 4) + QStringLiteral(" мВ"));
        l << PcbFmt::info(QStringLiteral("Температура переходного"), PcbFmt::fixed(m_via.tempC, 1) + QStringLiteral(" °C"));
        if (m_via.aspect > limit)
            l << PcbFmt::info(QStringLiteral("Внимание"), QStringLiteral("аспект выше допустимого"), true);
        return l;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_hole, m_pad, m_anti, m_height, m_plating, m_drill, m_openH, m_openW, m_spacing, m_stub })
            f->setUnitMode(context().unitMode());
        m_area->setUnitMode(context().imperial ? 1 : 0);
    }

    void receiveValue(const QString &key, double value) override
    {
        if (key == QLatin1String("targetZdiff"))
            m_targetZ = value;
    }

    void solve() override
    {
        const double nan = PcbFmt::kNaN;
        if (m_mode->currentIndex() == 1) {
            pcbcore::ViaStubInputs in;
            in.drillMm = m_drill->value();
            in.platingMm = m_plating->hasValue() ? m_plating->value() : 0.025;
            in.openingHMm = m_openH->value();
            in.openingWMm = m_openW->value();
            in.fullOpening = m_full->isChecked();
            in.spacingMm = m_spacing->value();
            in.anisotropy = m_aniso->hasValue() ? m_aniso->value() : 1.0;
            in.er = context().er;
            in.baudRate = m_baud->hasValue() ? m_baud->value() * 1e9 : 0.0;
            const auto r = pcbcore::solveViaStub(in);
            m_zd->setValue(r.valid ? r.zdiff : nan);
            m_zodd->setValue(r.valid ? r.zodd : nan);
            m_zeven->setValue(r.valid ? r.zeven : nan);
            m_zs->setValue(r.valid ? r.zSingle : nan);
            m_stub->setValue(r.valid ? r.maxStubMm : nan);
            return;
        }
        pcbcore::ViaInputs in;
        in.layers = static_cast<pcbcore::ViaLayerSet>(m_layers->currentIndex());
        in.holeMm = m_hole->value();
        in.padMm = m_pad->value();
        in.antipadMm = m_anti->value();
        in.heightMm = m_height->value();
        in.platingMm = m_plating->value();
        in.er = context().er;
        in.tempRise = context().tempRise;
        in.ambient = context().ambient;
        in.count = m_count->value();
        in.mode = context().ipcMode;
        in.disableHeightEffect = context().disableViaHeight;
        m_via = pcbcore::solveVia(in);
        const bool ok = m_via.valid, ac = ok && m_via.hasAc;
        m_cap->setValue(ac ? m_via.capPf : nan);
        m_ind->setValue(ok ? m_via.indNh : nan);
        m_z->setValue(ac ? m_via.zOhm : nan);
        m_fres->setValue(ac ? m_via.fresHz / 1e6 : nan);
        m_step->setValue(ac ? m_via.stepPs : nan);
        m_res->setValue(ok ? m_via.rDc : nan);
        m_area->setValue(ok ? m_via.crossMm2 : nan);
        m_pow->setValue(ok ? m_via.powerW : nan);
        m_cur->setValue(ok ? m_via.currentA : nan);
        if (ok && !ac)
            for (PcbField *f : { m_cap, m_z, m_fres, m_step })
                f->setText(QStringLiteral("Н/Д"));
    }

private:
    QStackedWidget *m_stack;
    QWidget *m_options;
    PcbChoice *m_mode, *m_layers;
    PcbField *m_hole, *m_pad, *m_anti, *m_height, *m_plating;
    PcbField *m_cap, *m_res, *m_pow, *m_ind, *m_fres, *m_area, *m_z, *m_step, *m_cur;
    PcbField *m_drill, *m_openH, *m_openW, *m_spacing, *m_aniso, *m_baud;
    PcbField *m_zd, *m_zodd, *m_zeven, *m_zs, *m_stub;
    QCheckBox *m_full;
    QSpinBox *m_count;
    pcbcore::ViaResult m_via;
    double m_targetZ = PcbFmt::kNaN;
};

// ================================================================================================
//  Минимальный зазор между проводниками
// ================================================================================================
class SpacingPage : public PcbPage
{
public:
    explicit SpacingPage(QWidget *parent)
        : PcbPage(QStringLiteral("Мин. зазор проводников"), QStringLiteral("Минимальный зазор между проводниками"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_volt = new PcbChoice(QStringLiteral("Напряжение между проводниками"), frame());
        for (const char *c : pcbcore::spacingVoltageClasses())
            m_volt->addOption(QString::fromUtf8(c));
        g->addWidget(m_volt, 0, 0, 2, 1);

        m_dev = new PcbChoice(QStringLiteral("Тип устройства"), frame());
        for (const auto &d : pcbcore::spacingDevices())
            m_dev->addOption(QStringLiteral("%1 — %2").arg(QString::fromUtf8(d.id), d.id[0] == 'B' ? QStringLiteral("плата") : QStringLiteral("сборка")));
        g->addWidget(m_dev, 0, 1);
        m_desc = new QLabel(frame());
        m_desc->setTextFormat(Qt::RichText);
        m_desc->setWordWrap(true);
        g->addWidget(m_desc, 1, 1);

        auto *table = pcbButton(QStringLiteral("Таблица значений IPC-2221"), frame());
        connect(table, &QPushButton::clicked, this, [this] { PcbDialogs::spacingTable(this); });
        g->addWidget(table, 2, 1, Qt::AlignRight);

        auto *d = new PcbDiagram(PcbDiagram::Spacing, frame());
        d->setMinimumHeight(130);
        g->addWidget(d, 2, 0);
        m_voltage = makeInput(501.0, QStringLiteral("В"));
        m_out = makeOutput(QString(), 3);
        m_out->setUnits(PcbUnits::lengthMm());
        g->addWidget(pcbLabeled(QStringLiteral("Напряжение между проводниками (для > 500 В)"), m_voltage, frame()), 3, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Минимальный зазор"), m_out, frame()), 4, 0);
        g->addWidget(pcbNote(QStringLiteral("Значения — по таблице 6-1 IPC-2221 (постоянное напряжение или амплитуда переменного). "
                                            "Справочные: для ответственных изделий сверяйтесь с действующей редакцией стандарта."), frame()), 5, 0, 1, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(6, 1);

        connect(m_volt, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
        connect(m_dev, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }

    void contextChanged() override { m_out->setUnitMode(context().unitMode()); }

    void solve() override
    {
        const int v = m_volt->currentIndex(), dev = m_dev->currentIndex();
        m_voltage->setEditable(v == 9);
        const double s = pcbcore::minConductorSpacingMm(v, dev, m_voltage->hasValue() ? m_voltage->value() : 500.0);
        m_out->setValue(s);
        QString legend;
        for (int i = 0; i < 8; ++i) {
            const auto &d = pcbcore::spacingDevices()[i];
            const QString line = QStringLiteral("%1 — %2").arg(QString::fromUtf8(d.id), QString::fromUtf8(d.text)).toHtmlEscaped();
            legend += i == dev ? QStringLiteral("<b>%1</b><br>").arg(line) : QStringLiteral("<span style=\"opacity:0.6\">%1</span><br>").arg(line);
        }
        m_desc->setText(legend);
    }

private:
    PcbChoice *m_volt, *m_dev;
    PcbField *m_voltage, *m_out;
    QLabel *m_desc;
};

} // namespace

PcbPage *makePropertiesPage(QWidget *parent) { return new PropertiesPage(parent); }
PcbPage *makeFusingPage(QWidget *parent) { return new FusingPage(parent); }
PcbPage *makeViaPage(QWidget *parent) { return new ViaPage(parent); }
PcbPage *makeSpacingPage(QWidget *parent) { return new SpacingPage(parent); }
