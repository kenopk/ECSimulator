// Страницы электрических расчётов: конвертер, закон Ома и цепочки, кварц и PPM, реактивные сопротивления.
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QVBoxLayout>
#include <array>
#include <functional>

#include "pcb/PcbDialogs.h"
#include "pcb/PcbPagesInternal.h"

namespace {

QVector<PcbUnit> freqUnits()
{
    return { { QStringLiteral("Гц"), 1.0 }, { QStringLiteral("кГц"), 1e3 }, { QStringLiteral("МГц"), 1e6 }, { QStringLiteral("ГГц"), 1e9 } };
}

// ================================================================================================
//  Реактивные сопротивления
// ================================================================================================
class ReactancePage : public PcbPage
{
public:
    explicit ReactancePage(QWidget *parent)
        : PcbPage(QStringLiteral("Реактивное сопротивление"), QStringLiteral("Реактивное сопротивление XC / XL"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_f = new PcbField(PcbField::Input, frame());
        m_f->setUnitChoices(freqUnits(), 2);
        m_f->setValue(1e6);
        m_c = new PcbField(PcbField::Input, frame());
        m_c->setUnitChoices({ { QStringLiteral("мкФ"), 1e-6 }, { QStringLiteral("нФ"), 1e-9 }, { QStringLiteral("пФ"), 1e-12 } }, 0);
        m_c->setValue(1e-6);
        m_l = new PcbField(PcbField::Input, frame());
        m_l->setUnitChoices({ { QStringLiteral("мГн"), 1e-3 }, { QStringLiteral("мкГн"), 1e-6 }, { QStringLiteral("нГн"), 1e-9 } }, 0);
        m_l->setValue(1e-3);
        for (PcbField *f : { m_f, m_c, m_l })
            connect(f, &PcbField::edited, this, [this] { recalc(); });
        g->addWidget(pcbLabeled(QStringLiteral("Частота"), m_f, frame()), 0, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Ёмкость"), m_c, frame()), 1, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Индуктивность"), m_l, frame()), 2, 0);
        auto *d = new PcbDiagram(PcbDiagram::Reactance, frame());
        d->setMinimumHeight(120);
        g->addWidget(d, 0, 1, 3, 1);
        g->addWidget(pcbFormula(QStringLiteral("X<sub>C</sub> = 1/(2π f C) &nbsp;&nbsp; X<sub>L</sub> = 2π f L &nbsp;&nbsp; f<sub>0</sub> = 1/(2π√(LC))"), frame()), 3, 0, 1, 2);
        m_xc = makeOutput(QStringLiteral("Ом"), 4);
        m_xl = makeOutput(QStringLiteral("Ом"), 4);
        m_f0 = makeOutput(QStringLiteral("Гц"), 4);
        g->addWidget(pcbLabeled(QStringLiteral("Xc"), m_xc, frame()), 4, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Xl"), m_xl, frame()), 4, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Резонансная частота"), m_f0, frame()), 5, 0);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(6, 1);
    }

    void solve() override
    {
        const auto r = pcbcore::reactance(m_f->value(), m_c->value(), m_l->value());
        m_xc->setValue(r.xc);
        m_xl->setValue(r.xl);
        m_f0->setValue(r.resonanceHz);
    }

private:
    PcbField *m_f, *m_c, *m_l, *m_xc, *m_xl, *m_f0;
};

