// Страницы конструкции платы: PDN, планарные катушки, падстек, тепловой режим, механика, встроенные резисторы.
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "pcb/PcbDialogs.h"
#include "pcb/PcbPagesInternal.h"

namespace {

// ================================================================================================
//  PDN
// ================================================================================================
class PdnPage : public PcbPage
{
public:
    explicit PdnPage(QWidget *parent)
        : PcbPage(QStringLiteral("PDN"), QStringLiteral("Информация о плоскости"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        QGridLayout *pdn;
        QGroupBox *pdnGroup = pcbSubGroup(QStringLiteral("Импеданс цепи питания (PDN)"), frame(), &pdn);
        m_v = makeInput(5.0, QStringLiteral("В"));
        m_ripple = makeInput(5.0, QStringLiteral("%"));
        m_i = makeInput(2.0, QStringLiteral("А"));
        m_trans = makeInput(50.0, QStringLiteral("%"));
        m_target = makeOutput(QStringLiteral("Ом"), 4);
        pdn->addWidget(pcbLabeled(QStringLiteral("Напряжение шины питания"), m_v, pdnGroup), 0, 0);
        pdn->addWidget(pcbLabeled(QStringLiteral("Допустимые пульсации (AC)"), m_ripple, pdnGroup), 0, 1);
        pdn->addWidget(pcbLabeled(QStringLiteral("Максимальный ток"), m_i, pdnGroup), 1, 0);
        pdn->addWidget(pcbLabeled(QStringLiteral("Доля переходной составляющей"), m_trans, pdnGroup), 1, 1);
        pdn->addWidget(pcbLabeled(QStringLiteral("Целевой импеданс PDN"), m_target, pdnGroup), 2, 0, 1, 2);
        auto *d = new PcbDiagram(PcbDiagram::Pdn, pdnGroup);
        d->setMinimumHeight(150);
        pdn->addWidget(d, 0, 2, 3, 1);
        pdn->setColumnStretch(2, 1);
        g->addWidget(pdnGroup, 0, 0);

        QGridLayout *pl;
        QGroupBox *planeGroup = pcbSubGroup(QStringLiteral("Ёмкость плоскостей"), frame(), &pl);
        m_area = makeInput(3225.8065);
        m_area->setUnits(PcbUnits::areaMm2());
        m_dist = makeInput(0.0508);
        m_dist->setUnits(PcbUnits::lengthMm());
        m_f = new PcbField(PcbField::Input, planeGroup);
        m_f->setUnit(QStringLiteral("МГц"));
        m_f->setValue(1.0);
        connect(m_f, &PcbField::edited, this, [this] { recalc(); });
        m_dc = new QCheckBox(QStringLiteral("DC"), planeGroup);
        connect(m_dc, &QCheckBox::toggled, this, [this](bool) { recalc(); });
        m_xc = makeOutput(QStringLiteral("Ом"), 4);
        m_c = makeOutput(QStringLiteral("пФ"), 2);
        pl->addWidget(pcbLabeled(QStringLiteral("Площадь плоскости"), m_area, planeGroup), 0, 0);
        pl->addWidget(pcbLabeled(QStringLiteral("Расстояние между плоскостями"), m_dist, planeGroup), 1, 0);
        auto *fb = new QWidget(planeGroup);
        auto *fl = new QVBoxLayout(fb);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(3);
        auto *frow = new QHBoxLayout;
        auto *flab = new QLabel(QStringLiteral("Частота"), fb);
        flab->setObjectName(QStringLiteral("pcbLabel"));
        frow->addWidget(flab, 1);
        frow->addWidget(m_dc);
        fl->addLayout(frow);
        fl->addWidget(m_f);
        pl->addWidget(fb, 2, 0);
        pl->addWidget(pcbLabeled(QStringLiteral("Ёмкостное сопротивление"), m_xc, planeGroup), 1, 1);
        pl->addWidget(pcbLabeled(QStringLiteral("Полная ёмкость плоскостей"), m_c, planeGroup), 2, 1);
        pl->setColumnStretch(0, 1);
        pl->setColumnStretch(1, 1);
        g->addWidget(planeGroup, 1, 0);
        g->setRowStretch(2, 1);
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.er = c.units = true; return c; }

    void contextChanged() override
    {
        m_area->setUnitMode(context().imperial ? 1 : 0);
        m_dist->setUnitMode(context().unitMode());
    }

    void solve() override
    {
        const double nan = PcbFmt::kNaN;
        m_target->setValue(m_v->hasValue() && m_ripple->hasValue() && m_i->hasValue() && m_trans->hasValue()
                               ? pcbcore::targetPdnImpedance(m_v->value(), m_ripple->value(), m_i->value(), m_trans->value()) : nan);
        const double cpf = (m_area->hasValue() && m_dist->hasValue())
                               ? pcbcore::planeCapacitancePf(context().er, m_area->value(), m_dist->value()) : nan;
        m_c->setValue(cpf);
        if (m_dc->isChecked())
            m_xc->setText(QStringLiteral("∞"));
        else
            m_xc->setValue(m_f->hasValue() && std::isfinite(cpf) ? pcbcore::capacitiveReactance(m_f->value() * 1e6, cpf * 1e-12) : nan);
        m_f->setEnabled(!m_dc->isChecked());
    }

private:
    PcbField *m_v, *m_ripple, *m_i, *m_trans, *m_target, *m_area, *m_dist, *m_f, *m_xc, *m_c;
    QCheckBox *m_dc;
};

// ================================================================================================
//  Планарные катушки
// ================================================================================================
class PlanarPage : public PcbPage
{
public:
    explicit PlanarPage(QWidget *parent)
        : PcbPage(QStringLiteral("Планарные катушки"), QStringLiteral("Расчёт планарных катушек"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_turns = makeInput(5.0);
        m_space = makeInput(0.254);
        m_width = makeInput(0.254);
        m_dout = makeInput(8.89);
        for (PcbField *f : { m_space, m_width, m_dout })
            f->setUnits(PcbUnits::lengthUm());
        g->addWidget(pcbLabeled(QStringLiteral("Число витков (n)"), m_turns, frame()), 0, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Зазор между витками (s)"), m_space, frame()), 0, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Ширина проводника (w)"), m_width, frame()), 1, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Внешний диаметр (dout)"), m_dout, frame()), 1, 1);
        auto *formulas = pcbButton(QStringLiteral("Коэффициенты формулы"), frame());
        connect(formulas, &QPushButton::clicked, this, [this] {
            PcbDialogs::info(this, QStringLiteral("Планарные катушки"), QStringLiteral(R"(
<h3>Модифицированная формула Уилера</h3>
<p>L = K<sub>1</sub>·μ<sub>0</sub>·n²·d<sub>avg</sub> / (1 + K<sub>2</sub>·ρ), где d<sub>avg</sub> = (d<sub>out</sub> + d<sub>in</sub>)/2, ρ = (d<sub>out</sub> − d<sub>in</sub>)/(d<sub>out</sub> + d<sub>in</sub>) — коэффициент заполнения,
d<sub>in</sub> = d<sub>out</sub> − 2·(n·w + (n − 1)·s).</p>
<table border="1" cellpadding="4" cellspacing="0">
<tr><th>Форма</th><th>K<sub>1</sub></th><th>K<sub>2</sub></th></tr>
<tr><td>Квадрат</td><td>2,34</td><td>2,75</td></tr>
<tr><td>Шестиугольник</td><td>2,33</td><td>3,82</td></tr>
<tr><td>Восьмиугольник</td><td>2,25</td><td>3,55</td></tr>
<tr><td>Круг</td><td>2,23</td><td>3,45</td></tr>
</table>
<p>Формула даёт индуктивность на низких частотах (без ёмкостных и вихревых потерь), точность — порядка единиц процентов при ρ &gt; 0,1.</p>)"));
        });
        g->addWidget(formulas, 0, 2, Qt::AlignTop);

        auto *d = new PcbDiagram(PcbDiagram::Planar, frame());
        d->setMinimumSize(200, 200);
        g->addWidget(d, 2, 0, 3, 1);
        m_din = makeOutput(QString(), 3);
        m_din->setUnits(PcbUnits::lengthUm());
        m_fill = makeOutput(QString(), 4);
        m_l = makeOutput(QStringLiteral("нГн"), 4);
        g->addWidget(pcbLabeled(QStringLiteral("Внутренний диаметр"), m_din, frame()), 2, 1, 1, 2);
        g->addWidget(pcbLabeled(QStringLiteral("Коэффициент заполнения ρ"), m_fill, frame()), 3, 1, 1, 2);
        g->addWidget(pcbLabeled(QStringLiteral("Индуктивность"), m_l, frame()), 4, 1, 1, 2);
        g->addWidget(pcbFormula(QStringLiteral("L = K<sub>1</sub> μ<sub>0</sub> n² d<sub>avg</sub> / (1 + K<sub>2</sub> ρ)"), frame()), 5, 0, 1, 3);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(6, 1);

        m_shape = new PcbChoice(QStringLiteral("Форма катушки"), this);
        m_shape->setOptions({ QStringLiteral("Квадратная"), QStringLiteral("Шестиугольная"), QStringLiteral("Восьмиугольная"), QStringLiteral("Круглая") });
        m_shape->hide();
        connect(m_shape, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }
    QWidget *optionsWidget() override { return m_shape; }

    QStringList infoItems() const override
    {
        QStringList l;
        if (m_bad)
            l << PcbFmt::info(QStringLiteral("Внимание"), QStringLiteral("витки не помещаются: внутренний диаметр ≤ 0"), true);
        return l;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_space, m_width, m_dout, m_din })
            f->setUnitMode(context().unitMode());
    }

