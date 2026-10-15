#include "util/Units.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

const QString &microSign()
{
    static const QString s(QChar(0x00B5));
    return s;
}

// Безразмерные значения и «%», «°»: приставки SI не применяются при выводе.
bool isPlainUnit(const QString &unit)
{
    return unit.isEmpty() || unit == QLatin1String("%") || unit == QString(QChar(0x00B0));
}

bool matchesUnit(const QString &s, const QString &unit)
{
    const QString l = s.toLower();
    if (l == unit.toLower())
        return true;
    if (unit == QString(QChar(0x03A9)))       // Ω
        return l == QLatin1String("ohm") || l == QLatin1String("r")
            || l == QString::fromUtf8("ом");
    if (unit == QLatin1String("F"))  return l == QString::fromUtf8("ф");
    if (unit == QLatin1String("H"))  return l == QString::fromUtf8("гн");
    if (unit == QLatin1String("V"))  return l == QString::fromUtf8("в");
    if (unit == QLatin1String("A"))  return l == QString::fromUtf8("а");
    if (unit == QLatin1String("Hz")) return l == QString::fromUtf8("гц");
    if (unit == QLatin1String("W"))  return l == QString::fromUtf8("вт");
    if (unit == QLatin1String("s"))  return l == QString::fromUtf8("с") || l == QLatin1String("sec");
    if (unit == QLatin1String("S"))  return l == QString::fromUtf8("см");
    return false;
}

bool prefixMultiplier(QChar c, double *mult)
{
    switch (c.unicode()) {
    case 'f':                          *mult = 1e-15; return true;
    case 'p': case 'P':                *mult = 1e-12; return true;
    case 'n': case 'N':                *mult = 1e-9;  return true;
    case 'u': case 'U': case 0x00B5: case 0x03BC:
                                       *mult = 1e-6;  return true;
    case 'm':                          *mult = 1e-3;  return true;
    case 'k': case 'K':                *mult = 1e3;   return true;
    case 'M':                          *mult = 1e6;   return true;
    case 'G': case 'g':                *mult = 1e9;   return true;
    default: return false;
    }
}

} // namespace

namespace Units {

QString format(double value, const QString &unit)
{
    if (isPlainUnit(unit)) {
        const QString num = QString::number(value, 'g', 6);
        return unit.isEmpty() ? num : num + QLatin1Char(' ') + unit;
    }
    if (value == 0.0 || !std::isfinite(value))
        return QStringLiteral("0 ") + unit;

    static const QStringList kPrefixes = {
        QStringLiteral("p"), QStringLiteral("n"), microSign(), QStringLiteral("m"),
        QString(), QStringLiteral("k"), QStringLiteral("M"), QStringLiteral("G")
    };

    int e = static_cast<int>(std::floor(std::log10(std::fabs(value)) / 3.0)) * 3;
    e = std::clamp(e, -12, 9);
    double m = value / std::pow(10.0, e);
    if (std::fabs(m) >= 999.5 && e < 9) {
        e += 3;
        m = value / std::pow(10.0, e);
    }
    return QString::number(m, 'g', 3) + QLatin1Char(' ') + kPrefixes[(e + 12) / 3] + unit;
}

bool parse(const QString &text, const QString &unit, double *out)
{
    QString t = text.trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));

    static const QRegularExpression re(
        QStringLiteral(R"(^([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)\s*(\S*)$)"));
    const QRegularExpressionMatch m = re.match(t);
    if (!m.hasMatch())
        return false;

    bool ok = false;
    const double v = m.captured(1).toDouble(&ok);
    if (!ok)
        return false;

    const QString suffix = m.captured(2);
    double mult = 1.0;

    if (!suffix.isEmpty()) {
        if (unit.isEmpty()) {                      // безразмерное: допустима только приставка (k, M, m…)
            double pm = 1.0;
            if (suffix.toLower() == QLatin1String("meg"))
                pm = 1e6;
            else if (suffix.size() != 1 || !prefixMultiplier(suffix.at(0), &pm))
                return false;
            mult = pm;
        } else if (isPlainUnit(unit)) {
            if (suffix != unit)
                return false;
        } else if (!matchesUnit(suffix, unit)) {
            QString rest;
            double pm = 1.0;
            if (suffix.toLower().startsWith(QLatin1String("meg"))) {
                pm = 1e6;
                rest = suffix.mid(3);
            } else if (prefixMultiplier(suffix.at(0), &pm)) {
                rest = suffix.mid(1);
            } else {
                return false;
            }
            if (!rest.isEmpty() && !matchesUnit(rest, unit))
                return false;
            mult = pm;
        }
    }

    const double result = v * mult;
    if (!std::isfinite(result))
        return false;
    *out = result;
    return true;
}

} // namespace Units
