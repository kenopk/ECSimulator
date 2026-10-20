// Страницы линий передачи: полоса и длина, импеданс, эффективная Er, длина волны, дифференциальные пары.
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QSlider>
#include <QVBoxLayout>

#include "pcb/PcbDialogs.h"
#include "pcb/PcbPagesInternal.h"

namespace {

QVector<PcbUnit> freqUnits()
{
    return { { QStringLiteral("Гц"), 1.0 }, { QStringLiteral("кГц"), 1e3 }, { QStringLiteral("МГц"), 1e6 }, { QStringLiteral("ГГц"), 1e9 } };
}
QVector<PcbUnit> timeUnits()
{
    return { { QStringLiteral("нс"), 1e-9 }, { QStringLiteral("пс"), 1e-12 }, { QStringLiteral("мкс"), 1e-6 } };
}

// Ряд делителей длины волны: полная, 1/2 … 1/20.
void fillLambdaSteps(PcbStepSlider *s, int index)
{
    QVector<double> v;
    QStringList l;
    for (int n = 1; n <= 20; ++n) {
        v << n;
        l << (n == 1 ? QStringLiteral("Полная") : QStringLiteral("1/%1 λ").arg(n));
    }
    s->setSteps(v, l, index);
}

// ================================================================================================
//  Полоса и максимальная длина проводника
// ================================================================================================
class BandwidthPage : public PcbPage
{
public:
    explicit BandwidthPage(QWidget *parent)
        : PcbPage(QStringLiteral("Полоса и макс. длина"), QStringLiteral("Полоса пропускания и максимальная длина проводника"), parent)
    {
        QGridLayout *g = pcbGrid(frame());

        m_method = new PcbChoice(QStringLiteral("Способ ввода"), frame());
        m_method->setOptions({ QStringLiteral("Время нарастания сигнала"), QStringLiteral("Частота") });
        g->addWidget(m_method, 0, 0);

        auto *speed = pcbButton(QStringLiteral("Скорость света"), frame());
        g->addWidget(speed, 0, 1, Qt::AlignTop);
        connect(speed, &QPushButton::clicked, this, [this] { PcbDialogs::speedOfLight(this); });
        g->addWidget(new PcbDiagram(PcbDiagram::RiseTime, frame()), 0, 2, 2, 1);

        m_rise = new PcbField(PcbField::Input, frame());
        m_rise->setUnitChoices(timeUnits(), 0);
        m_rise->setValue(1e-9);
        m_riseBox = pcbLabeled(QStringLiteral("Время нарастания сигнала"), m_rise, frame());
        m_freq = new PcbField(PcbField::Input, frame());
        m_freq->setUnitChoices(freqUnits(), 2);
        m_freq->setValue(350e6);
        m_freqBox = pcbLabeled(QStringLiteral("Частота"), m_freq, frame());
        m_freqBox->hide();
        g->addWidget(m_riseBox, 1, 0);
        g->addWidget(m_freqBox, 1, 0);

        QGridLayout *ipc;
        QGroupBox *ipcGroup = pcbSubGroup(QStringLiteral("Метод IPC-2251"), frame(), &ipc);
        m_bw = makeOutput(QStringLiteral("МГц"), 5);
        m_speed = makeOutput(QStringLiteral("м/с"), 3);
        m_sr = new PcbStepSlider(ipcGroup);
        m_sr->setSteps({ 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25, 0.2, 0.15, 0.1, 0.05 },
                       { QStringLiteral("1 Sr"), QStringLiteral("0,75 Sr"), QStringLiteral("0,5 Sr"), QStringLiteral("0,33 Sr"),
                         QStringLiteral("0,25 Sr"), QStringLiteral("0,2 Sr"), QStringLiteral("0,15 Sr"), QStringLiteral("0,1 Sr"),
                         QStringLiteral("0,05 Sr") }, 4);
        m_len1 = makeOutput(QString(), 5);
        m_len1->setUnits(PcbUnits::lengthCm());
        ipc->addWidget(pcbLabeled(QStringLiteral("Полоса пропускания"), m_bw, ipcGroup), 0, 0);
        ipc->addWidget(pcbLabeled(QStringLiteral("Скорость распространения (c/√Er)"), m_speed, ipcGroup), 1, 0);
        ipc->addWidget(pcbLabeled(QStringLiteral("Делитель длины фронта Sr"), m_sr, ipcGroup), 2, 0);
        ipc->addWidget(pcbLabeled(QStringLiteral("Максимальная длина проводника"), m_len1, ipcGroup), 3, 0);
        ipc->setRowStretch(4, 1);

        QGridLayout *fd;
        QGroupBox *fdGroup = pcbSubGroup(QStringLiteral("Частотный метод"), frame(), &fd);
        m_lamAir = makeOutput(QString(), 5);
        m_lamAir->setUnits(PcbUnits::lengthCm());
        m_lam = new PcbStepSlider(fdGroup);
        fillLambdaSteps(m_lam, 6);
        m_len2 = makeOutput(QString(), 5);
        m_len2->setUnits(PcbUnits::lengthCm());
        fd->addWidget(pcbLabeled(QStringLiteral("Полная длина волны (в воздухе)"), m_lamAir, fdGroup), 0, 0);
        fd->addWidget(pcbLabeled(QStringLiteral("Делитель длины волны"), m_lam, fdGroup), 1, 0);
        fd->addWidget(pcbLabeled(QStringLiteral("Максимальная длина проводника"), m_len2, fdGroup), 2, 0);
        fd->setRowStretch(3, 1);

        g->addWidget(ipcGroup, 2, 0, 1, 2);
        g->addWidget(fdGroup, 2, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(2, 1);
        g->setRowStretch(3, 1);

        m_passive = new PcbChoice(QStringLiteral("Пассивные цепи"), this);
        m_passive->setOptions({ QStringLiteral("Микрополосок"), QStringLiteral("Полосковая линия") });
        m_passive->hide();

        connect(m_method, &PcbChoice::currentChanged, this, [this](int i) {
            m_riseBox->setVisible(i == 0);
            m_freqBox->setVisible(i == 1);
            m_passive->setEnabled(i == 0);
            recalc();
        });
        connect(m_passive, &PcbChoice::currentChanged, this, [this](int) { recalc(); });
        connect(m_sr, &PcbStepSlider::changed, this, [this] { recalc(); });
        connect(m_lam, &PcbStepSlider::changed, this, [this] { recalc(); });
        connect(m_rise, &PcbField::edited, this, [this] { recalc(); });
        connect(m_freq, &PcbField::edited, this, [this] { recalc(); });
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.er = true; c.units = true; return c; }
    QWidget *optionsWidget() override { return m_passive; }

    bool usesTrace() const override { return true; }
    void setTrace(const PcbTrace &t) override { m_trace = t; recalc(); }
    QStringList infoItems() const override
    {
        QStringList l;
        if (m_trace.valid && m_ipcLenMm > 0) {
            const bool ok = m_trace.lengthMm <= m_ipcLenMm;
            l << PcbFmt::info(QStringLiteral("Трасса L = %1 мм").arg(PcbFmt::fixed(m_trace.lengthMm, 2)),
                              ok ? QStringLiteral("короче предела — можно не согласовывать")
                                 : QStringLiteral("длиннее предела — это линия передачи"), !ok);
        }
        return l;
    }
    QString traceSummary() const override
    {
        if (!(m_ipcLenMm > 0))
            return QString();
        QString s = QStringLiteral("Граничная длина: IPC-2251 — %1 мм, по λ — %2 мм").arg(PcbFmt::fixed(m_ipcLenMm, 1), PcbFmt::fixed(m_freqLenMm, 1));
        if (m_trace.valid)
            s += QStringLiteral("; трасса %1 мм — %2").arg(PcbFmt::fixed(m_trace.lengthMm, 1),
                                                       m_trace.lengthMm <= m_ipcLenMm ? QStringLiteral("в пределе") : QStringLiteral("длиннее предела"));
        return s;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_len1, m_lamAir, m_len2 })
            f->setUnitMode(context().unitMode());
    }

