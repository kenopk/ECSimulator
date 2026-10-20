#include "pcb/PcbDialogs.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QIcon>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <vector>

#include "pcb/PcbCore.h"
#include "pcb/PcbWidgets.h"
#include "theme/Theme.h"

namespace PcbDialogs {

namespace {

QDialog *makeDialog(QWidget *parent, const QString &title, QSize size)
{
    auto *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(title);
    dlg->resize(size);
    new QVBoxLayout(dlg);
    return dlg;
}

QTableWidget *makeTable(QDialog *dlg, const QStringList &headers)
{
    auto *t = new QTableWidget(dlg);
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setAlternatingRowColors(false);
    return t;
}

void setCell(QTableWidget *t, int row, int col, const QString &text, bool right = false)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment((right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    t->setItem(row, col, item);
}

void addClose(QDialog *dlg)
{
    auto *box = new QDialogButtonBox(dlg);
    QPushButton *close = box->addButton(QStringLiteral("Закрыть"), QDialogButtonBox::RejectRole);
    QObject::connect(close, &QPushButton::clicked, dlg, &QDialog::close);
    dlg->layout()->addWidget(box);
}

} // namespace

QString engineering(double value, const QString &unit, int significant)
{
    if (!std::isfinite(value))
        return QStringLiteral("—");
    if (value == 0.0)
        return QStringLiteral("0 ") + unit;
    static const struct { int exp; const char *name; } kPrefix[] = {
        { 12, "Т" }, { 9, "Г" }, { 6, "М" }, { 3, "к" }, { 0, "" }, { -3, "м" }, { -6, "мк" }, { -9, "н" }, { -12, "п" }, { -15, "ф" } };
    const int e = int(std::floor(std::log10(std::fabs(value)) / 3.0)) * 3;
    int pick = 4;
    for (int i = 0; i < 10; ++i)
        if (kPrefix[i].exp == std::clamp(e, -15, 12)) { pick = i; break; }
    const double scaled = value / std::pow(10.0, kPrefix[pick].exp);
    QString num = QString::number(scaled, 'g', significant);
    num.replace(QLatin1Char('.'), QLocale::system().decimalPoint());
    return num + QLatin1Char(' ') + QString::fromUtf8(kPrefix[pick].name) + unit;
}

void info(QWidget *parent, const QString &title, const QString &html)
{
    QDialog *dlg = makeDialog(parent, title, QSize(560, 420));
    auto *view = new QTextBrowser(dlg);
    view->setOpenExternalLinks(false);
    view->setHtml(html);
    dlg->layout()->addWidget(view);
    addClose(dlg);
    dlg->show();
}

// ------------------------------------------------------------------------------------------------
void siPrefixes(QWidget *parent)
{
    QDialog *dlg = makeDialog(parent, QStringLiteral("Приставки СИ"), QSize(460, 420));
    auto *t = makeTable(dlg, { QStringLiteral("Приставка"), QStringLiteral("Обозначение"), QStringLiteral("Множитель") });
    static const struct { const char *name; const char *sym; const char *mult; } kRows[] = {
        { "тера", "Т", "10¹²" }, { "гига", "Г", "10⁹" }, { "мега", "М", "10⁶" }, { "кило", "к", "10³" },
        { "—", "—", "1" }, { "милли", "м", "10⁻³" }, { "микро", "мк (µ)", "10⁻⁶" }, { "нано", "н", "10⁻⁹" },
        { "пико", "п", "10⁻¹²" }, { "фемто", "ф", "10⁻¹⁵" } };
    t->setRowCount(10);
    for (int i = 0; i < 10; ++i) {
        setCell(t, i, 0, QString::fromUtf8(kRows[i].name));
        setCell(t, i, 1, QString::fromUtf8(kRows[i].sym));
        setCell(t, i, 2, QString::fromUtf8(kRows[i].mult), true);
    }
    dlg->layout()->addWidget(t);
    addClose(dlg);
    dlg->show();
}

void dbmChart(QWidget *parent)
{
    QDialog *dlg = makeDialog(parent, QStringLiteral("Таблица дБм"), QSize(520, 560));
    auto *t = makeTable(dlg, { QStringLiteral("дБм"), QStringLiteral("Мощность"), QStringLiteral("Напряжение на 50 Ом (RMS)") });
    QVector<int> rows;
    for (int d = -100; d <= 50; d += 10)
        rows << d;
    rows << -3 << 3;
    std::sort(rows.begin(), rows.end());
    t->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const double w = pcbcore::dbmToWatts(rows[i]);
        setCell(t, i, 0, QString::number(rows[i]), true);
        setCell(t, i, 1, engineering(w, QStringLiteral("Вт")), true);
        setCell(t, i, 2, engineering(std::sqrt(w * 50.0), QStringLiteral("В")), true);
    }
    dlg->layout()->addWidget(t);
    addClose(dlg);
    dlg->show();
}

void capacitorChart(QWidget *parent)
{
    QDialog *dlg = makeDialog(parent, QStringLiteral("Конденсаторы: мкФ — нФ — пФ"), QSize(560, 560));
    auto *t = makeTable(dlg, { QStringLiteral("пФ"), QStringLiteral("нФ"), QStringLiteral("мкФ"), QStringLiteral("Код") });
    static const double kPf[] = { 1, 4.7, 10, 22, 47, 100, 220, 470, 1000, 2200, 4700, 10000, 22000, 47000, 100000,
                                  220000, 470000, 1e6, 2.2e6, 4.7e6, 1e7, 1e8 };
    const int n = int(sizeof(kPf) / sizeof(kPf[0]));
    t->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const double pf = kPf[i];
        setCell(t, i, 0, PcbFmt::trimmed(pf), true);
        setCell(t, i, 1, PcbFmt::trimmed(pf / 1e3), true);
        setCell(t, i, 2, PcbFmt::trimmed(pf / 1e6), true);
        QString code = QStringLiteral("—");
        if (pf >= 10) {                                    // трёхзначный код: две цифры и множитель
            const int exp = int(std::floor(std::log10(pf))) - 1;
            const int digits = int(std::lround(pf / std::pow(10.0, exp)));
            code = QString::number(digits) + QString::number(exp);
        }
        setCell(t, i, 3, code, true);
    }
    dlg->layout()->addWidget(t);
    auto *note = new QLabel(QStringLiteral("Код на корпусе: две значащие цифры и число нулей, в пикофарадах (104 = 100 000 пФ = 100 нФ)."), dlg);
    note->setWordWrap(true);
    dlg->layout()->addWidget(note);
    addClose(dlg);
    dlg->show();
}