// ================================================================================================
//  PPM и кварц
// ================================================================================================
class PpmXtalPage : public PcbPage
{
public:
    explicit PpmXtalPage(QWidget *parent)
        : PcbPage(QStringLiteral("PPM / кварц"), QStringLiteral("Калькулятор PPM"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        QGridLayout *cl;
        QGroupBox *caps = pcbSubGroup(QStringLiteral("Нагрузочные конденсаторы кварца"), frame(), &cl);
        m_load = makeInput(10.0, QStringLiteral("пФ"));
        m_stray = makeInput(3.0, QStringLiteral("пФ"));
        m_c1 = makeInput(14.0, QStringLiteral("пФ"));
        m_c2 = makeInput(14.0, QStringLiteral("пФ"));
        m_fromC = makeOutput(QStringLiteral("пФ"), 2);
        m_thumb = makeOutput(QStringLiteral("пФ"), 2);
        cl->addWidget(pcbLabeled(QStringLiteral("Ёмкость нагрузки (по даташиту)"), m_load, caps), 0, 0);
        cl->addWidget(pcbLabeled(QStringLiteral("Паразитная ёмкость"), m_stray, caps), 1, 0);
        cl->addWidget(pcbLabeled(QStringLiteral("C1"), m_c1, caps), 0, 1);
        cl->addWidget(pcbLabeled(QStringLiteral("C2"), m_c2, caps), 1, 1);
        cl->addWidget(pcbLabeled(QStringLiteral("По C1–C2"), m_fromC, caps), 0, 2);
        cl->addWidget(pcbLabeled(QStringLiteral("Правило (C1 = C2)"), m_thumb, caps), 1, 2);
        auto *d = new PcbDiagram(PcbDiagram::Crystal, caps);
        d->setMinimumSize(120, 90);
        cl->addWidget(d, 0, 3, 2, 1);
        auto *help = pcbButton(QStringLiteral("Справка"), caps);
        connect(help, &QPushButton::clicked, this, [this] { PcbDialogs::xtalInfo(this); });
        cl->addWidget(help, 2, 3, Qt::AlignRight);
        g->addWidget(caps, 0, 0, 1, 2);

        QGridLayout *h2p;
        QGroupBox *a = pcbSubGroup(QStringLiteral("Гц → PPM"), frame(), &h2p);
        m_center1 = makeInput(32000.0, QStringLiteral("Гц"));
        m_max1 = makeInput(32001.0, QStringLiteral("Гц"));
        m_var1 = makeOutput(QStringLiteral("Гц"), 2);
        m_ppm1 = makeOutput(QStringLiteral("PPM"), 2);
        h2p->addWidget(pcbLabeled(QStringLiteral("Центральная частота"), m_center1, a), 0, 0);
        h2p->addWidget(pcbLabeled(QStringLiteral("Максимальная частота"), m_max1, a), 1, 0);
        h2p->addWidget(pcbLabeled(QStringLiteral("Отклонение частоты"), m_var1, a), 2, 0);
        h2p->addWidget(pcbLabeled(QStringLiteral("PPM"), m_ppm1, a), 3, 0);
        g->addWidget(a, 1, 0);

        QGridLayout *p2h;
        QGroupBox *b = pcbSubGroup(QStringLiteral("PPM → Гц"), frame(), &p2h);
        m_center2 = makeInput(50e6, QStringLiteral("Гц"));
        m_ppm2 = makeInput(25.0, QStringLiteral("PPM"));
        m_var2 = makeOutput(QStringLiteral("Гц"), 2);
        m_max2 = makeOutput(QStringLiteral("Гц"), 2);
        m_min2 = makeOutput(QStringLiteral("Гц"), 2);
        p2h->addWidget(pcbLabeled(QStringLiteral("Центральная частота"), m_center2, b), 0, 0);
        p2h->addWidget(pcbLabeled(QStringLiteral("PPM"), m_ppm2, b), 1, 0);
        p2h->addWidget(pcbLabeled(QStringLiteral("Отклонение частоты"), m_var2, b), 2, 0);
        p2h->addWidget(pcbLabeled(QStringLiteral("Максимальная частота"), m_max2, b), 3, 0);
        p2h->addWidget(pcbLabeled(QStringLiteral("Минимальная частота"), m_min2, b), 4, 0);
        g->addWidget(b, 1, 1);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(2, 1);
    }

    void solve() override
    {
        const double nan = PcbFmt::kNaN;
        m_fromC->setValue(pcbcore::xtalLoadFromCaps(m_c1->value(), m_c2->value(), m_stray->hasValue() ? m_stray->value() : 0.0));
        m_thumb->setValue(m_load->hasValue() && m_stray->hasValue() ? pcbcore::xtalRuleOfThumb(m_load->value(), m_stray->value()) : nan);
        if (m_center1->hasValue() && m_max1->hasValue()) {
            const auto r = pcbcore::hzToPpm(m_center1->value(), m_max1->value());
            m_var1->setValue(r.variationHz);
            m_ppm1->setValue(r.ppm);
        } else {
            m_var1->setValue(nan);
            m_ppm1->setValue(nan);
        }
        if (m_center2->hasValue() && m_ppm2->hasValue()) {
            const auto r = pcbcore::ppmToHz(m_center2->value(), m_ppm2->value());
            m_var2->setValue(r.variationHz);
            m_max2->setValue(r.maxHz);
            m_min2->setValue(r.minHz);
        } else {
            for (PcbField *f : { m_var2, m_max2, m_min2 }) f->setValue(nan);
        }
    }

private:
    PcbField *m_load, *m_stray, *m_c1, *m_c2, *m_fromC, *m_thumb;
    PcbField *m_center1, *m_max1, *m_var1, *m_ppm1;
    PcbField *m_center2, *m_ppm2, *m_var2, *m_max2, *m_min2;
};

// ================================================================================================
//  Конвертер
// ================================================================================================
class ConversionPage : public PcbPage
{
public:
    explicit ConversionPage(QWidget *parent)
        : PcbPage(QStringLiteral("Конвертер"), QStringLiteral("Преобразование величин"), parent)
    {
        QGridLayout *g = pcbGrid(frame());

        // --- расстояния -------------------------------------------------------------------------------
        QGridLayout *dl;
        QGroupBox *dist = pcbSubGroup(QStringLiteral("Расстояния"), frame(), &dl);
        m_dist = new PcbField(PcbField::Input, dist);
        m_dist->setUnitChoices({ { QStringLiteral("мм"), 1.0 }, { QStringLiteral("мил"), 0.0254 }, { QStringLiteral("мкм"), 0.001 },
                                 { QStringLiteral("дюйм"), 25.4 } }, 0);
        m_dist->setValue(0.254);
        connect(m_dist, &PcbField::edited, this, [this] { recalc(); });
        dl->addWidget(pcbLabeled(QStringLiteral("Ввод"), m_dist, dist), 0, 0, 1, 2);
        const char *names[] = { "мм", "мил", "мкм", "дюйм" };
        for (int i = 0; i < 4; ++i) {
            m_distOut[i] = makeOutput(QString::fromUtf8(names[i]), i == 3 ? 4 : 3);
            dl->addWidget(m_distOut[i], 1 + i / 2, i % 2);
        }
        g->addWidget(dist, 0, 0);

        // --- температура ------------------------------------------------------------------------------
        QGridLayout *tl;
        QGroupBox *temp = pcbSubGroup(QStringLiteral("Температура"), frame(), &tl);
        m_temp = new PcbField(PcbField::Input, temp);
        m_temp->setUnitChoices({ { QStringLiteral("°C"), 1.0 }, { QStringLiteral("°F"), 1.0 } }, 0);
        m_temp->setValue(72.0);
        connect(m_temp, &PcbField::edited, this, [this] { recalc(); });
        m_tempOut = makeOutput(QStringLiteral("°F"), 2);
        tl->addWidget(pcbLabeled(QStringLiteral("Ввод"), m_temp, temp), 0, 0);
        tl->addWidget(pcbLabeled(QStringLiteral("Результат"), m_tempOut, temp), 1, 0);
        tl->setRowStretch(2, 1);
        g->addWidget(temp, 0, 1);

        // --- дБ и усиление -----------------------------------------------------------------------------
        QGridLayout *bl;
        QGroupBox *db = pcbSubGroup(QStringLiteral("дБ и коэффициент усиления по напряжению"), frame(), &bl);
        m_dbmIn = makeInput(-60.0, QStringLiteral("дБм"));
        m_gain = makeInput(23.0, QStringLiteral("дБ"));
        m_att = makeInput(10.0, QStringLiteral("дБ"));
        m_dbmOut = makeOutput(QStringLiteral("дБм"), 2);
        m_vIn = makeInput(1.0, QStringLiteral("В"));
        m_vGain = makeOutput(QString(), 4);
        m_vOut = makeOutput(QStringLiteral("В"), 4);
        bl->addWidget(pcbLabeled(QStringLiteral("Вход, дБм"), m_dbmIn, db), 0, 0);
        bl->addWidget(pcbLabeled(QStringLiteral("Усиление"), m_gain, db), 0, 1);
        bl->addWidget(pcbLabeled(QStringLiteral("Аттенюатор"), m_att, db), 0, 2);
        bl->addWidget(pcbLabeled(QStringLiteral("Выход, дБм"), m_dbmOut, db), 0, 3);
        bl->addWidget(pcbLabeled(QStringLiteral("Вход, В"), m_vIn, db), 1, 0);
        bl->addWidget(pcbLabeled(QStringLiteral("Усиление по напряжению"), m_vGain, db), 1, 1, 1, 2);
        bl->addWidget(pcbLabeled(QStringLiteral("Выход, В"), m_vOut, db), 1, 3);
        g->addWidget(db, 1, 0, 1, 2);

        // --- прямоугольная ↔ полярная форма ------------------------------------------------------------------
        QGridLayout *rl;
        QGroupBox *rect = pcbSubGroup(QStringLiteral("Прямоугольная ↔ полярная форма"), frame(), &rl);
        m_re = makeInput(50.0);
        m_im = makeInput(0.0);
        m_mag = makeOutput(QString(), 4);
        m_ang = makeOutput(QStringLiteral("°"), 4);
        m_swapRect = new QCheckBox(QStringLiteral("поменять"), rect);
        auto *send = pcbButton(QStringLiteral("Отправить →"), rect);
        m_reL = new QLabel(QStringLiteral("Действительная часть"), rect);
        m_imL = new QLabel(QStringLiteral("Мнимая часть (j)"), rect);
        m_magL = new QLabel(QStringLiteral("Модуль"), rect);
        m_angL = new QLabel(QStringLiteral("Угол"), rect);
        for (QLabel *l : { m_reL, m_imL, m_magL, m_angL })
            l->setObjectName(QStringLiteral("pcbLabel"));
        auto addLab = [&](QLabel *lab, PcbField *f, int row, int col) {
            auto *w = new QWidget(rect);
            auto *v = new QVBoxLayout(w);
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(3);
            v->addWidget(lab);
            v->addWidget(f);
            rl->addWidget(w, row, col);
        };
        addLab(m_reL, m_re, 0, 0);
        addLab(m_imL, m_im, 0, 1);
        addLab(m_magL, m_mag, 1, 0);
        addLab(m_angL, m_ang, 1, 1);
        rl->addWidget(send, 2, 0);
        rl->addWidget(m_swapRect, 2, 1);
        g->addWidget(rect, 2, 0);

        // --- углы ----------------------------------------------------------------------------------------
        QGridLayout *al;
        QGroupBox *angle = pcbSubGroup(QStringLiteral("Углы"), frame(), &al);
        m_deg = makeInput(1.0, QStringLiteral("°"));
        m_rad = makeOutput(QStringLiteral("рад"), 4);
        m_swapAng = new QCheckBox(QStringLiteral("поменять"), angle);
        m_degL = new QLabel(QStringLiteral("Градусы"), angle);
        m_radL = new QLabel(QStringLiteral("Радианы"), angle);
        m_degL->setObjectName(QStringLiteral("pcbLabel"));
        m_radL->setObjectName(QStringLiteral("pcbLabel"));
        auto addAng = [&](QLabel *lab, PcbField *f, int row) {
            auto *w = new QWidget(angle);
            auto *v = new QVBoxLayout(w);
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(3);
            v->addWidget(lab);
            v->addWidget(f);
            al->addWidget(w, row, 0);
        };
        addAng(m_degL, m_deg, 0);
        addAng(m_radL, m_rad, 1);
        al->addWidget(m_swapAng, 2, 0);
        g->addWidget(angle, 2, 1);

        // --- справочные таблицы ----------------------------------------------------------------------------
        QGridLayout *cl;
        QGroupBox *charts = pcbSubGroup(QStringLiteral("Таблицы и справка"), frame(), &cl);
        struct Btn { const char *text; std::function<void()> fn; };
        const Btn buttons[] = {
            { "Приставки СИ", [this] { PcbDialogs::siPrefixes(this); } },
            { "Закон Ома", [this] { emit sendValue(PcbPageId::OhmsLaw, QString(), 0.0); } },
            { "Резисторы: цветовой код", [this] { PcbDialogs::resistorCalculator(this); } },
            { "Таблица дБм", [this] { PcbDialogs::dbmChart(this); } },
            { "Конденсаторы мкФ–нФ–пФ", [this] { PcbDialogs::capacitorChart(this); } },
            { "Правила проектирования", [this] { PcbDialogs::designRules(this); } },
        };
        for (int i = 0; i < 6; ++i) {
            auto *b = pcbButton(QString::fromUtf8(buttons[i].text), charts);
            connect(b, &QPushButton::clicked, this, buttons[i].fn);
            cl->addWidget(b, i / 3, i % 3);
        }
        g->addWidget(charts, 3, 0, 1, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(4, 1);

        for (PcbField *f : { m_re, m_im, m_mag, m_ang, m_deg, m_rad })
            connect(f, &PcbField::edited, this, [this] { recalc(); });
        connect(m_swapRect, &QCheckBox::toggled, this, [this](bool on) {
            m_re->setOutputMode(on);
            m_im->setOutputMode(on);
            m_mag->setOutputMode(!on);
            m_ang->setOutputMode(!on);
            recalc();
        });
        connect(m_swapAng, &QCheckBox::toggled, this, [this](bool on) {
            m_deg->setOutputMode(on);
            m_rad->setOutputMode(!on);
            recalc();
        });
        connect(send, &QPushButton::clicked, this, [this] {
            const double a = m_ang->value();
            if (m_swapAng->isChecked())
                m_swapAng->setChecked(false);
            if (std::isfinite(a)) {
                m_deg->setValue(a);
                recalc();
            }
        });
        for (PcbField *f : { m_dbmIn, m_gain, m_att, m_vIn })
            connect(f, &PcbField::edited, this, [this] { recalc(); });
        m_ang->setOutputMode(true);
    }

    QStringList infoItems() const override
    {
        QStringList l;
        l << PcbFmt::info(QStringLiteral("Мощность на выходе (дБм → Вт)"), m_watts);
        return l;
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }

    void contextChanged() override { m_dist->setUnitIndex(context().imperial ? 1 : 0); }

    void solve() override
    {
        const double nan = PcbFmt::kNaN;
        // расстояния
        const int di = m_dist->unitIndex();
        const double mm = m_dist->value();
        const double factors[4] = { 1.0, 1.0 / 0.0254, 1000.0, 1.0 / 25.4 };       // из мм
        for (int i = 0; i < 4; ++i) {                                                // порядок и у ввода, и у вывода: мм, мил, мкм, дюйм
            const bool self = i == di;
            m_distOut[i]->setEnabled(!self);
            if (self || !std::isfinite(mm))
                m_distOut[i]->setText(QString());
            else
                m_distOut[i]->setValue(mm * factors[i]);
        }
        // температура
        const double t = m_temp->value();
        if (m_temp->unitIndex() == 0) {
            m_tempOut->setUnit(QStringLiteral("°F"));
            m_tempOut->setValue(std::isfinite(t) ? pcbcore::celsiusToFahrenheit(t) : nan);
        } else {
            m_tempOut->setUnit(QStringLiteral("°C"));
            m_tempOut->setValue(std::isfinite(t) ? pcbcore::fahrenheitToCelsius(t) : nan);
        }
        // дБ
        double out = nan;
        if (m_dbmIn->hasValue() && m_gain->hasValue() && m_att->hasValue())
            out = pcbcore::dbmOut(m_dbmIn->value(), m_gain->value(), m_att->value());
        m_dbmOut->setValue(out);
        m_watts = std::isfinite(out) ? PcbDialogs::engineering(pcbcore::dbmToWatts(out), QStringLiteral("Вт")) : QStringLiteral("—");
        const double ratio = m_gain->hasValue() ? pcbcore::dbToVoltageRatio(m_gain->value()) : nan;
        m_vGain->setValue(ratio);
        m_vOut->setValue(m_vIn->hasValue() && std::isfinite(ratio) ? m_vIn->value() * ratio : nan);
        // прямоугольная ↔ полярная
        if (!m_swapRect->isChecked()) {
            if (m_re->hasValue() && m_im->hasValue()) {
                const auto p = pcbcore::rectToPolar(m_re->value(), m_im->value());
                m_mag->setValue(p.magnitude);
                m_ang->setValue(p.angleDeg);
            } else {
                m_mag->setValue(nan);
                m_ang->setValue(nan);
            }
        } else {
            if (m_mag->hasValue() && m_ang->hasValue()) {
                double re, im;
                pcbcore::polarToRect(m_mag->value(), m_ang->value(), &re, &im);
                m_re->setValue(re);
                m_im->setValue(im);
            } else {
                m_re->setValue(nan);
                m_im->setValue(nan);
            }
        }
        // углы
        if (!m_swapAng->isChecked())
            m_rad->setValue(m_deg->hasValue() ? pcbcore::degToRad(m_deg->value()) : nan);
        else
            m_deg->setValue(m_rad->hasValue() ? pcbcore::radToDeg(m_rad->value()) : nan);
    }

private:
    PcbField *m_dist, *m_distOut[4], *m_temp, *m_tempOut;
    PcbField *m_dbmIn, *m_gain, *m_att, *m_dbmOut, *m_vIn, *m_vGain, *m_vOut;
    PcbField *m_re, *m_im, *m_mag, *m_ang, *m_deg, *m_rad;
    QCheckBox *m_swapRect, *m_swapAng;
    QLabel *m_reL, *m_imL, *m_magL, *m_angL, *m_degL, *m_radL;
    QString m_watts = QStringLiteral("—");
};

// ================================================================================================
//  Закон Ома и цепочки R / C / L, аттенюаторы
// ================================================================================================
class OhmsLawPage : public PcbPage
{
public:
    explicit OhmsLawPage(QWidget *parent)
        : PcbPage(QStringLiteral("Закон Ома"), QStringLiteral("Закон Ома и цепи R, L, C"), parent)
    {
        auto *outer = new QVBoxLayout(frame());
        outer->setContentsMargins(12, 20, 12, 12);
        outer->setSpacing(10);

        // --- верх: треугольник закона Ома ---------------------------------------------------------------------
        auto *top = new QGridLayout;
        top->setHorizontalSpacing(14);
        auto *wheel = new PcbDiagram(PcbDiagram::OhmWheel, frame());
        wheel->setMinimumSize(240, 220);
        top->addWidget(wheel, 0, 0, 3, 1);
        m_solve = new PcbChoice(QStringLiteral("Найти"), frame());
        m_solve->setOptions({ QStringLiteral("Напряжение"), QStringLiteral("Ток"), QStringLiteral("Сопротивление") });
        top->addWidget(m_solve, 0, 1, 3, 1);
        m_amps = new PcbField(PcbField::Input, frame());
        m_amps->setUnit(QStringLiteral("А"));
        m_amps->setValue(1.0);
        m_ohms = new PcbField(PcbField::Input, frame());
        m_ohms->setUnit(QStringLiteral("Ом"));
        m_ohms->setValue(12.0);
        m_volts = new PcbField(PcbField::Input, frame());
        m_volts->setUnit(QStringLiteral("В"));
        m_volts->setValue(12.0);
        m_watts = makeOutput(QStringLiteral("Вт"), 4);
        for (PcbField *f : { m_amps, m_ohms, m_volts })
            connect(f, &PcbField::edited, this, [this] { recalc(); });
        top->addWidget(pcbLabeled(QStringLiteral("Ток"), m_amps, frame()), 0, 2);
        top->addWidget(pcbLabeled(QStringLiteral("Сопротивление"), m_ohms, frame()), 1, 2);
        top->addWidget(pcbLabeled(QStringLiteral("Напряжение"), m_volts, frame()), 2, 2);
        top->addWidget(pcbLabeled(QStringLiteral("Мощность"), m_watts, frame()), 3, 2);
        top->setColumnStretch(2, 1);
        outer->addLayout(top);

        // --- низ: вкладки с цепочками -----------------------------------------------------------------------------
        m_tabs = new QTabWidget(frame());
        m_tabs->setDocumentMode(true);
        buildLed();
        buildNetwork(QStringLiteral("R послед."), Kind::Resistor, true);
        buildNetwork(QStringLiteral("R паралл."), Kind::Resistor, false);
        buildAttenuator(QStringLiteral("Π-аттенюатор"), true);
        buildAttenuator(QStringLiteral("Т-аттенюатор"), false);
        buildNetwork(QStringLiteral("C послед."), Kind::Capacitor, true);
        buildNetwork(QStringLiteral("C паралл."), Kind::Capacitor, false);
        buildNetwork(QStringLiteral("L послед."), Kind::Inductor, true);
        buildNetwork(QStringLiteral("L паралл."), Kind::Inductor, false);
        outer->addWidget(m_tabs, 1);
        connect(m_solve, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
        connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) { recalc(); });
    }