    void solve() override
    {
        double tr = PcbFmt::kNaN;
        if (m_method->currentIndex() == 0)
            tr = m_rise->value();
        else if (m_freq->hasValue() && m_freq->value() > 0)
            tr = 0.35 / m_freq->value();
        if (!(tr > 0)) {
            for (PcbField *f : { m_bw, m_speed, m_len1, m_lamAir, m_len2 })
                f->setValue(PcbFmt::kNaN);
            return;
        }
        const auto r = pcbcore::bandwidthAndLength(tr, context().er, m_passive->currentIndex() == 0, m_sr->value(), m_lam->value());
        m_ipcLenMm = r.ipcLengthMm;
        m_freqLenMm = r.freqLengthMm;
        m_bw->setValue(r.bandwidthHz / 1e6);
        m_speed->setValue(r.speed);
        m_len1->setValue(r.ipcLengthMm);
        m_lamAir->setValue(r.wavelengthAirM * 1000.0);
        m_len2->setValue(r.freqLengthMm);
    }

private:
    PcbChoice *m_method, *m_passive;
    PcbField *m_rise, *m_freq, *m_bw, *m_speed, *m_len1, *m_lamAir, *m_len2;
    QWidget *m_riseBox, *m_freqBox;
    PcbStepSlider *m_sr, *m_lam;
    PcbTrace m_trace;
    double m_ipcLenMm = 0, m_freqLenMm = 0;
};

