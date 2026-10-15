#include "widgets/ParamForm.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStringList>
#include <QStyleOptionComboBox>
#include <QStylePainter>

#include <algorithm>
#include <cmath>

#include "util/Units.h"

namespace {

// ------------------------------------------------------------ приставки и единицы

struct Prefix {
    int exp;
    const char *sym;        // латиница
    const char *ru;         // название
    const char *ruShort;    // сокращение
};

const Prefix kPrefixes[] = {
    { -12, "p", "пико",  "п"  },
    { -9,  "n", "нано",  "н"  },
    { -6,  "µ", "микро", "мк" },
    { -3,  "m", "милли", "м"  },
    { 0,   "",  "",      ""   },
    { 3,   "k", "кило",  "к"  },
    { 6,   "M", "мега",  "М"  },
    { 9,   "G", "гига",  "Г"  },
};

struct UnitName {
    const char *unit;
    const char *ru;         // единица по-русски
    const char *ruShort;    // сокращение
};

const UnitName kUnits[] = {
    { "Ω",  "ом",    "Ом" },
    { "F",  "фарад", "Ф"  },
    { "H",  "генри", "Гн" },
    { "V",  "вольт", "В"  },
    { "A",  "ампер", "А"  },
    { "Hz", "герц",  "Гц" },
    { "W",  "ватт",  "Вт" },
    { "s",  "секунда", "с" },
    { "S",  "сименс", "См" },
};

const Prefix *prefixFor(int exp)
{
    for (const Prefix &p : kPrefixes)
        if (p.exp == exp)
            return &p;
    return &kPrefixes[4];
}

const UnitName *unitName(const QString &unit)
{
    for (const UnitName &u : kUnits)
        if (unit == QString::fromUtf8(u.unit))
            return &u;
    return nullptr;
}

bool isPlain(const QString &unit)
{
    return unit.isEmpty() || unit == QLatin1String("%") || unit == QString(QChar(0x00B0));
}

// Какие приставки имеют смысл для единицы.
QVector<int> allowedExps(const QString &unit)
{
    if (unit == QString::fromUtf8("Ω")) return { -3, 0, 3, 6, 9 };
    if (unit == QLatin1String("F"))     return { -12, -9, -6, -3, 0 };
    if (unit == QLatin1String("H"))     return { -9, -6, -3, 0, 3 };
    if (unit == QLatin1String("V"))     return { -6, -3, 0, 3, 6 };
    if (unit == QLatin1String("A"))     return { -6, -3, 0, 3 };
    if (unit == QLatin1String("Hz"))    return { 0, 3, 6, 9 };
    if (unit == QLatin1String("W"))     return { -3, 0, 3, 6 };
    if (unit == QLatin1String("s"))     return { -12, -9, -6, -3, 0 };
    if (unit == QLatin1String("S"))     return { -6, -3, 0 };
    return { -12, -9, -6, -3, 0, 3, 6, 9 };
}

// Подбирает приставку так, чтобы число в поле было «человеческим» (1…999).
int autoExp(double value, const QVector<int> &exps)
{
    if (exps.isEmpty())
        return 0;
    if (value == 0.0 || !std::isfinite(value))
        return exps.contains(0) ? 0 : exps.first();
    int e = int(std::floor(std::log10(std::fabs(value)) / 3.0)) * 3;
    if (std::fabs(value / std::pow(10.0, e)) >= 999.5)
        e += 3;
    int best = exps.first();
    for (int x : exps)
        if (x <= e)
            best = x;                      // наибольшая допустимая, не превышающая e
    return best;
}

QString numberText(double value, int exp)
{
    return QString::number(value / std::pow(10.0, exp), 'g', 6);
}

double clean(double v)                     // убираем хвосты вида 4700.000000000001
{
    return QString::number(v, 'g', 12).toDouble();
}

// Список единиц: в закрытом виде показывает коротко («kΩ»), в раскрытом — с переводом.
class UnitCombo : public QComboBox
{
public:
    using QComboBox::QComboBox;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter p(this);
        QStyleOptionComboBox opt;
        initStyleOption(&opt);
        const QString shortText = itemData(currentIndex(), Qt::UserRole).toString();
        if (!shortText.isEmpty())
            opt.currentText = shortText;
        p.drawComplexControl(QStyle::CC_ComboBox, opt);
        p.drawControl(QStyle::CE_ComboBoxLabel, opt);
    }
};

} // namespace