    void solve() override
    {
        const auto r = pcbcore::planarInductor(static_cast<pcbcore::PlanarShape>(m_shape->currentIndex()),
                                               m_turns->value(), m_width->value(), m_space->value(), m_dout->value());
        m_bad = !r.valid && m_turns->hasValue() && m_width->hasValue() && m_space->hasValue() && m_dout->hasValue();
        const double nan = PcbFmt::kNaN;
        m_din->setValue(r.valid ? r.dinMm : nan);
        m_fill->setValue(r.valid ? r.fill : nan);
        m_l->setValue(r.valid ? r.inductanceH * 1e9 : nan);
    }

private:
    PcbChoice *m_shape;
    PcbField *m_turns, *m_space, *m_width, *m_dout, *m_din, *m_fill, *m_l;
    bool m_bad = false;
};

// ================================================================================================
//  Падстек
// ================================================================================================
class PadstackPage : public PcbPage
{
public:
    explicit PadstackPage(QWidget *parent)
        : PcbPage(QStringLiteral("Падстек"), QStringLiteral("Калькулятор падстека"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_type = new PcbChoice(QStringLiteral("Тип расчёта"), frame());
        m_type->setOptions({ QStringLiteral("Сквозная площадка"), QStringLiteral("Площадка BGA"), QStringLiteral("Проводник / площадка (сквозные)"),
                             QStringLiteral("Проводник / площадка (BGA)"), QStringLiteral("2 проводника / площадка (сквозные)"),
                             QStringLiteral("2 проводника / площадка (BGA)"), QStringLiteral("Угол–угол") });
        g->addWidget(m_type, 0, 0, Qt::AlignTop);
        m_stack = new QStackedWidget(frame());
        g->addWidget(m_stack, 0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(1, 1);
        buildPages();
        connect(m_type, &PcbChoice::currentChanged, this, [this](int i) { m_stack->setCurrentIndex(i); recalc(); });
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }

    QStringList infoItems() const override { return m_info; }

    void contextChanged() override
    {
        for (PcbField *f : m_lenFields)
            f->setUnitMode(context().unitMode());
    }

    void solve() override
    {
        const double nan = PcbFmt::kNaN;
        m_info.clear();
        switch (m_type->currentIndex()) {
        case 0: {
            if (!(m_hole->hasValue() && m_ring->hasValue() && m_iso->hasValue())) {
                for (PcbField *f : { m_ext, m_intSig, m_plane, m_inner, m_spoke }) f->setValue(nan);
                break;
            }
            const auto p = pcbcore::thruHolePad(m_hole->value(), m_ring->value(), m_iso->value(), m_style->currentIndex() == 0);
            m_ext->setValue(p.externalMm);
            m_intSig->setValue(p.internalSignalMm);
            m_plane->setValue(p.planeAntipadMm);
            m_inner->setValue(p.thermalInnerMm);
            m_spoke->setValue(p.spokeMm);
            m_info << PcbFmt::info(QStringLiteral("Диаметр площадки"), PcbFmt::fixed(p.externalMm, 4) + QStringLiteral(" мм"));
            break;
        }
        case 1: {
            const double ball = pcbcore::bgaBallDiametersMm()[std::clamp(m_ball->currentIndex(), 0, int(pcbcore::bgaBallDiametersMm().size()) - 1)];
            const auto l = pcbcore::bgaLand(ball);
            m_landNom->setValue(l.nominalMm);
            m_landMin->setValue(l.minMm);
            m_landMax->setValue(l.maxMm);
            break;
        }
        case 2:
        case 4: {
            const int n = m_type->currentIndex() == 2 ? 1 : 2;
            PcbField *pitch = n == 1 ? m_thPitch1 : m_thPitch2, *cw = n == 1 ? m_thW1 : m_thW2, *sp = n == 1 ? m_thS1 : m_thS2,
                     *hole = n == 1 ? m_thHole1 : m_thHole2, *ring = n == 1 ? m_thRing1 : m_thRing2,
                     *outPad = n == 1 ? m_thPad1 : m_thPad2, *outRing = n == 1 ? m_thAr1 : m_thAr2;
            if (!(pitch->hasValue() && cw->hasValue() && sp->hasValue() && hole->hasValue() && ring->hasValue())) {
                outPad->setValue(nan);
                outRing->setValue(nan);
                break;
            }
            const auto r = pcbcore::conductorBetweenThruPads(pitch->value(), cw->value(), sp->value(), hole->value(), ring->value(), n);
            outPad->setValue(r.maxPadMm);
            outRing->setValue(r.annularRingMm);
            if (!r.ringOk)
                m_info << PcbFmt::info(QStringLiteral("Внимание"), QStringLiteral("кольцо меньше минимального"), true);
            break;
        }
        case 3:
        case 5: {
            const int n = m_type->currentIndex() == 3 ? 1 : 2;
            PcbField *pitch = n == 1 ? m_bgaPitch1 : m_bgaPitch2, *land = n == 1 ? m_bgaLand1 : m_bgaLand2,
                     *sp = n == 1 ? m_bgaS1 : m_bgaS2, *out = n == 1 ? m_bgaW1 : m_bgaW2;
            if (!(pitch->hasValue() && land->hasValue() && sp->hasValue())) {
                out->setValue(nan);
                break;
            }
            const double w = pcbcore::maxWidthBetweenBgaLands(pitch->value(), land->value(), sp->value(), n);
            out->setValue(w);
            if (w <= 0)
                m_info << PcbFmt::info(QStringLiteral("Внимание"), QStringLiteral("проводник между площадками не помещается"), true);
            break;
        }
        case 6: {
            if (!(m_sideA->hasValue() && m_sideB->hasValue())) {
                for (PcbField *f : { m_diag, m_drillMin, m_drillMax }) f->setValue(nan);
                break;
            }
            const auto c = pcbcore::cornerToCorner(m_sideA->value(), m_sideB->value());
            m_diag->setValue(c.diagonalMm);
            m_drillMin->setValue(c.minDrillMm);
            m_drillMax->setValue(c.maxDrillMm);
            break;
        }
        }
    }

private:
    PcbField *lenIn(double mm, QWidget *parent)
    {
        auto *f = makeInput(mm);
        f->setParent(parent);
        f->setUnits(PcbUnits::lengthMm());
        m_lenFields << f;
        return f;
    }
    PcbField *lenOut(QWidget *parent, int decimals = 4)
    {
        auto *f = makeOutput(QString(), decimals);
        f->setParent(parent);
        f->setUnits(PcbUnits::lengthMm());
        m_lenFields << f;
        return f;
    }

    void buildPages()
    {
        // 0 — сквозная площадка
        {
            auto *w = new QWidget(m_stack);
            auto *g = new QGridLayout(w);
            g->setContentsMargins(0, 0, 0, 0);
            g->setHorizontalSpacing(12);
            g->setVerticalSpacing(8);
            m_holeType = new QComboBox(w);
            m_holeType->addItem(QStringLiteral("Своё значение"));
            for (double d : pcbcore::commonHoleDiametersMm())
                m_holeType->addItem(QStringLiteral("%1 мм").arg(PcbFmt::trimmed(d)));
            m_hole = lenIn(0.8128, w);
            m_ring = lenIn(0.3048, w);
            m_iso = lenIn(0.3048, w);
            m_style = new PcbChoice(QStringLiteral("Покрытие отверстия"), w);
            m_style->setOptions({ QStringLiteral("С металлизацией"), QStringLiteral("Без металлизации") });
            g->addWidget(pcbLabeled(QStringLiteral("Тип отверстия"), m_holeType, w), 0, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Диаметр отверстия"), m_hole, w), 1, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Минимальное кольцо"), m_ring, w), 2, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Ширина изоляции (зазор)"), m_iso, w), 3, 0);
            g->addWidget(m_style, 0, 1, 2, 1);
            auto *d = new PcbDiagram(PcbDiagram::Padstack, w);
            d->setMinimumSize(200, 190);
            g->addWidget(d, 2, 1, 3, 1);
            m_ext = lenOut(w);
            m_intSig = lenOut(w);
            m_plane = lenOut(w);
            m_inner = lenOut(w);
            m_spoke = lenOut(w);
            g->addWidget(pcbLabeled(QStringLiteral("Внешние слои, диаметр площадки"), m_ext, w), 5, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Внутренние сигнальные слои, диаметр"), m_intSig, w), 6, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Слои плоскостей, диаметр окна"), m_plane, w), 7, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Внутренний диаметр (тепловой барьер)"), m_inner, w), 5, 1);
            g->addWidget(pcbLabeled(QStringLiteral("Ширина перемычки"), m_spoke, w), 6, 1);
            g->setRowStretch(8, 1);
            m_stack->addWidget(w);
            connect(m_holeType, &QComboBox::currentIndexChanged, this, [this](int i) {
                if (i > 0) {
                    m_hole->setValue(pcbcore::commonHoleDiametersMm()[i - 1]);
                    recalc();
                }
            });
            connect(m_hole, &PcbField::edited, this, [this] { QSignalBlocker b(m_holeType); m_holeType->setCurrentIndex(0); });
            connect(m_style, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
        }
        // 1 — BGA
        {
            auto *w = new QWidget(m_stack);
            auto *g = new QGridLayout(w);
            g->setContentsMargins(0, 0, 0, 0);
            g->setVerticalSpacing(8);
            m_ball = new QComboBox(w);
            for (double d : pcbcore::bgaBallDiametersMm())
                m_ball->addItem(QStringLiteral("%1 мм").arg(PcbFmt::trimmed(d)));
            m_ball->setCurrentIndex(4);
            m_landNom = lenOut(w);
            m_landMin = lenOut(w);
            m_landMax = lenOut(w);
            g->addWidget(pcbLabeled(QStringLiteral("Номинальный диаметр шарика"), m_ball, w), 0, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Номинальный диаметр площадки"), m_landNom, w), 1, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Допуск: минимум"), m_landMin, w), 2, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Допуск: максимум"), m_landMax, w), 3, 0);
            g->addWidget(pcbNote(QStringLiteral("Площадка ≈ 80 % диаметра шарика (NSMD) с допуском ±10 %. IPC-7351 требует брать "
                                                "верхнюю границу допуска."), w), 4, 0);
            g->setRowStretch(5, 1);
            m_stack->addWidget(w);
            connect(m_ball, &QComboBox::currentIndexChanged, this, [this](int) { recalc(); });
        }
        // 2 и 4 — проводники между сквозными площадками
        for (int n = 1; n <= 2; ++n) {
            auto *w = new QWidget(m_stack);
            auto *g = new QGridLayout(w);
            g->setContentsMargins(0, 0, 0, 0);
            g->setVerticalSpacing(8);
            PcbField *pitch = lenIn(2.54, w), *cw = lenIn(0.254, w), *sp = lenIn(0.2032, w), *hole = lenIn(0.8128, w),
                     *ring = lenIn(0.1524, w), *pad = lenOut(w), *ar = lenOut(w);
            const QString cwLabel = n == 1 ? QStringLiteral("Ширина проводника") : QStringLiteral("Ширина каждого из двух проводников");
            g->addWidget(pcbLabeled(QStringLiteral("Расстояние между центрами площадок"), pitch, w), 0, 0);
            g->addWidget(pcbLabeled(cwLabel, cw, w), 1, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Зазор «площадка — проводник»"), sp, w), 2, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Диаметр отверстия"), hole, w), 0, 1);
            g->addWidget(pcbLabeled(QStringLiteral("Минимальное кольцо"), ring, w), 1, 1);
            g->addWidget(pcbLabeled(QStringLiteral("Максимальный диаметр площадки"), pad, w), 3, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Расчётное кольцо"), ar, w), 3, 1);
            g->setRowStretch(4, 1);
            m_stack->insertWidget(n == 1 ? 2 : 4, w);
            (n == 1 ? m_thPitch1 : m_thPitch2) = pitch; (n == 1 ? m_thW1 : m_thW2) = cw; (n == 1 ? m_thS1 : m_thS2) = sp;
            (n == 1 ? m_thHole1 : m_thHole2) = hole; (n == 1 ? m_thRing1 : m_thRing2) = ring;
            (n == 1 ? m_thPad1 : m_thPad2) = pad; (n == 1 ? m_thAr1 : m_thAr2) = ar;
        }
        // 3 и 5 — проводники между площадками BGA
        for (int n = 1; n <= 2; ++n) {
            auto *w = new QWidget(m_stack);
            auto *g = new QGridLayout(w);
            g->setContentsMargins(0, 0, 0, 0);
            g->setVerticalSpacing(8);
            PcbField *pitch = lenIn(1.0, w), *land = lenIn(0.45, w), *sp = lenIn(0.1, w), *out = lenOut(w);
            g->addWidget(pcbLabeled(QStringLiteral("Расстояние между центрами площадок"), pitch, w), 0, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Диаметр площадки"), land, w), 1, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Зазор «площадка — проводник»"), sp, w), 2, 0);
            g->addWidget(pcbLabeled(n == 1 ? QStringLiteral("Максимальная ширина проводника")
                                           : QStringLiteral("Максимальная ширина каждого из двух проводников"), out, w), 3, 0);
            g->setRowStretch(4, 1);
            m_stack->insertWidget(n == 1 ? 3 : 5, w);
            (n == 1 ? m_bgaPitch1 : m_bgaPitch2) = pitch; (n == 1 ? m_bgaLand1 : m_bgaLand2) = land;
            (n == 1 ? m_bgaS1 : m_bgaS2) = sp; (n == 1 ? m_bgaW1 : m_bgaW2) = out;
        }
        // 6 — угол–угол
        {
            auto *w = new QWidget(m_stack);
            auto *g = new QGridLayout(w);
            g->setContentsMargins(0, 0, 0, 0);
            g->setVerticalSpacing(8);
            m_sideA = lenIn(0.64, w);
            m_sideB = lenIn(0.64, w);
            m_diag = lenOut(w);
            m_drillMin = lenOut(w);
            m_drillMax = lenOut(w);
            g->addWidget(pcbLabeled(QStringLiteral("Сторона a"), m_sideA, w), 0, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Сторона b"), m_sideB, w), 1, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Расстояние между углами"), m_diag, w), 2, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Рекомендуемое сверло: минимум"), m_drillMin, w), 3, 0);
            g->addWidget(pcbLabeled(QStringLiteral("Рекомендуемое сверло: максимум"), m_drillMax, w), 4, 0);
            g->addWidget(pcbNote(QStringLiteral("Для квадратного или прямоугольного вывода отверстие должно вмещать диагональ; "
                                                "запас +0,10…0,25 мм."), w), 5, 0);
            g->setRowStretch(6, 1);
            m_stack->addWidget(w);
        }
    }

    PcbChoice *m_type, *m_style;
    QStackedWidget *m_stack;
    QVector<PcbField *> m_lenFields;
    QComboBox *m_holeType, *m_ball;
    PcbField *m_hole, *m_ring, *m_iso, *m_ext, *m_intSig, *m_plane, *m_inner, *m_spoke;
    PcbField *m_landNom, *m_landMin, *m_landMax;
    PcbField *m_thPitch1, *m_thW1, *m_thS1, *m_thHole1, *m_thRing1, *m_thPad1, *m_thAr1;
    PcbField *m_thPitch2, *m_thW2, *m_thS2, *m_thHole2, *m_thRing2, *m_thPad2, *m_thAr2;
    PcbField *m_bgaPitch1, *m_bgaLand1, *m_bgaS1, *m_bgaW1;
    PcbField *m_bgaPitch2, *m_bgaLand2, *m_bgaS2, *m_bgaW2;
    PcbField *m_sideA, *m_sideB, *m_diag, *m_drillMin, *m_drillMax;
    QStringList m_info;
};