void resistorCalculator(QWidget *parent)
{
    QDialog *dlg = makeDialog(parent, QStringLiteral("Цветовой код резистора"), QSize(420, 340));
    struct Color { const char *name; QColor rgb; int digit; double mult; double tol; };
    static const Color kColors[] = {
        { "Чёрный",    QColor("#111111"), 0,  1.0,    0.0 },
        { "Коричневый", QColor("#7a4a21"), 1,  10.0,   1.0 },
        { "Красный",   QColor("#d33a2c"), 2,  100.0,  2.0 },
        { "Оранжевый", QColor("#f08a24"), 3,  1e3,    0.0 },
        { "Жёлтый",    QColor("#f2d338"), 4,  1e4,    0.0 },
        { "Зелёный",   QColor("#3aa655"), 5,  1e5,    0.5 },
        { "Синий",     QColor("#2f6fe0"), 6,  1e6,    0.25 },
        { "Фиолетовый", QColor("#8a4fd6"), 7,  1e7,    0.1 },
        { "Серый",     QColor("#8a8f98"), 8,  1e8,    0.05 },
        { "Белый",     QColor("#f4f4f4"), 9,  1e9,    0.0 },
        { "Золотой",   QColor("#c9a227"), -1, 0.1,    5.0 },
        { "Серебряный", QColor("#b8bcc4"), -1, 0.01,   10.0 },
    };
    const int nColors = int(sizeof(kColors) / sizeof(kColors[0]));
    auto swatch = [](const QColor &c) {
        QPixmap pm(18, 12);
        pm.fill(c);
        return QIcon(pm);
    };

    auto *form = new QFormLayout;
    auto *bands = new QComboBox(dlg);
    bands->addItems({ QStringLiteral("4 полосы"), QStringLiteral("5 полос") });
    form->addRow(QStringLiteral("Число полос:"), bands);
    QComboBox *digits[3];
    for (int i = 0; i < 3; ++i) {
        digits[i] = new QComboBox(dlg);
        for (int k = 0; k < nColors; ++k)
            if (kColors[k].digit >= 0)
                digits[i]->addItem(swatch(kColors[k].rgb), QString::fromUtf8(kColors[k].name) + QStringLiteral(" (%1)").arg(kColors[k].digit), k);
        form->addRow(QStringLiteral("Полоса %1:").arg(i + 1), digits[i]);
    }
    auto *mult = new QComboBox(dlg);
    auto *tol = new QComboBox(dlg);
    for (int k = 0; k < nColors; ++k) {
        mult->addItem(swatch(kColors[k].rgb), QString::fromUtf8(kColors[k].name), k);
        if (kColors[k].tol > 0)
            tol->addItem(swatch(kColors[k].rgb), QString::fromUtf8(kColors[k].name) + QStringLiteral(" (±%1 %)").arg(PcbFmt::trimmed(kColors[k].tol)), k);
    }
    form->addRow(QStringLiteral("Множитель:"), mult);
    form->addRow(QStringLiteral("Допуск:"), tol);
    auto *result = new QLabel(dlg);
    result->setObjectName(QStringLiteral("pcbResult"));
    QFont f = result->font();
    f.setPointSize(16);
    f.setBold(true);
    result->setFont(f);
    dlg->layout()->addItem(form);
    dlg->layout()->addWidget(result);

    auto update = [=] {
        const bool five = bands->currentIndex() == 1;
        digits[2]->setEnabled(five);
        std::vector<int> d;
        d.push_back(kColors[digits[0]->currentData().toInt()].digit);
        d.push_back(kColors[digits[1]->currentData().toInt()].digit);
        if (five)
            d.push_back(kColors[digits[2]->currentData().toInt()].digit);
        double v = 0;
        for (int x : d)
            v = v * 10 + x;
        v *= kColors[mult->currentData().toInt()].mult;
        const double t = kColors[tol->currentData().toInt()].tol;
        result->setText(QStringLiteral("%1,  ±%2 %").arg(engineering(v, QStringLiteral("Ом")), PcbFmt::trimmed(t)));
    };
    for (QComboBox *c : { bands, digits[0], digits[1], digits[2], mult, tol })
        QObject::connect(c, &QComboBox::currentIndexChanged, dlg, update);
    digits[0]->setCurrentIndex(1);      // коричневый
    digits[1]->setCurrentIndex(0);      // чёрный
    digits[2]->setCurrentIndex(0);
    mult->setCurrentIndex(2);           // красный → ×100
    tol->setCurrentIndex(3);            // золотой → ±5 %
    update();
    addClose(dlg);
    dlg->show();
}