// ================================================================================================
//  Импеданс проводника
// ================================================================================================
class ImpedancePage : public PcbPage
{
public:
    explicit ImpedancePage(QWidget *parent)
        : PcbPage(QStringLiteral("Импеданс проводника"), QStringLiteral("Импеданс проводника"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_w = makeInput(0.4318);
        m_h = makeInput(0.254);
        m_f = new PcbField(PcbField::Input, frame());
        m_f->setUnit(QStringLiteral("МГц"));
        m_f->setValue(500.0);
        connect(m_f, &PcbField::edited, this, [this] { recalc(); });
        m_extra = makeInput(0.254);
        for (PcbField *f : { m_w, m_h, m_extra })
            f->setUnits(PcbUnits::lengthMm());

        m_hLabel = new QLabel(QStringLiteral("Высота проводника над плоскостью (H)"), frame());
        m_hLabel->setObjectName(QStringLiteral("pcbLabel"));
        m_extraLabel = new QLabel(frame());
        m_extraLabel->setObjectName(QStringLiteral("pcbLabel"));
        auto *wl = new QLabel(QStringLiteral("Ширина проводника (W)"), frame());
        wl->setObjectName(QStringLiteral("pcbLabel"));
        auto *fl = new QLabel(QStringLiteral("Частота"), frame());
        fl->setObjectName(QStringLiteral("pcbLabel"));
        m_fLabel = fl;

        g->addWidget(wl, 0, 0);
        g->addWidget(m_w, 1, 0);
        g->addWidget(m_hLabel, 2, 0);
        g->addWidget(m_h, 3, 0);
        g->addWidget(fl, 4, 0);
        g->addWidget(m_f, 5, 0);
        g->addWidget(m_extraLabel, 6, 0);
        g->addWidget(m_extra, 7, 0);

        m_diagram = new PcbDiagram(PcbDiagram::Microstrip, frame());
        m_diagram->setMinimumHeight(170);
        g->addWidget(m_diagram, 0, 1, 8, 1);

        m_zo = makeOutput(QStringLiteral("Ом"), 4);
        m_lo = makeOutput(QStringLiteral("нГн/см"), 4);
        m_co = makeOutput(QStringLiteral("пФ/см"), 4);
        m_tpd = makeOutput(QStringLiteral("пс/см"), 4);
        auto *outs = new QGridLayout;
        outs->setHorizontalSpacing(12);
        outs->addWidget(pcbLabeled(QStringLiteral("Zo — волновое сопротивление"), m_zo, frame()), 0, 0);
        outs->addWidget(pcbLabeled(QStringLiteral("Lo — погонная индуктивность"), m_lo, frame()), 0, 1);
        outs->addWidget(pcbLabeled(QStringLiteral("Co — погонная ёмкость"), m_co, frame()), 1, 0);
        outs->addWidget(pcbLabeled(QStringLiteral("Tpd — задержка"), m_tpd, frame()), 1, 1);
        g->addLayout(outs, 8, 0, 1, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(9, 1);

        m_type = new PcbChoice(QStringLiteral("Пассивные цепи"), this);
        m_type->setOptions({ QStringLiteral("Микрополосок"), QStringLiteral("Микрополосок погружённый"),
                             QStringLiteral("Полосковая линия"), QStringLiteral("Полосковая асимметричная"),
                             QStringLiteral("Двойная полосковая"), QStringLiteral("Копланарный волновод") });
        m_type->hide();
        connect(m_type, &PcbChoice::currentChanged, this, [this](int) { updateTypeUi(); recalc(); });
        updateTypeUi();
    }

    PcbPageConfig config() const override
    {
        PcbPageConfig c;
        c.base = c.plating = c.er = c.units = true;
        return c;
    }
    QWidget *optionsWidget() override { return m_type; }

    bool usesTrace() const override { return true; }
    void setTrace(const PcbTrace &t) override
    {
        m_trace = t;
        if (t.valid && t.widthMm > 0)
            m_w->setValue(t.widthMm);
        recalc();
    }
    QString traceSummary() const override
    {
        if (!m_valid)
            return QString();
        static const char *const kNames[] = { "микрополосок", "микрополосок погружённый", "полосковая линия", "полосковая асимметричная",
                                              "двойная полосковая", "копланарный волновод" };
        QString s = QStringLiteral("Zo = %1 Ом (%2), W = %3 мм, H = %4 мм, Er = %5")
                        .arg(PcbFmt::fixed(m_zoOhm, 1), QString::fromUtf8(kNames[std::clamp(m_type->currentIndex(), 0, 5)]),
                             PcbFmt::fixed(m_w->value(), 3), PcbFmt::fixed(m_h->value(), 3), PcbFmt::trimmed(context().er));
        if (m_trace.valid)
            s += QStringLiteral("; задержка по трассе %1 мм — %2 пс").arg(PcbFmt::fixed(m_trace.lengthMm, 1),
                                                                    PcbFmt::fixed(m_tpdSPerM * m_trace.lengthMm * 1e-3 * 1e12, 1));
        return s;
    }

    QStringList infoItems() const override
    {
        QStringList l;
        if (m_valid && m_trace.valid)
            l << PcbFmt::info(QStringLiteral("Задержка трассы %1 мм").arg(PcbFmt::fixed(m_trace.lengthMm, 1)),
                              PcbFmt::fixed(m_tpdSPerM * m_trace.lengthMm * 1e-3 * 1e12, 2) + QStringLiteral(" пс"));
        if (m_valid)
            l << PcbFmt::info(QStringLiteral("Эффективная Er"), PcbFmt::fixed(m_eeff, 4));
        else
            l << PcbFmt::info(QStringLiteral("Расчёт"), QStringLiteral("параметры вне области формулы"), true);
        return l;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_w, m_h, m_extra })
            f->setUnitMode(context().unitMode());
        const bool imp = context().imperial;
        m_lo->setUnit(imp ? QStringLiteral("нГн/дюйм") : QStringLiteral("нГн/см"));
        m_co->setUnit(imp ? QStringLiteral("пФ/дюйм") : QStringLiteral("пФ/см"));
        m_tpd->setUnit(imp ? QStringLiteral("пс/дюйм") : QStringLiteral("пс/см"));
    }