// ================================================================================================
//  Тепловой режим
// ================================================================================================
class ThermalPage : public PcbPage
{
public:
    explicit ThermalPage(QWidget *parent)
        : PcbPage(QStringLiteral("Тепловой режим"), QStringLiteral("Тепловой режим"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        QGridLayout *a;
        QGroupBox *ga = pcbSubGroup(QStringLiteral("Температура перехода устройства"), frame(), &a);
        m_rth = makeInput(3.0, QStringLiteral("°C/Вт"));
        m_p = makeInput(5.0, QStringLiteral("Вт"));
        m_tj = makeOutput(QStringLiteral("°C"), 3);
        a->addWidget(pcbLabeled(QStringLiteral("Тепловое сопротивление устройства"), m_rth, ga), 0, 0);
        a->addWidget(pcbLabeled(QStringLiteral("Рассеиваемая мощность"), m_p, ga), 1, 0);
        a->addWidget(pcbLabeled(QStringLiteral("Температура перехода"), m_tj, ga), 2, 0);
        auto *chip = new PcbDiagram(PcbDiagram::ThermalChip, ga);
        chip->setMinimumHeight(140);
        a->addWidget(chip, 0, 1, 3, 1);
        a->setColumnStretch(1, 1);
        g->addWidget(ga, 0, 0);

        QGridLayout *b;
        QGroupBox *gb = pcbSubGroup(QStringLiteral("Подбор радиатора"), frame(), &b);
        m_rhs = makeInput(25.0, QStringLiteral("°C/Вт"));
        m_p2 = makeInput(5.0, QStringLiteral("Вт"));
        m_ths = makeOutput(QStringLiteral("°C"), 3);
        b->addWidget(pcbLabeled(QStringLiteral("Тепловое сопротивление радиатора"), m_rhs, gb), 0, 0);
        b->addWidget(pcbLabeled(QStringLiteral("Рассеиваемая мощность"), m_p2, gb), 1, 0);
        b->addWidget(pcbLabeled(QStringLiteral("Температура радиатора"), m_ths, gb), 2, 0);
        auto *hs = new PcbDiagram(PcbDiagram::HeatSink, gb);
        hs->setMinimumHeight(140);
        b->addWidget(hs, 0, 1, 3, 1);
        b->addWidget(pcbNote(QStringLiteral("Тепловое сопротивление радиатора обычно приводится для нагрева на 75 °C при естественной конвекции."), gb), 3, 0, 1, 2);
        b->setColumnStretch(1, 1);
        g->addWidget(gb, 1, 0);
        g->setRowStretch(2, 1);
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.ambient = true; return c; }

    void solve() override
    {
        const double nan = PcbFmt::kNaN, ta = context().ambient;
        m_tj->setValue(m_rth->hasValue() && m_p->hasValue() ? pcbcore::junctionTemperature(ta, m_rth->value(), m_p->value()) : nan);
        m_ths->setValue(m_rhs->hasValue() && m_p2->hasValue() ? pcbcore::junctionTemperature(ta, m_rhs->value(), m_p2->value()) : nan);
    }

private:
    PcbField *m_rth, *m_p, *m_tj, *m_rhs, *m_p2, *m_ths;
};

// ================================================================================================
//  Механика
// ================================================================================================
class MechanicalPage : public PcbPage
{
public:
    explicit MechanicalPage(QWidget *parent)
        : PcbPage(QStringLiteral("Механика"), QStringLiteral("Механические данные"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        QGridLayout *w;
        QGroupBox *wire = pcbSubGroup(QStringLiteral("Калибр провода (AWG)"), frame(), &w);
        m_awg = new QComboBox(wire);
        for (const auto &gg : pcbcore::wireGauges())
            m_awg->addItem(QString::fromStdString(gg.name));
        m_load = makeInput(5.0, QStringLiteral("А"));
        m_len = makeInput(10.0);
        m_len->setUnits(PcbUnits::lengthM());
        m_dia = makeOutput(QString(), 4);
        m_dia->setUnits(PcbUnits::lengthMm());
        m_ohms = makeOutput(QStringLiteral("Ом/км"), 5);
        m_chassis = makeOutput(QStringLiteral("А"), 0);
        m_tx = makeOutput(QStringLiteral("А"), 3);
        m_drop = makeOutput(QStringLiteral("В"), 4);
        auto *btn = pcbButton(QStringLiteral("!"), wire);
        btn->setMaximumWidth(32);
        connect(btn, &QPushButton::clicked, this, [this] { PcbDialogs::wireInfo(this); });
        w->addWidget(pcbLabeled(QStringLiteral("Калибр AWG"), m_awg, wire), 0, 0);
        w->addWidget(btn, 0, 1, Qt::AlignBottom);
        w->addWidget(pcbLabeled(QStringLiteral("Ток нагрузки"), m_load, wire), 1, 0, 1, 2);
        w->addWidget(pcbLabeled(QStringLiteral("Длина провода"), m_len, wire), 2, 0, 1, 2);
        w->addWidget(pcbLabeled(QStringLiteral("Падение напряжения"), m_drop, wire), 3, 0, 1, 2);
        w->addWidget(pcbLabeled(QStringLiteral("Диаметр"), m_dia, wire), 0, 2);
        w->addWidget(pcbLabeled(QStringLiteral("Сопротивление"), m_ohms, wire), 1, 2);
        w->addWidget(pcbLabeled(QStringLiteral("Макс. ток при монтаже в шасси"), m_chassis, wire), 2, 2);
        w->addWidget(pcbLabeled(QStringLiteral("Макс. ток для передачи мощности"), m_tx, wire), 3, 2);
        w->setColumnStretch(0, 1);
        w->setColumnStretch(2, 1);
        g->addWidget(wire, 0, 0);

        QGridLayout *dl;
        QGroupBox *drill = pcbSubGroup(QStringLiteral("Таблица сверл"), frame(), &dl);
        auto *dt = new QTableWidget(int(pcbcore::drillChart().size()), 3, drill);
        dt->setHorizontalHeaderLabels({ QStringLiteral("Сверло №"), QStringLiteral("Диаметр, дюйм"), QStringLiteral("Диаметр, мм") });
        dt->setEditTriggers(QAbstractItemView::NoEditTriggers);
        dt->setSelectionMode(QAbstractItemView::NoSelection);
        dt->verticalHeader()->setVisible(false);
        dt->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int i = 0; i < dt->rowCount(); ++i) {
            const auto &s = pcbcore::drillChart()[i];
            const double mm = std::floor(s.inches * 25.4 * 1000.0 + 1e-6) / 1000.0;
            dt->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(s.name)));
            auto *in = new QTableWidgetItem(PcbFmt::fixed(s.inches, 4));
            auto *mmi = new QTableWidgetItem(PcbFmt::fixed(mm, 3));
            in->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            mmi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            dt->setItem(i, 1, in);
            dt->setItem(i, 2, mmi);
        }
        dl->addWidget(dt, 0, 0);
        g->addWidget(drill, 0, 1, 2, 1);