    QStringList infoItems() const override
    {
        QStringList l;
        l << PcbFmt::info(QStringLiteral("Суммарное значение"), m_infoTotal);
        l << PcbFmt::info(QStringLiteral("Ток цепи"), m_infoCurrent);
        return l;
    }

    void solve() override
    {
        solveOhm();
        m_infoTotal = QStringLiteral("Н/Д");
        m_infoCurrent = QStringLiteral("Н/Д");
        for (const auto &fn : m_updaters)
            fn();
    }

private:
    enum class Kind { Resistor, Capacitor, Inductor };

    void solveOhm()
    {
        const int mode = m_solve->currentIndex();
        m_volts->setOutputMode(mode == 0);
        m_amps->setOutputMode(mode == 1);
        m_ohms->setOutputMode(mode == 2);
        const auto r = pcbcore::ohmsLaw(static_cast<pcbcore::OhmSolve>(mode), m_volts->value(), m_amps->value(), m_ohms->value());
        const double nan = PcbFmt::kNaN;
        if (mode == 0) m_volts->setValue(r.valid ? r.volts : nan);
        if (mode == 1) m_amps->setValue(r.valid ? r.amps : nan);
        if (mode == 2) m_ohms->setValue(r.valid ? r.ohms : nan);
        m_watts->setValue(r.valid ? r.watts : nan);
    }