QString ParamForm::suffixHelp()
{
    return QString::fromUtf8(
        "Приставки (латиница — по-русски):\n"
        "p — пико (10⁻¹²) · n — нано (10⁻⁹) · u или µ — микро (10⁻⁶) · m — милли (10⁻³)\n"
        "k — кило (10³) · M — мега (10⁶) · G — гига (10⁹)\n"
        "Единицы: Ω — Ом, F — фарад, H — генри, V — вольт, A — ампер, Hz — герц, W — ватт, "
        "s — секунда, S — сименс.\n"
        "Единицу выбирают справа: число пересчитается само. Можно писать и так: 4.7k, 100n, 2.2u.");
}

QString ParamForm::suffixHelpShort()
{
    return QString::fromUtf8(
        "Единица — справа списком. Приставки: p пико, n нано, u микро, m милли, k кило, M мега, G гига.");
}

ParamForm::ParamForm(ElementType type, const QMap<QString, double> &values, QWidget *parent)
    : QWidget(parent)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);

    const ElementInfo &info = elementInfo(type);
    int row = 0;
    for (const ParamDef &pd : info.params) {
        Field f;
        f.key = QString::fromUtf8(pd.key);
        f.unit = QString::fromUtf8(pd.unit);
        f.minValue = pd.minValue;
        f.maxValue = pd.maxValue;
        f.defaultValue = pd.defaultValue;
        f.integer = pd.integer;
        f.value = values.value(f.key, pd.defaultValue);

        auto *label = new QLabel(QString::fromUtf8(pd.label), this);
        label->setWordWrap(true);
        grid->addWidget(label, row, 0);

        const int index = m_fields.size();
        if (pd.choices) {
            f.combo = new QComboBox(this);
            f.combo->addItems(QString::fromUtf8(pd.choices).split(QLatin1Char('|')));
            f.combo->setCurrentIndex(std::clamp(int(f.value), 0, std::max(0, f.combo->count() - 1)));
            grid->addWidget(f.combo, row, 1, 1, 2);
            connect(f.combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, index](int i) {
                Field &fld = m_fields[index];
                if (double(i) == fld.value)
                    return;
                fld.value = i;
                emit valueEdited(fld.key, fld.value);
            });
        } else {
            f.edit = new QLineEdit(this);
            f.edit->setToolTip(QString::fromUtf8(pd.label));
            grid->addWidget(f.edit, row, 1);
            connect(f.edit, &QLineEdit::editingFinished, this, [this, index] { commit(index); });

            if (isPlain(f.unit)) {
                if (!f.unit.isEmpty()) {                       // «%», «°» — просто подпись
                    auto *u = new QLabel(f.unit, this);
                    u->setObjectName(QStringLiteral("hint"));
                    u->setMinimumWidth(84);
                    grid->addWidget(u, row, 2);
                }
            } else {
                f.exps = allowedExps(f.unit);
                f.unitBox = new UnitCombo(this);
                f.unitBox->setFixedWidth(84);
                f.unitBox->setToolTip(QStringLiteral("Единица измерения"));
                const UnitName *un = unitName(f.unit);
                for (int e : std::as_const(f.exps)) {
                    const Prefix *pf = prefixFor(e);
                    const QString shortText = QString::fromUtf8(pf->sym) + f.unit;
                    QString full = shortText;
                    if (un)
                        full += QStringLiteral(" — %1%2 (%3%4)")
                                    .arg(QString::fromUtf8(pf->ru), QString::fromUtf8(un->ru),
                                         QString::fromUtf8(pf->ruShort), QString::fromUtf8(un->ruShort));
                    f.unitBox->addItem(full, shortText);
                }
                f.unitBox->view()->setMinimumWidth(250);
                grid->addWidget(f.unitBox, row, 2);
                connect(f.unitBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, index](int i) {
                    Field &fld = m_fields[index];
                    if (i < 0 || i >= fld.exps.size() || fld.exps[i] == fld.exp)
                        return;
                    fld.exp = fld.exps[i];               // значение то же, число в поле пересчитывается
                    fld.shown = numberText(fld.value, fld.exp);
                    fld.edit->setText(fld.shown);
                });
            }
        }
        m_fields.append(f);
        if (m_fields.last().edit)
            showValue(m_fields.last(), true);
        ++row;
    }
}