        QGridLayout *sl;
        QGroupBox *screws = pcbSubGroup(QStringLiteral("Метрические винты"), frame(), &sl);
        auto *st = new QTableWidget(int(pcbcore::metricScrews().size()), 3, screws);
        st->setHorizontalHeaderLabels({ QStringLiteral("Размер"), QStringLiteral("Диаметр, мм"), QStringLiteral("Диаметр, дюйм") });
        st->setEditTriggers(QAbstractItemView::NoEditTriggers);
        st->setSelectionMode(QAbstractItemView::NoSelection);
        st->verticalHeader()->setVisible(false);
        st->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int i = 0; i < st->rowCount(); ++i) {
            const auto &s = pcbcore::metricScrews()[i];
            st->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(s.name)));
            auto *mm = new QTableWidgetItem(PcbFmt::trimmed(s.mm));
            auto *in = new QTableWidgetItem(PcbFmt::fixed(s.mm / 25.4, 3));
            mm->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            in->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            st->setItem(i, 1, mm);
            st->setItem(i, 2, in);
        }
        sl->addWidget(st, 0, 0);
        g->addWidget(screws, 1, 0);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(0, 1);
        g->setRowStretch(1, 1);

        connect(m_awg, &QComboBox::currentIndexChanged, this, [this](int) { recalc(); });
        m_awg->setCurrentIndex(0);
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }

    void contextChanged() override
    {
        const bool imp = context().imperial;
        m_len->setUnitMode(imp ? 1 : 0);
        if (imp != m_lastImperial) {                      // длину провода вводят в метрах или футах: число не пересчитываем
            m_lastImperial = imp;
            m_len->setValue(10.0 * (imp ? 0.3048 : 1.0));
        }
        m_dia->setUnitMode(context().unitMode());
        m_ohms->setUnit(imp ? QStringLiteral("Ом/1000 фут") : QStringLiteral("Ом/км"));
    }

    void solve() override
    {
        const auto &list = pcbcore::wireGauges();
        const auto &gg = list[std::clamp(m_awg->currentIndex(), 0, int(list.size()) - 1)];
        const double lenM = m_len->hasValue() ? m_len->value() : 0.0;
        const auto r = pcbcore::wireGaugeCalc(gg, m_load->hasValue() ? m_load->value() : 0.0, lenM);
        m_dia->setValue(r.diameterMm);
        m_ohms->setValue(context().imperial ? r.ohmsPerKm * 0.3048 : r.ohmsPerKm);
        m_chassis->setValue(r.chassisAmps);
        m_chassis->setDecimals(r.chassisAmps < 10 ? 2 : 0);
        m_tx->setValue(r.powerTxAmps);
        m_drop->setValue(r.voltageDrop);
    }