    void solve() override
    {
        pcbcore::LineInputs in;
        in.type = static_cast<pcbcore::LineType>(m_type->currentIndex());
        in.widthMm = m_w->value();
        in.heightMm = m_h->value();
        in.extraMm = m_extra->value();
        in.thicknessMm = context().totalCuMm();
        in.er = context().er;
        in.freqHz = m_f->hasValue() ? m_f->value() * 1e6 : 0.0;
        const pcbcore::LineResult r = pcbcore::solveLine(in);
        m_valid = r.valid;
        const double perLen = context().imperial ? 0.0254 : 0.01;           // метров в единице длины (дюйм или см)
        if (!r.valid) {
            for (PcbField *f : { m_zo, m_lo, m_co, m_tpd })
                f->setValue(PcbFmt::kNaN);
            return;
        }
        m_eeff = r.eeff;
        m_zoOhm = r.z0;
        m_tpdSPerM = r.tpd;
        m_zo->setValue(r.z0);
        m_lo->setValue(r.lo * perLen * 1e9);
        m_co->setValue(r.co * perLen * 1e12);
        m_tpd->setValue(r.tpd * perLen * 1e12);
    }

private:
    void updateTypeUi()
    {
        const int t = m_type->currentIndex();
        static const PcbDiagram::Kind kinds[] = { PcbDiagram::Microstrip, PcbDiagram::MicrostripEmbed, PcbDiagram::Stripline,
                                                  PcbDiagram::StriplineAsym, PcbDiagram::DualStripline, PcbDiagram::Coplanar };
        m_diagram->setKind(kinds[std::clamp(t, 0, 5)]);
        QString extra;
        switch (t) {
        case 1: extra = QStringLiteral("Слой диэлектрика над проводником (B)"); break;
        case 3: extra = QStringLiteral("Расстояние до дальней плоскости (H2)"); break;
        case 4: extra = QStringLiteral("Расстояние между сигнальными слоями (D)"); break;
        case 5: extra = QStringLiteral("Зазор до копланарной земли (S)"); break;
        }
        m_extraLabel->setText(extra);
        m_extraLabel->setVisible(!extra.isEmpty());
        m_extra->setVisible(!extra.isEmpty());
        m_f->setEnabled(t == 0);
        m_fLabel->setEnabled(t == 0);
        m_hLabel->setText(t == 5 ? QStringLiteral("Высота над нижней плоскостью (H)")
                                 : (t >= 2 && t <= 4 ? QStringLiteral("Расстояние до плоскости (H)")
                                                     : QStringLiteral("Высота проводника над плоскостью (H)")));
    }

    PcbChoice *m_type;
    PcbDiagram *m_diagram;
    PcbField *m_w, *m_h, *m_f, *m_extra, *m_zo, *m_lo, *m_co, *m_tpd;
    QLabel *m_hLabel, *m_extraLabel, *m_fLabel;
    bool m_valid = false;
    double m_eeff = 1.0, m_zoOhm = 0, m_tpdSPerM = 0;
    PcbTrace m_trace;
};

// ================================================================================================
//  Эффективная Er
// ================================================================================================
class ErEffectivePage : public PcbPage
{
public:
    explicit ErEffectivePage(QWidget *parent)
        : PcbPage(QStringLiteral("Эффективная Er"), QStringLiteral("Эффективная диэлектрическая проницаемость"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_w = makeInput(0.4318);
        m_h = makeInput(0.254);
        m_f = new PcbField(PcbField::Input, frame());
        m_f->setUnit(QStringLiteral("МГц"));
        m_f->setValue(500.0);
        connect(m_f, &PcbField::edited, this, [this] { recalc(); });
        for (PcbField *f : { m_w, m_h })
            f->setUnits(PcbUnits::lengthMm());
        g->addWidget(pcbLabeled(QStringLiteral("Ширина проводника (W)"), m_w, frame()), 0, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Высота проводника над плоскостью (H)"), m_h, frame()), 1, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Частота"), m_f, frame()), 2, 0);
        auto *info = pcbButton(QStringLiteral("Об эффективной Er"), frame());
        connect(info, &QPushButton::clicked, this, [this] { PcbDialogs::erEffectiveInfo(this); });
        g->addWidget(info, 0, 1, Qt::AlignTop | Qt::AlignRight);
        g->addWidget(pcbNote(QStringLiteral("Расчёт идёт по сложной модели Hammerstad–Jensen с поправкой на толщину проводника, "
                                            "а не по упрощённой формуле IPC-2141A."), frame()), 3, 0, 1, 2);
        auto *d = new PcbDiagram(PcbDiagram::Microstrip, frame());
        d->setMinimumHeight(170);
        g->addWidget(d, 4, 0, 1, 2);
        m_out = makeOutput(QString(), 4);
        auto *send = pcbButton(QStringLiteral("Отправить в калькулятор длины волны"), frame());
        connect(send, &QPushButton::clicked, this, [this] {
            if (std::isfinite(m_eeff))
                emit sendValue(PcbPageId::Wavelength, QStringLiteral("er"), m_eeff);
        });
        auto *bottom = new QVBoxLayout;
        bottom->addWidget(pcbLabeled(QStringLiteral("Эффективная Er"), m_out, frame()));
        bottom->addWidget(send);
        g->addLayout(bottom, 5, 0, 1, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setRowStretch(6, 1);
    }