void designRules(QWidget *parent)
{
    info(parent, QStringLiteral("Правила проектирования печатных плат"), QStringLiteral(R"(
<h3>Ориентиры для проекта</h3>
<p>Это типовые рекомендации, а не нормативы. Требования изготовителя платы и стандарты (IPC-2221, IPC-2152, IPC-2222)
важнее; перед запуском в производство сверьтесь с технологическими нормами завода.</p>
<ul>
<li><b>Ширина и зазор.</b> Не меньше технологического минимума изготовителя; для обычных плат типично 0,10–0,15 мм (4–6 мил).</li>
<li><b>Ток.</b> Ширину под ток выбирайте по странице «Свойства проводника» (IPC-2152): важны толщина меди, допустимый нагрев и наличие плоскости.</li>
<li><b>Зазоры по напряжению.</b> См. страницу «Мин. зазор проводников» (IPC-2221): зависят от напряжения, высоты над уровнем моря и покрытия.</li>
<li><b>Переходные.</b> Отношение высоты к диаметру отверстия для сквозных обычно не более 8:1…10:1, для микропереходов — около 1:1.</li>
<li><b>Кольцо вокруг отверстия.</b> Чем больше, тем надёжнее; на внешних слоях типично от 0,125 мм (5 мил).</li>
<li><b>Правило 3W.</b> Расстояние между центрами проводников не меньше трёх их ширин — меньше наводки между линиями.</li>
<li><b>Линии передачи.</b> Проводник длиннее примерно 1/4…1/7 длины фронта сигнала считайте линией и согласуйте (страница «Полоса и макс. длина»).</li>
<li><b>Дифференциальные пары.</b> Одинаковая длина плеч, постоянный зазор, без лишних переходных; расстояние до соседей — не меньше удвоенного зазора пары.</li>
<li><b>Ответвления и «пеньки».</b> Длина ответвления не больше λ/20 на частоте сигнала.</li>
<li><b>Опорные плоскости.</b> Не разрезайте плоскость под быстрыми линиями; обратный ток должен идти рядом с прямым.</li>
<li><b>Развязка питания.</b> Конденсаторы ближе к выводам питания, малые индуктивности подключения (страница PDN).</li>
<li><b>Нагрев.</b> Тепловые переходные под корпусом, медь вокруг; расчёт — страница «Тепловой режим».</li>
</ul>)"));
}

void spacingTable(QWidget *parent)
{
    QDialog *dlg = makeDialog(parent, QStringLiteral("Зазоры между проводниками (IPC-2221, табл. 6-1), мм"), QSize(820, 520));
    QStringList heads { QStringLiteral("Напряжение") };
    for (const auto &d : pcbcore::spacingDevices())
        heads << QString::fromUtf8(d.id);
    auto *t = makeTable(dlg, heads);
    t->setRowCount(10);
    const auto &classes = pcbcore::spacingVoltageClasses();
    for (int r = 0; r < 10; ++r) {
        setCell(t, r, 0, QString::fromUtf8(classes[r]));
        for (int c = 0; c < 8; ++c) {
            const QString text = r < 9 ? PcbFmt::trimmed(pcbcore::spacingTableMm(r, c))
                                       : QStringLiteral("+%1 мм/В").arg(PcbFmt::trimmed(pcbcore::spacingPerVoltMm(c)));
            setCell(t, r, c + 1, text, true);
        }
    }
    dlg->layout()->addWidget(t);
    QString legend;
    for (const auto &d : pcbcore::spacingDevices())
        legend += QStringLiteral("<b>%1</b> — %2<br>").arg(QString::fromUtf8(d.id), QString::fromUtf8(d.text));
    legend += QStringLiteral("<br>Для напряжений выше 500 В к значению в строке «301 – 500 В» прибавляется указанная величина на каждый вольт сверх 500 В. "
                             "Значения справочные — сверяйте с действующей редакцией стандарта.");
    auto *lab = new QLabel(legend, dlg);
    lab->setWordWrap(true);
    lab->setTextFormat(Qt::RichText);
    dlg->layout()->addWidget(lab);
    addClose(dlg);
    dlg->show();
}

// ------------------------------------------------------------------------------------------------
void speedOfLight(QWidget *parent)
{
    info(parent, QStringLiteral("Скорость света"), QStringLiteral(R"(
<h3>Скорость света</h3>
<p>В вакууме c = 299 792 458 м/с ≈ 29,98 см/нс ≈ 11,80 дюйм/нс.</p>
<p>В диэлектрике сигнал идёт медленнее: v = c / √ε, где ε — проницаемость, которую «видит» поле (для микрополоска — эффективная Er).
Задержка на единицу длины — обратная величина: t<sub>pd</sub> = √ε / c.</p>
<p>Для FR-4 (Er ≈ 4,6) в однородной среде v ≈ 14 см/нс, то есть ≈ 71 пс/см.</p>)"));
}

void wavelengthInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Длина волны"), QStringLiteral(R"(
<h3>Длина волны в линии</h3>
<p>λ = c / (f · √Er<sub>eff</sub>), где f — частота сигнала, Er<sub>eff</sub> — эффективная диэлектрическая проницаемость.
Для длины волны в воздухе берите Er<sub>eff</sub> = 1.</p>
<p>Делитель длины волны показывает, какая часть λ получается: проводник короче λ/10…λ/20 обычно можно не считать линией передачи,
а ответвления (stub) ограничивают значениями λ/20 и меньше.</p>
<p>Эффективная проницаемость микрополоска всегда меньше Er подложки — её даёт страница «Эффективная Er».</p>)"));
}

void erEffectiveInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Эффективная проницаемость"), QStringLiteral(R"(
<h3>Эффективная Er</h3>
<p>Поле микрополосковой линии проходит частично по подложке, частично по воздуху, поэтому для расчётов нужна не Er подложки,
а эффективное значение, зависящее от отношения ширины проводника к высоте над плоскостью.</p>
<p>Здесь использована модель Hammerstad–Jensen с поправкой на толщину проводника и дисперсией Kobayashi — она точнее
упрощённой формулы IPC-2141A, особенно у широких проводников и на высоких частотах.</p>)"));
}

void onderdonkInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Множитель Ондердонка"), QStringLiteral(R"(
<h3>Уравнение Ондердонка</h3>
<p>I = A · √( lg(1 + (T<sub>пл</sub> − T<sub>окр</sub>) / (234 + T<sub>окр</sub>)) / (33 · t) ), где A — сечение в круговых милах,
t — время в секундах, T<sub>пл</sub> = 1084,62 °C — температура плавления меди.</p>
<p>Уравнение создано для голого медного провода в воздухе. На печатной плате проводник отдаёт тепло в подложку, поэтому
значения получаются приблизительными; формула применима для коротких импульсов (до ~5 с). Поправочный множитель позволяет
подогнать результат под ваши измерения: 1 — без поправки.</p>)"));
}

void wireInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Токи в проводах"), QStringLiteral(R"(
<h3>Допустимый ток провода</h3>
<p><b>Для монтажа в шасси</b> — значения из распространённой таблицы допустимых токов проводов (AWG) при монтаже внутри аппаратуры.</p>
<p><b>Для передачи мощности</b> — по правилу «700 круговых милов на ампер»: I = A<sub>cmil</sub> / 700; консервативная оценка для длинных линий.</p>
<p>Реальный допустимый ток зависит от изоляции, пучка проводов, вентиляции и температуры окружающей среды.</p>)"));
}

void differentialInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Дифференциальные пары и перекрёстные помехи"), QStringLiteral(R"(
<h3>Дифференциальный импеданс</h3>
<p>Для внешней пары: Z<sub>diff</sub> = 2·Z<sub>0</sub>·(1 − 0,48·e<sup>−0,96·S/H</sup>); для внутренней: Z<sub>diff</sub> = 2·Z<sub>0</sub>·(1 − 0,347·e<sup>−2,9·S/B</sup>)
(IPC-2141). Формулы справедливы при 0,1 &lt; W/H &lt; 3,0 и 0,1 &lt; S/H &lt; 3,0, вне диапазона даётся предупреждение.</p>
<p>Z<sub>odd</sub> = Z<sub>diff</sub>/2, Z<sub>even</sub> = Z<sub>0</sub>²/Z<sub>odd</sub>.</p>
<h3>Перекрёстные помехи (NEXT)</h3>
<p>K<sub>b</sub> = (Z<sub>even</sub> − Z<sub>0</sub>)/(Z<sub>even</sub> + Z<sub>0</sub>) — для согласованной линии, K<sub>b</sub>′ = (Z<sub>even</sub> − Z<sub>odd</sub>)/(Z<sub>even</sub> + Z<sub>odd</sub>) — для несогласованной.
Напряжение наводки растёт с длиной связи до длины насыщения L<sub>sat</sub> = t<sub>r</sub>·v/2 и дальше не меняется.</p>
<p>Типы «Edge Cpld» — пары с боковой связью, «Broad Cpld» — друг над другом (broadside); для последних даётся оценочная модель.</p>)"));
}

void conductorInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Свойства проводника"), QStringLiteral(R"(
<h3>Ток и нагрев проводника</h3>
<p><b>IPC-2221</b>: I = k·ΔT<sup>0,44</sup>·A<sup>0,725</sup>, A — сечение в мил², k = 0,048 (наружный слой), 0,024 (внутренний).</p>
<p><b>IPC-2152</b>: та же зависимость с коэффициентом, подогнанным под кривые стандарта; «с поправками» добавляет упрощённый учёт
толщины платы, соседней плоскости меди и параллельных проводников. Это инженерная аппроксимация, а не сами графики IPC.</p>
<p>«Ток проводника» — ток, при котором проводник нагреется на заданный прирост температуры, а не предельный ток до разрушения.
Если окружающая температура + прирост выше Tg материала, выводится предупреждение.</p>
<p>Сечение учитывает травление: при 2:1 верх проводника уже низа на половину толщины, при 1:1 — на толщину.
Сопротивление считается для меди при 85 °C.</p>)"));
}

void xtalInfo(QWidget *parent)
{
    info(parent, QStringLiteral("Кварцевый резонатор"), QStringLiteral(R"(
<h3>Нагрузочные конденсаторы</h3>
<p>C<sub>нагр</sub> = C1·C2/(C1 + C2) + C<sub>пар</sub>. Если C1 = C2, то C = 2·(C<sub>нагр</sub> − C<sub>пар</sub>) — первое приближение.
Итог проверяйте по «С учётом C1–C2» — оно должно совпасть с нагрузочной ёмкостью из даташита.</p>
<h3>PPM</h3>
<p>PPM = (f<sub>макс</sub> − f<sub>центр</sub>)/f<sub>центр</sub> · 10⁶. Обратно: Δf = f<sub>центр</sub>·PPM·10⁻⁶.</p>)"));
}

} // namespace PcbDialogs