private:
    QComboBox *m_awg;
    PcbField *m_load, *m_len, *m_dia, *m_ohms, *m_chassis, *m_tx, *m_drop;
    bool m_lastImperial = false;
};

// ================================================================================================
//  Встроенные резисторы
// ================================================================================================
class EmbeddedPage : public PcbPage
{
public:
    explicit EmbeddedPage(QWidget *parent)
        : PcbPage(QStringLiteral("Встроенные резисторы"), QStringLiteral("Встроенные резисторы (PTF)"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_type = new PcbChoice(QStringLiteral("Тип встроенного резистора"), frame());
        m_type->setOptions({ QStringLiteral("Аддитивный (трафаретная печать)"), QStringLiteral("Субтрактивный (медь–никель)"),
                             QStringLiteral("Кольцевой встроенный резистор") });
        g->addWidget(m_type, 0, 0);
        auto *d = new PcbDiagram(PcbDiagram::EmbeddedResistor, frame());
        d->setMinimumHeight(170);
        g->addWidget(d, 0, 1, 2, 1);
        g->addWidget(pcbNote(QStringLiteral("Подробности о встроенных резисторах — в стандарте IPC-2316."), frame()), 1, 0);

        m_rs = makeInput(25.0, QStringLiteral("Ом/□"));
        m_l = makeInput(2.54);
        m_w = makeInput(1.27);
        for (PcbField *f : { m_l, m_w })
            f->setUnits(PcbUnits::lengthMm());
        m_lLabel = new QLabel(frame());
        m_lLabel->setObjectName(QStringLiteral("pcbLabel"));
        m_wLabel = new QLabel(frame());
        m_wLabel->setObjectName(QStringLiteral("pcbLabel"));
        auto *form = new QGridLayout;
        form->setVerticalSpacing(8);
        form->addWidget(pcbLabeled(QStringLiteral("Поверхностное сопротивление материала"), m_rs, frame()), 0, 0);
        auto *lbox = new QWidget(frame());
        auto *ll = new QVBoxLayout(lbox);
        ll->setContentsMargins(0, 0, 0, 0);
        ll->setSpacing(3);
        ll->addWidget(m_lLabel);
        ll->addWidget(m_l);
        auto *wbox = new QWidget(frame());
        auto *wl = new QVBoxLayout(wbox);
        wl->setContentsMargins(0, 0, 0, 0);
        wl->setSpacing(3);
        wl->addWidget(m_wLabel);
        wl->addWidget(m_w);
        form->addWidget(lbox, 1, 0);
        form->addWidget(wbox, 2, 0);
        g->addLayout(form, 2, 0);

        m_area = makeOutput(QString(), 3);
        m_area->setUnits(PcbUnits::areaMm2());
        m_ohms = makeOutput(QStringLiteral("Ом"), 3);
        auto *outs = new QVBoxLayout;
        outs->addWidget(pcbLabeled(QStringLiteral("Площадь резистора"), m_area, frame()));
        outs->addWidget(pcbLabeled(QStringLiteral("Сопротивление"), m_ohms, frame()));
        g->addLayout(outs, 2, 1);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(3, 1);

        connect(m_type, &PcbChoice::currentChanged, this, [this](int) { updateLabels(); recalc(); });
        updateLabels();
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }

    void contextChanged() override
    {
        m_l->setUnitMode(context().unitMode());
        m_w->setUnitMode(context().unitMode());
        m_area->setUnitMode(context().imperial ? 1 : 0);
    }

    void solve() override
    {
        const double nan = PcbFmt::kNaN;
        if (!(m_rs->hasValue() && m_l->hasValue() && m_w->hasValue())) {
            m_area->setValue(nan);
            m_ohms->setValue(nan);
            return;
        }
        if (m_type->currentIndex() == 2) {
            const double a = kPi / 4.0 * (m_l->value() * m_l->value() - m_w->value() * m_w->value());
            m_area->setValue(a > 0 ? a : nan);
            m_ohms->setValue(pcbcore::annularResistorOhms(m_rs->value(), m_l->value(), m_w->value()));
        } else {
            m_area->setValue(m_l->value() * m_w->value());
            m_ohms->setValue(pcbcore::embeddedResistorOhms(m_rs->value(), m_l->value(), m_w->value()));
        }
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    void updateLabels()
    {
        const bool ring = m_type->currentIndex() == 2;
        m_lLabel->setText(ring ? QStringLiteral("Внешний диаметр") : QStringLiteral("Длина (L)"));
        m_wLabel->setText(ring ? QStringLiteral("Внутренний диаметр") : QStringLiteral("Ширина (W)"));
    }

    PcbChoice *m_type;
    PcbField *m_rs, *m_l, *m_w, *m_area, *m_ohms;
    QLabel *m_lLabel, *m_wLabel;
};

} // namespace

PcbPage *makePdnPage(QWidget *parent) { return new PdnPage(parent); }
PcbPage *makePlanarPage(QWidget *parent) { return new PlanarPage(parent); }
PcbPage *makePadstackPage(QWidget *parent) { return new PadstackPage(parent); }
PcbPage *makeThermalPage(QWidget *parent) { return new ThermalPage(parent); }
PcbPage *makeMechanicalPage(QWidget *parent) { return new MechanicalPage(parent); }
PcbPage *makeEmbeddedPage(QWidget *parent) { return new EmbeddedPage(parent); }