    void buildLed()
    {
        auto *w = new QWidget(m_tabs);
        auto *g = new QGridLayout(w);
        g->setHorizontalSpacing(12);
        PcbField *v = makeInput(12.0, QStringLiteral("В"));
        PcbField *vf = makeInput(2.0, QStringLiteral("В"));
        PcbField *i = makeInput(0.010);
        i->setUnitChoices({ { QStringLiteral("мА"), 1e-3 }, { QStringLiteral("А"), 1.0 } }, 0);
        i->setValue(0.010);
        PcbField *r = makeOutput(QStringLiteral("Ом"), 1);
        PcbField *p = makeOutput(QStringLiteral("Вт"), 4);
        auto *solveCur = new QCheckBox(QStringLiteral("Найти ток светодиода"), w);
        g->addWidget(pcbLabeled(QStringLiteral("Напряжение питания (+V)"), v, w), 0, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Падение на светодиоде"), vf, w), 1, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Ток светодиода"), i, w), 2, 0);
        g->addWidget(solveCur, 3, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Резистор"), r, w), 0, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Мощность на резисторе"), p, w), 1, 1);
        g->setRowStretch(4, 1);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        m_tabs->addTab(w, QStringLiteral("Светодиод"));
        auto update = [=]() {
            const double nan = PcbFmt::kNaN;
            const bool cur = solveCur->isChecked();
            r->setOutputMode(!cur);
            i->setOutputMode(cur);
            if (!cur) {
                const auto b = pcbcore::ledResistor(v->value(), vf->value(), i->value());
                r->setValue(b.valid ? b.ohms : nan);
                p->setValue(b.valid ? b.watts : nan);
            } else {
                const auto b = pcbcore::ledCurrent(v->value(), vf->value(), r->value());
                i->setValue(b.valid ? b.amps : nan);
                p->setValue(b.valid ? b.watts : nan);
            }
        };
        for (PcbField *f : { v, vf, i, r })
            connect(f, &PcbField::edited, this, [update] { update(); });
        connect(solveCur, &QCheckBox::toggled, this, [update](bool) { update(); });
        m_updaters << update;
    }

    void buildNetwork(const QString &title, Kind kind, bool series)
    {
        auto *w = new QWidget(m_tabs);
        auto *g = new QGridLayout(w);
        g->setHorizontalSpacing(12);
        g->setVerticalSpacing(6);
        const QVector<PcbUnit> units = kind == Kind::Resistor
            ? QVector<PcbUnit>{ { QStringLiteral("Ом"), 1.0 }, { QStringLiteral("кОм"), 1e3 }, { QStringLiteral("МОм"), 1e6 } }
            : kind == Kind::Capacitor
                  ? QVector<PcbUnit>{ { QStringLiteral("мкФ"), 1e-6 }, { QStringLiteral("нФ"), 1e-9 }, { QStringLiteral("пФ"), 1e-12 } }
                  : QVector<PcbUnit>{ { QStringLiteral("мГн"), 1e-3 }, { QStringLiteral("мкГн"), 1e-6 }, { QStringLiteral("нГн"), 1e-9 } };
        const QString unit0 = kind == Kind::Resistor ? QStringLiteral("Ом") : kind == Kind::Capacitor ? QStringLiteral("Ф") : QStringLiteral("Гн");
        auto *count = new QComboBox(w);
        count->addItems({ QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4") });
        g->addWidget(pcbLabeled(QStringLiteral("Число элементов"), count, w), 0, 0);

        std::array<PcbField *, 4> in{};
        std::array<PcbField *, 4> cur{}, pw{};
        for (int k = 0; k < 4; ++k) {
            in[k] = new PcbField(PcbField::Input, w);
            in[k]->setUnitChoices(units, 1);
            in[k]->setValue(units[1].factor * (k == 0 ? 1.0 : 2.2));
            const QString name = kind == Kind::Resistor ? QStringLiteral("R%1") : kind == Kind::Capacitor ? QStringLiteral("C%1") : QStringLiteral("L%1");
            g->addWidget(pcbLabeled(name.arg(k + 1), in[k], w), 1 + k, 0);
            connect(in[k], &PcbField::edited, this, [this] { recalc(); });
        }
        PcbField *total = makeOutput(QString(), 4);
        total->setUnitChoices(units, 1);
        g->addWidget(pcbLabeled(series ? QStringLiteral("Общее (последовательно)") : QStringLiteral("Общее (параллельно)"), total, w), 0, 1);
        PcbField *volts = nullptr;
        if (kind == Kind::Resistor) {
            volts = makeInput(12.0, QStringLiteral("В"));
            g->addWidget(pcbLabeled(QStringLiteral("Напряжение на цепи"), volts, w), 1, 1);
            for (int k = 0; k < 4; ++k) {
                cur[k] = makeOutput(QStringLiteral("мА"), 4);
                pw[k] = makeOutput(QStringLiteral("мВт"), 4);
                g->addWidget(cur[k], 1 + k, 2);
                g->addWidget(pw[k], 1 + k, 3);
            }
            auto *h1 = new QLabel(QStringLiteral("Ток"), w);
            auto *h2 = new QLabel(QStringLiteral("Мощность"), w);
            h1->setObjectName(QStringLiteral("pcbLabel"));
            h2->setObjectName(QStringLiteral("pcbLabel"));
            g->addWidget(h1, 0, 2);
            g->addWidget(h2, 0, 3);
            connect(volts, &PcbField::edited, this, [this] { recalc(); });
        }
        g->setRowStretch(6, 1);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        m_tabs->addTab(w, title);

        auto update = [=]() {
            const double nan = PcbFmt::kNaN;
            const int n = count->currentIndex() + 2;
            std::vector<double> vals;
            bool ok = true;
            for (int k = 0; k < 4; ++k) {
                in[k]->setEnabled(k < n);
                if (k < n) {
                    if (in[k]->hasValue()) vals.push_back(in[k]->value()); else ok = false;
                }
            }
            double t = nan;
            if (ok) {
                if (kind == Kind::Capacitor)
                    t = series ? pcbcore::parallelSum(vals) : pcbcore::seriesSum(vals);     // у ёмкостей наоборот
                else
                    t = series ? pcbcore::seriesSum(vals) : pcbcore::parallelSum(vals);
            }
            total->setValue(t);
            if (kind == Kind::Resistor) {
                for (int k = 0; k < 4; ++k) {
                    cur[k]->setValue(nan);
                    pw[k]->setValue(nan);
                }
                if (ok && volts->hasValue()) {
                    const auto net = pcbcore::resistorNetwork(vals, series, volts->value());
                    if (net.valid) {
                        for (int k = 0; k < n; ++k) {
                            cur[k]->setValue(net.branches[k].amps * 1e3);
                            pw[k]->setValue(net.branches[k].watts * 1e3);
                        }
                        if (m_tabs->currentWidget() == w) {
                            m_infoTotal = PcbDialogs::engineering(net.totalOhms, QStringLiteral("Ом"));
                            m_infoCurrent = PcbDialogs::engineering(net.totalAmps, QStringLiteral("А"));
                        }
                    }
                }
            } else if (ok && m_tabs->currentWidget() == w) {
                m_infoTotal = PcbDialogs::engineering(t, kind == Kind::Capacitor ? QStringLiteral("Ф") : QStringLiteral("Гн"));
            }
        };
        connect(count, &QComboBox::currentIndexChanged, this, [this](int) { recalc(); });
        m_updaters << update;
        Q_UNUSED(unit0)
    }

    void buildAttenuator(const QString &title, bool pi)
    {
        auto *w = new QWidget(m_tabs);
        auto *g = new QGridLayout(w);
        g->setHorizontalSpacing(12);
        PcbField *db = makeInput(10.0, QStringLiteral("дБ"));
        PcbField *zin = makeInput(50.0, QStringLiteral("Ом"));
        PcbField *zout = makeInput(50.0, QStringLiteral("Ом"));
        PcbField *r1 = makeOutput(QStringLiteral("Ом"), 3);
        PcbField *r2 = makeOutput(QStringLiteral("Ом"), 3);
        PcbField *r3 = makeOutput(QStringLiteral("Ом"), 3);
        auto *note = pcbNote(QString(), w);
        g->addWidget(pcbLabeled(QStringLiteral("Затухание"), db, w), 0, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Zвх"), zin, w), 1, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Zвых"), zout, w), 2, 0);
        g->addWidget(pcbLabeled(pi ? QStringLiteral("R1 (шунт у входа)") : QStringLiteral("R1 (последовательный у входа)"), r1, w), 0, 1);
        g->addWidget(pcbLabeled(pi ? QStringLiteral("R2 (шунт у выхода)") : QStringLiteral("R2 (последовательный у выхода)"), r2, w), 1, 1);
        g->addWidget(pcbLabeled(pi ? QStringLiteral("R3 (последовательный)") : QStringLiteral("R3 (шунт)"), r3, w), 2, 1);
        g->addWidget(note, 3, 0, 1, 2);
        g->setRowStretch(4, 1);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        m_tabs->addTab(w, title);
        auto update = [=]() {
            const double nan = PcbFmt::kNaN;
            note->setText(QString());
            const auto a = pi ? pcbcore::piAttenuator(db->value(), zin->value(), zout->value())
                              : pcbcore::tAttenuator(db->value(), zin->value(), zout->value());
            r1->setValue(a.valid ? a.r1 : nan);
            r2->setValue(a.valid ? a.r2 : nan);
            r3->setValue(a.valid ? a.r3 : nan);
            if (!a.valid && db->hasValue() && zin->hasValue() && zout->hasValue() && db->value() > 0 && zin->value() > 0 && zout->value() > 0)
                note->setText(QStringLiteral("При таких Zвх и Zвых минимальное затухание согласующей цепи — %1 дБ.").arg(PcbFmt::fixed(a.minDb, 2)));
        };
        for (PcbField *f : { db, zin, zout })
            connect(f, &PcbField::edited, this, [update] { update(); });
        m_updaters << update;
    }

    PcbChoice *m_solve;
    PcbField *m_amps, *m_ohms, *m_volts, *m_watts;
    QTabWidget *m_tabs;
    QVector<std::function<void()>> m_updaters;
    QString m_infoTotal = QStringLiteral("Н/Д"), m_infoCurrent = QStringLiteral("Н/Д");
};

} // namespace

PcbPage *makeConversionPage(QWidget *parent) { return new ConversionPage(parent); }
PcbPage *makeOhmsLawPage(QWidget *parent) { return new OhmsLawPage(parent); }
PcbPage *makePpmXtalPage(QWidget *parent) { return new PpmXtalPage(parent); }
PcbPage *makeReactancePage(QWidget *parent) { return new ReactancePage(parent); }