void ParamForm::showValue(Field &f, bool autoUnit)
{
    if (f.combo) {
        const QSignalBlocker block(f.combo);
        f.combo->setCurrentIndex(std::clamp(int(f.value), 0, std::max(0, f.combo->count() - 1)));
        return;
    }
    if (!f.edit)
        return;
    if (f.unitBox) {
        if (autoUnit)
            f.exp = autoExp(f.value, f.exps);
        const QSignalBlocker block(f.unitBox);
        f.unitBox->setCurrentIndex(std::max(0, int(f.exps.indexOf(f.exp))));
    } else {
        f.exp = 0;
    }
    f.shown = numberText(f.value, f.exp);
    f.edit->setText(f.shown);
}

void ParamForm::commit(int index)
{
    if (index < 0 || index >= m_fields.size())
        return;
    Field &f = m_fields[index];
    if (!f.edit)
        return;
    const QString text = f.edit->text().trimmed();
    if (text == f.shown)
        return;                                       // пользователь ничего не менял

    // Просто число — в выбранной справа единице; с суффиксом (4.7k, 100n) — в СИ,
    // тогда приставку подбираем сами.
    QString t = text;
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    bool plain = false;
    double v = 0;
    bool ok = false;
    const double num = t.toDouble(&ok);
    if (ok && std::isfinite(num)) {
        v = clean(num * std::pow(10.0, f.exp));
        plain = true;
    } else if (!Units::parse(text, f.unit, &v)) {
        showValue(f, false);                          // неверный ввод — откат
        return;
    }

    if (f.integer)
        v = std::round(v);
    v = std::clamp(v, f.minValue, f.maxValue);
    if (v != f.value) {
        f.value = v;
        emit valueEdited(f.key, v);
    }
    showValue(f, !plain);
}

void ParamForm::commitPending()
{
    for (int i = 0; i < m_fields.size(); ++i)
        commit(i);
}

QMap<QString, double> ParamForm::values() const
{
    QMap<QString, double> out;
    for (const Field &f : m_fields)
        out.insert(f.key, f.value);
    return out;
}

void ParamForm::setValues(const QMap<QString, double> &values)
{
    for (Field &f : m_fields) {
        const double nv = values.value(f.key, f.value);
        // Выбранную пользователем единицу не трогаем, пока значение то же самое.
        const bool changed = (nv != f.value);
        f.value = nv;
        showValue(f, changed);
    }
}

void ParamForm::resetToDefaults()
{
    for (Field &f : m_fields) {
        if (f.value == f.defaultValue)
            continue;
        f.value = f.defaultValue;
        showValue(f, true);
        emit valueEdited(f.key, f.value);
    }
}

void ParamForm::focusFirst()
{
    for (Field &f : m_fields) {
        if (f.edit) {
            f.edit->setFocus();
            f.edit->selectAll();
            return;
        }
        if (f.combo) {
            f.combo->setFocus();
            return;
        }
    }
}