    PcbPageConfig config() const override
    {
        PcbPageConfig c;
        c.base = c.plating = c.er = c.units = true;
        return c;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_w, m_h })
            f->setUnitMode(context().unitMode());
    }

    void solve() override
    {
        m_eeff = pcbcore::microstripEeff(m_w->value(), m_h->value(), context().totalCuMm(), context().er,
                                         m_f->hasValue() ? m_f->value() * 1e6 : 0.0);
        m_out->setValue(m_eeff);
    }

private:
    PcbField *m_w, *m_h, *m_f, *m_out;
    double m_eeff = PcbFmt::kNaN;
};

// ================================================================================================
//  Длина волны
// ================================================================================================
class WavelengthPage : public PcbPage
{
public:
    explicit WavelengthPage(QWidget *parent)
        : PcbPage(QStringLiteral("Длина волны"), QStringLiteral("Длина волны"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_method = new PcbChoice(QStringLiteral("Способ ввода"), frame());
        m_method->setOptions({ QStringLiteral("Период"), QStringLiteral("Частота") });
        m_method->setMinimumWidth(150);
        g->addWidget(m_method, 0, 0, 2, 1);

        auto *b1 = pcbButton(QStringLiteral("О длине волны"), frame());
        auto *b2 = pcbButton(QStringLiteral("Об эффективной Er"), frame());
        auto *b3 = pcbButton(QStringLiteral("Скорость света"), frame());
        connect(b1, &QPushButton::clicked, this, [this] { PcbDialogs::wavelengthInfo(this); });
        connect(b2, &QPushButton::clicked, this, [this] { PcbDialogs::erEffectiveInfo(this); });
        connect(b3, &QPushButton::clicked, this, [this] { PcbDialogs::speedOfLight(this); });
        auto *bl = new QVBoxLayout;
        bl->addWidget(b1);
        bl->addWidget(b2);
        bl->addWidget(b3);
        g->addLayout(bl, 0, 1, 2, 1);
        g->addWidget(new PcbDiagram(PcbDiagram::Period, frame()), 0, 2, 3, 1);

        m_period = new PcbField(PcbField::Input, frame());
        m_period->setUnitChoices(timeUnits(), 0);
        m_period->setValue(10e-9);
        m_periodBox = pcbLabeled(QStringLiteral("Период"), m_period, frame());
        m_freq = new PcbField(PcbField::Input, frame());
        m_freq->setUnitChoices(freqUnits(), 2);
        m_freq->setValue(100e6);
        m_freqBox = pcbLabeled(QStringLiteral("Частота"), m_freq, frame());
        m_freqBox->hide();
        g->addWidget(m_periodBox, 2, 0, 1, 2);
        g->addWidget(m_freqBox, 2, 0, 1, 2);

        m_er = makeInput(4.0);
        g->addWidget(pcbLabeled(QStringLiteral("Эффективная Er"), m_er, frame()), 3, 0, 1, 2);
        auto *calc = pcbButton(QStringLiteral("Калькулятор Er эфф."), frame());
        connect(calc, &QPushButton::clicked, this, [this] { emit sendValue(PcbPageId::ErEffective, QString(), 0.0); });
        g->addWidget(calc, 4, 0, 1, 2);

        g->addWidget(pcbFormula(QStringLiteral("λ = c / ( f · √Er<sub>eff</sub> )"), frame()), 5, 0, 1, 2);
        m_div = new PcbStepSlider(frame());
        fillLambdaSteps(m_div, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Делитель длины волны"), m_div, frame()), 6, 0, 1, 3);
        m_out = makeOutput(QString(), 5);
        m_out->setUnits(PcbUnits::lengthCm());
        g->addWidget(pcbLabeled(QStringLiteral("Длина волны"), m_out, frame()), 7, 0, 1, 2);
        g->addWidget(pcbNote(QStringLiteral("Для длины волны в воздухе введите Er эфф. = 1."), frame()), 8, 0, 1, 3);
        g->setColumnStretch(2, 1);
        g->setRowStretch(9, 1);

        connect(m_method, &PcbChoice::currentChanged, this, [this](int i) {
            m_periodBox->setVisible(i == 0);
            m_freqBox->setVisible(i == 1);
            recalc();
        });
        connect(m_period, &PcbField::edited, this, [this] { recalc(); });
        connect(m_freq, &PcbField::edited, this, [this] { recalc(); });
        connect(m_div, &PcbStepSlider::changed, this, [this] { recalc(); });
    }

    PcbPageConfig config() const override { PcbPageConfig c; c.units = true; return c; }

    void contextChanged() override { m_out->setUnitMode(context().unitMode()); }

    void receiveValue(const QString &key, double value) override
    {
        if (key == QLatin1String("er")) {
            m_er->setValue(value);
            recalc();
        }
    }

    void solve() override
    {
        double f = PcbFmt::kNaN;
        if (m_method->currentIndex() == 0)
            f = m_period->value() > 0 ? 1.0 / m_period->value() : PcbFmt::kNaN;
        else
            f = m_freq->value();
        const double lam = pcbcore::wavelengthM(f, m_er->value());
        m_out->setValue(std::isfinite(lam) ? lam * 1000.0 / m_div->value() : PcbFmt::kNaN);
    }

private:
    PcbChoice *m_method;
    PcbField *m_period, *m_freq, *m_er, *m_out;
    QWidget *m_periodBox, *m_freqBox;
    PcbStepSlider *m_div;
};

// ================================================================================================
//  Дифференциальные пары и перекрёстные помехи
// ================================================================================================
class DiffPairsPage : public PcbPage
{
public:
    explicit DiffPairsPage(QWidget *parent)
        : PcbPage(QStringLiteral("Дифф. пары / XTALK"), QStringLiteral("Дифференциальные пары"), parent)
    {
        QGridLayout *g = pcbGrid(frame());
        m_w = makeInput(0.254);
        m_s = makeInput(0.127);
        m_h = makeInput(0.381);
        m_extra = makeInput(0.381);
        m_len = makeInput(2.54);
        for (PcbField *f : { m_w, m_s, m_h, m_extra, m_len })
            f->setUnits(PcbUnits::lengthMm());
        m_v = makeInput(1.0, QStringLiteral("В"));
        m_tr = makeInput(1e-9);
        m_tr->setUnitChoices(timeUnits(), 0);
        m_tr->setValue(1e-9);

        g->addWidget(pcbLabeled(QStringLiteral("Ширина проводника (W)"), m_w, frame()), 0, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Зазор между проводниками (S)"), m_s, frame()), 1, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Высота над плоскостью (H)"), m_h, frame()), 2, 0);
        m_extraLabelBox = pcbLabeled(QStringLiteral("Дополнительный размер"), m_extra, frame());
        g->addWidget(m_extraLabelBox, 3, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Приложенное напряжение"), m_v, frame()), 4, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Длина связи"), m_len, frame()), 5, 0);
        g->addWidget(pcbLabeled(QStringLiteral("Время нарастания сигнала"), m_tr, frame()), 6, 0);

        m_target = makeInput(100.0, QStringLiteral("Ом"));
        g->addWidget(pcbLabeled(QStringLiteral("Целевое Zdiff"), m_target, frame()), 0, 1);
        m_tolLabel = new QLabel(frame());
        m_tolLabel->setObjectName(QStringLiteral("pcbLabel"));
        m_tol = new QSlider(Qt::Horizontal, frame());
        m_tol->setRange(1, 30);
        m_tol->setValue(10);
        auto *tolBox = new QVBoxLayout;
        tolBox->setSpacing(3);
        tolBox->addWidget(m_tolLabel);
        tolBox->addWidget(m_tol);
        g->addLayout(tolBox, 1, 1);
        m_plus = makeOutput(QStringLiteral("Ом"), 3);
        m_minus = makeOutput(QStringLiteral("Ом"), 3);
        g->addWidget(pcbLabeled(QStringLiteral("Целевое Zdiff плюс"), m_plus, frame()), 2, 1);
        g->addWidget(pcbLabeled(QStringLiteral("Целевое Zdiff минус"), m_minus, frame()), 3, 1);
        g->addWidget(pcbNote(QStringLiteral("Формулы верны при 0,1 < W/H < 3,0 и 0,1 < S/H < 3,0."), frame()), 4, 1, 2, 1);
        auto *help = pcbButton(QStringLiteral("Справка"), frame());
        connect(help, &QPushButton::clicked, this, [this] { PcbDialogs::differentialInfo(this); });
        g->addWidget(help, 6, 1, Qt::AlignLeft | Qt::AlignBottom);

        m_protocol = new QComboBox(frame());
        for (const auto &p : pcbcore::diffProtocols())
            m_protocol->addItem(QString::fromUtf8(p.name));
        m_protocol->setCurrentIndex(1);
        g->addWidget(pcbLabeled(QStringLiteral("Протокол"), m_protocol, frame()), 0, 2);
        auto *toVia = pcbButton(QStringLiteral("Отправить в калькулятор переходных"), frame());
        connect(toVia, &QPushButton::clicked, this, [this] {
            if (m_target->hasValue())
                emit sendValue(PcbPageId::Via, QStringLiteral("targetZdiff"), m_target->value());
        });
        g->addWidget(toVia, 1, 2);

        m_zdiff = makeOutput(QStringLiteral("Ом"), 3);
        m_zo = makeOutput(QStringLiteral("Ом"), 3);
        m_zodd = makeOutput(QStringLiteral("Ом"), 3);
        m_zeven = makeOutput(QStringLiteral("Ом"), 3);
        m_led = new QLabel(frame());
        m_led->setFixedSize(16, 16);
        auto *zrow = new QHBoxLayout;
        zrow->addWidget(m_led);
        zrow->addWidget(m_zdiff, 1);
        auto *zbox = new QWidget(frame());
        auto *zl = new QVBoxLayout(zbox);
        zl->setContentsMargins(0, 0, 0, 0);
        zl->setSpacing(3);
        auto *zlab = new QLabel(QStringLiteral("Zdiff — дифференциальное"), zbox);
        zlab->setObjectName(QStringLiteral("pcbLabel"));
        zl->addWidget(zlab);
        zl->addLayout(zrow);
        g->addWidget(zbox, 2, 2);
        g->addWidget(pcbLabeled(QStringLiteral("Zo — одиночный проводник"), m_zo, frame()), 3, 2);
        g->addWidget(pcbLabeled(QStringLiteral("Zodd — нечётная мода"), m_zodd, frame()), 4, 2);
        g->addWidget(pcbLabeled(QStringLiteral("Zeven — чётная мода"), m_zeven, frame()), 5, 2);

        m_diagram = new PcbDiagram(PcbDiagram::DiffPair, frame());
        m_diagram->setMinimumHeight(150);
        g->addWidget(m_diagram, 7, 0, 1, 2);

        QGridLayout *xt;
        QGroupBox *xg = pcbSubGroup(QStringLiteral("Перекрёстные помехи (NEXT)"), frame(), &xt);
        m_kb = makeOutput(QString(), 4);
        m_kbp = makeOutput(QString(), 4);
        m_kbDb = makeOutput(QStringLiteral("дБ"), 3);
        m_kbpDb = makeOutput(QStringLiteral("дБ"), 3);
        m_next = makeOutput(QStringLiteral("В"), 4);
        m_nextp = makeOutput(QStringLiteral("В"), 4);
        xt->addWidget(pcbLabeled(QStringLiteral("Kb (согласованная)"), m_kb, xg), 0, 0);
        xt->addWidget(pcbLabeled(QStringLiteral("Kb′ (несогласованная)"), m_kbp, xg), 0, 1);
        xt->addWidget(m_kbDb, 1, 0);
        xt->addWidget(m_kbpDb, 1, 1);
        xt->addWidget(pcbLabeled(QStringLiteral("NEXT, В"), m_next, xg), 2, 0);
        xt->addWidget(pcbLabeled(QStringLiteral("NEXT, В"), m_nextp, xg), 2, 1);
        g->addWidget(xg, 7, 2);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        g->setColumnStretch(2, 1);
        g->setRowStretch(8, 1);

        m_layer = new PcbChoice(QStringLiteral("Слой пары"), this);
        m_layer->setOptions({ QStringLiteral("Боковая связь, наружный"), QStringLiteral("Боковая связь, внутр. симм."),
                              QStringLiteral("Боковая связь, внутр. асимм."), QStringLiteral("Боковая связь, погружённый"),
                              QStringLiteral("Broadside, с экраном"), QStringLiteral("Broadside, без экрана") });
        m_layer->hide();

        connect(m_layer, &PcbChoice::currentChanged, this, [this](int) { updateLayerUi(); recalc(); });
        connect(m_protocol, &QComboBox::currentIndexChanged, this, [this](int i) {
            const double z = pcbcore::diffProtocols()[std::clamp(i, 0, int(pcbcore::diffProtocols().size()) - 1)].zdiff;
            if (z > 0) {
                m_target->setValue(z);
                recalc();
            }
        });
        connect(m_target, &PcbField::edited, this, [this] {
            QSignalBlocker b(m_protocol);
            m_protocol->setCurrentIndex(0);
            recalc();
        });
        connect(m_tol, &QSlider::valueChanged, this, [this](int) { recalc(); });
        updateLayerUi();
    }

    PcbPageConfig config() const override
    {
        PcbPageConfig c;
        c.base = c.plating = c.er = c.units = true;
        return c;
    }
    QWidget *optionsWidget() override { return m_layer; }

    bool usesTrace() const override { return true; }
    void setTrace(const PcbTrace &t) override
    {
        m_trace = t;
        if (t.valid) {
            m_len->setValue(t.lengthMm);                        // длина связи пары — длина трассы
            if (t.widthMm > 0)
                m_w->setValue(t.widthMm);
        }
        recalc();
    }
    QString traceSummary() const override
    {
        if (!m_res.valid)
            return QString();
        QString s = QStringLiteral("Дифф. пара%1: Zdiff = %2 Ом (цель %3 ±%4 %), Zo = %5 Ом, W = %6 мм, S = %7 мм, H = %8 мм; NEXT %9 В")
                        .arg(m_trace.valid && m_trace.pair > 0 ? QStringLiteral(" №%1").arg(m_trace.pair) : QString(),
                             PcbFmt::fixed(m_res.zdiff, 1), PcbFmt::trimmed(m_target->value()), QString::number(m_tol->value()),
                             PcbFmt::fixed(m_res.zo, 1), PcbFmt::fixed(m_w->value(), 3), PcbFmt::fixed(m_s->value(), 3),
                             PcbFmt::fixed(m_h->value(), 3), PcbFmt::fixed(m_res.nextUnterm, 4));
        if (m_trace.valid && m_trace.pair > 0 && m_trace.pairSize >= 2)
            s += m_trace.skewMm == 0 ? QStringLiteral("; длины проводников равны")
                                     : QStringLiteral("; рассогласование длин %1 мм").arg(PcbFmt::fixed(std::fabs(m_trace.skewMm), 2));
        return s;
    }

    QStringList infoItems() const override
    {
        QStringList l;
        if (m_trace.valid && m_trace.pair > 0) {
            const QString title = QStringLiteral("Пара №%1 на схеме").arg(m_trace.pair);
            if (m_trace.pairSize < 2)
                l << PcbFmt::info(title, QStringLiteral("второго проводника нет"), true);
            else if (m_trace.skewMm == 0)
                l << PcbFmt::info(title, QStringLiteral("длины проводников равны"));
            else
                l << PcbFmt::info(title, QStringLiteral("этот проводник %1 на %2 мм").arg(m_trace.skewMm < 0 ? QStringLiteral("короче") : QStringLiteral("длиннее"),
                                                                                         PcbFmt::fixed(std::fabs(m_trace.skewMm), 2)), true);
        }
        l << PcbFmt::info(QStringLiteral("W/H"), PcbFmt::fixed(m_res.wh, 3), !m_res.inRange && m_res.valid && (m_res.wh <= 0.1 || m_res.wh >= 3.0));
        l << PcbFmt::info(QStringLiteral("S/H"), PcbFmt::fixed(m_res.sh, 3), !m_res.inRange && m_res.valid && (m_res.sh <= 0.1 || m_res.sh >= 3.0));
        l << PcbFmt::info(QStringLiteral("Lsat (длина насыщения)"), PcbFmt::fixed(m_res.lsatMm, 2) + QStringLiteral(" мм"));
        if (m_res.valid && !m_res.inRange)
            l << PcbFmt::info(QStringLiteral("Внимание"), QStringLiteral("данные вне области формулы"), true);
        return l;
    }

    void contextChanged() override
    {
        for (PcbField *f : { m_w, m_s, m_h, m_extra, m_len })
            f->setUnitMode(context().unitMode());
    }

    void solve() override
    {
        pcbcore::DiffInputs in;
        in.layer = static_cast<pcbcore::DiffLayer>(m_layer->currentIndex());
        in.widthMm = m_w->value();
        in.spacingMm = m_s->value();
        in.heightMm = m_h->value();
        in.extraMm = m_extra->value();
        in.thicknessMm = context().totalCuMm();
        in.er = context().er;
        in.appliedV = m_v->hasValue() ? m_v->value() : 0.0;
        in.coupledLenMm = m_len->hasValue() ? m_len->value() : 0.0;
        in.riseTimeS = m_tr->hasValue() ? m_tr->value() : 1e-9;
        m_res = pcbcore::solveDiffPair(in);

        const double tol = m_tol->value();
        m_tolLabel->setText(QStringLiteral("Допуск ± %1 %").arg(tol));
        const double target = m_target->value();
        m_plus->setValue(std::isfinite(target) ? target * (1 + tol / 100.0) : PcbFmt::kNaN);
        m_minus->setValue(std::isfinite(target) ? target * (1 - tol / 100.0) : PcbFmt::kNaN);

        const double nan = PcbFmt::kNaN;
        m_zdiff->setValue(m_res.valid ? m_res.zdiff : nan);
        m_zo->setValue(m_res.valid ? m_res.zo : nan);
        m_zodd->setValue(m_res.valid ? m_res.zodd : nan);
        m_zeven->setValue(m_res.valid ? m_res.zeven : nan);
        m_kb->setValue(m_res.valid ? m_res.kbTerm : nan);
        m_kbp->setValue(m_res.valid ? m_res.kbUnterm : nan);
        m_kbDb->setValue(m_res.valid ? m_res.kbTermDb : nan);
        m_kbpDb->setValue(m_res.valid ? m_res.kbUntermDb : nan);
        m_next->setValue(m_res.valid ? m_res.nextTerm : nan);
        m_nextp->setValue(m_res.valid ? m_res.nextUnterm : nan);

        const bool ok = m_res.valid && std::isfinite(target) && std::fabs(m_res.zdiff - target) <= target * tol / 100.0;
        m_led->setStyleSheet(QStringLiteral("background:%1; border-radius:3px;").arg(!m_res.valid ? QStringLiteral("#6b7280")
                                                                                       : ok ? QStringLiteral("#22c55e") : QStringLiteral("#ef4444")));
    }

    void receiveValue(const QString &, double) override {}

private:
    void updateLayerUi()
    {
        const int t = m_layer->currentIndex();
        static const PcbDiagram::Kind kinds[] = { PcbDiagram::DiffPair, PcbDiagram::DiffPair, PcbDiagram::DiffPair,
                                                  PcbDiagram::DiffPair, PcbDiagram::DiffPair, PcbDiagram::DiffPair };
        m_diagram->setKind(kinds[std::clamp(t, 0, 5)]);
        QString label;
        if (t == 2) label = QStringLiteral("Расстояние до дальней плоскости (H2)");
        else if (t == 3) label = QStringLiteral("Слой диэлектрика над проводниками (B)");
        m_extraLabelBox->setVisible(!label.isEmpty());
        if (auto *l = m_extraLabelBox->findChild<QLabel *>())
            l->setText(label);
    }

    PcbChoice *m_layer;
    PcbDiagram *m_diagram;
    PcbField *m_w, *m_s, *m_h, *m_extra, *m_len, *m_v, *m_tr, *m_target, *m_plus, *m_minus;
    PcbField *m_zdiff, *m_zo, *m_zodd, *m_zeven, *m_kb, *m_kbp, *m_kbDb, *m_kbpDb, *m_next, *m_nextp;
    QWidget *m_extraLabelBox;
    QLabel *m_tolLabel, *m_led;
    QSlider *m_tol;
    QComboBox *m_protocol;
    pcbcore::DiffResult m_res;
    PcbTrace m_trace;
};

} // namespace

PcbPage *makeBandwidthPage(QWidget *parent) { return new BandwidthPage(parent); }
PcbPage *makeImpedancePage(QWidget *parent) { return new ImpedancePage(parent); }
PcbPage *makeErEffectivePage(QWidget *parent) { return new ErEffectivePage(parent); }
PcbPage *makeWavelengthPage(QWidget *parent) { return new WavelengthPage(parent); }
PcbPage *makeDiffPairsPage(QWidget *parent) { return new DiffPairsPage(parent); }
