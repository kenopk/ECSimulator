#include "items/CircuitItem.h"
#include "tools/KarnaughSolver.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "canvas/CircuitScene.h"
#include "items/SymbolPainter.h"
#include "theme/Theme.h"
#include "util/Units.h"

namespace {

constexpr qreal kMargin = 40.0;          // запас bounding rect под тело/ореол/маркеры
constexpr qreal kFlowSpacing = 14.0;     // шаг точек тока
constexpr qreal kRedLead = 12.0;         // длина красного «концевого» участка

// Подпись элемента: номиналы — моноширинным шрифтом, пользовательская «Надпись» — шрифтом интерфейса.
QFont labelFont(int px, bool mono)
{
    return mono ? Theme::monoFont(px) : Theme::uiFont(px);
}

} // namespace

CircuitItem::CircuitItem(ElementType type)
    : m_type(type)
{
    m_uid = ++s_nextUid;
    for (const auto &pd : elementInfo(type).params)
        m_params.insert(QString::fromUtf8(pd.key), pd.defaultValue);
    m_variant = computeVariant();
    if (type == ElementType::KarnaughMap) {
        m_cells.fill(0, kKarnaughStride * kKarnaughStride);
        recomputeKarnaugh();
    }

    m_p2 = SymbolPainter::normalizeEnd(m_type, m_p1, m_p1, m_variant);       // длина/направление по умолчанию

    setFlag(ItemIsSelectable, true);
    setAcceptedMouseButtons(Qt::NoButton);   // весь ввод обрабатывает CircuitView
    setZValue(isWire() || isDecoration() ? 0 : 1);

    m_label = computeLabel();
    updateLabelRect();
}

int CircuitItem::pinCount() const
{
    return isWire() ? 2 : SymbolPainter::pinCount(m_type, m_variant);
}

unsigned CircuitItem::computeVariant() const
{
    const ElementInfo &info = elementInfo(m_type);
    if (info.dynamic == govno::core::Dynamic::None)
        return 0;
    return govno::core::variantOf(info, [this](const char *key) {
        return m_params.value(QString::fromUtf8(key));
    });
}

// Параметры могли поменять число и расположение выводов (входы вентиля, объём памяти, размер матрицы…).
void CircuitItem::refreshVariant()
{
    const unsigned v = computeVariant();
    if (v == m_variant)
        return;
    // Если элемент стоял с длиной «по умолчанию» (её никто не растягивал), то при смене раскладки выводов
    // (число входов, размер матрицы, объём памяти) длина тоже становится «по умолчанию» для новой раскладки —
    // иначе после уменьшения у элемента остаются длинные выводы от прежнего размера.
    const qreal oldLen = length();
    const bool wasDefault = std::fabs(oldLen - SymbolPainter::defaultLength(m_type, m_variant)) < 1.0;

    prepareGeometryChange();
    m_variant = v;
    m_shapeDirty = true;

    QPoint target = m_p2;
    if (wasDefault && oldLen > 0.0 && !isSinglePin()) {
        const qreal newLen = SymbolPainter::defaultLength(m_type, m_variant);
        const QPointF dir = QPointF(m_p2 - m_p1) / oldLen;
        target = m_p1 + QPoint(qRound(dir.x() * newLen), qRound(dir.y() * newLen));
    }
    m_p2 = SymbolPainter::normalizeEnd(m_type, m_p1, target, m_variant);
    updateLabelRect();
    notifyScene();
    update();
}

bool CircuitItem::isLightEmitter(ElementType type)
{
    switch (type) {
    case ElementType::LED:
    case ElementType::Lamp:
    case ElementType::NeonLamp:
    case ElementType::LogicOutput:
    case ElementType::SegDisplay:
    case ElementType::LedMatrix:
    case ElementType::LedRGB:
        return true;
    default:
        return false;
    }
}

QColor CircuitItem::glowColor() const
{
    if (m_type == ElementType::LedRGB) {                // цвет — смесь трёх каналов по их яркости
        const double r = m_light.value(0), g = m_light.value(1), b = m_light.value(2);
        const auto ch = [&](double fr, double fg, double fb) {
            return std::clamp(int(std::lround(fr * r + fg * g + fb * b)), 0, 255);
        };
        return QColor(ch(255, 60, 70), ch(70, 255, 120), ch(60, 100, 255));
    }
    const auto it = m_params.constFind(QStringLiteral("glow"));
    if (it == m_params.constEnd())
        return QColor("#e5483d");
    return QColor::fromRgb(quint32(std::max(0.0, it.value())) & 0xFFFFFF);
}

void CircuitItem::setLight(const QVector<double> &levels)
{
    if (levels == m_light)
        return;
    m_light = levels;
    update();
}

void CircuitItem::setReadout(const QString &text)
{
    if (text == m_readout)
        return;
    m_readout = text;
    m_label = computeLabel();                           // у приборов показание заменяет подпись
    updateLabelRect();
    update();
}

void CircuitItem::clearLight()
{
    if (m_light.isEmpty())
        return;
    m_light.clear();
    update();
}

void CircuitItem::setProblem(quint64 pinMask, bool wholeBody)
{
    if (pinMask == m_problemMask && wholeBody == m_problemBody)
        return;
    m_problemMask = pinMask;
    m_problemBody = wholeBody;
    update();
}

void CircuitItem::clearProblem()
{
    setProblem(0, false);
}

CircuitItem::~CircuitItem()
{
    if (m_probe)
        m_probe->targetGone();
}

ScopeSource *CircuitItem::probe(bool create)
{
    if (!m_probe && create)
        m_probe = new ScopeSource(this, scene());
    return m_probe;
}

QString CircuitItem::displayName() const { return QString::fromUtf8(elementInfo(m_type).name); }

namespace {

// Параметр-переключатель: перечисление "state" или "position" с двумя и более вариантами
// (выключатели, кнопки, переключатели, реле-контакты, логический вход).
const ParamDef *toggleParam(ElementType type)
{
    for (const ParamDef &pd : elementInfo(type).params) {
        const QByteArray key(pd.key);
        if ((key == "state" || key == "position") && pd.choices)
            return &pd;
    }
    return nullptr;
}

int choiceCount(const ParamDef &pd)
{
    return QString::fromUtf8(pd.choices).split(QLatin1Char('|')).size();
}

} // namespace

bool CircuitItem::isToggle() const
{
    const ParamDef *pd = toggleParam(m_type);
    return pd && choiceCount(*pd) >= 2;
}

QString CircuitItem::toggleKey() const
{
    const ParamDef *pd = toggleParam(m_type);
    return pd ? QString::fromUtf8(pd->key) : QStringLiteral("state");
}

double CircuitItem::toggledValue() const
{
    const ParamDef *pd = toggleParam(m_type);
    if (!pd)
        return 0.0;
    const int n = std::max(2, choiceCount(*pd));
    return double((int(param(QString::fromUtf8(pd->key))) + 1) % n);       // следующий вариант по кругу
}

bool CircuitItem::isDecoration() const
{
    return m_type == ElementType::TextLabel || m_type == ElementType::Box || m_type == ElementType::Line;
}

void CircuitItem::setText(const QString &t)
{
    if (t == m_text)
        return;
    prepareGeometryChange();
    m_text = t;
    m_shapeDirty = true;
    m_label = computeLabel();
    updateLabelRect();
    update();
}

bool CircuitItem::hasParts() const
{
    return m_type == ElementType::Keypad || m_type == ElementType::DipSwitch || m_type == ElementType::KarnaughMap;
}

void CircuitItem::setCells(const QVector<quint8> &cells)
{
    if (!isKarnaugh() || cells.size() != kKarnaughStride * kKarnaughStride || cells == m_cells)
        return;
    m_cells = cells;
    recomputeKarnaugh();
    update();
}

// Формула по выбранным клеткам: параметр «Что выводить» — СДНФ, СКНФ, минимальная ДНФ или КНФ.
void CircuitItem::recomputeKarnaugh()
{
    if (!isKarnaugh())
        return;
    if (m_cells.size() != kKarnaughStride * kKarnaughStride)
        m_cells.fill(0, kKarnaughStride * kKarnaughStride);
    Karnaugh::Input in;
    in.width = std::clamp(int(std::lround(param(QStringLiteral("width")))), 1, kKarnaughStride);
    in.height = std::clamp(int(std::lround(param(QStringLiteral("height")))), 1, kKarnaughStride);
    in.cells.resize(in.width * in.height);
    for (int r = 0; r < in.height; ++r)
        for (int c = 0; c < in.width; ++c)
            in.cells[r * in.width + c] = m_cells[r * kKarnaughStride + c];
    const Karnaugh::Result res = Karnaugh::solve(in);
    switch (int(std::lround(param(QStringLiteral("output"))))) {
    case 0:  m_formula = res.sdnf; break;
    case 1:  m_formula = res.sknf; break;
    case 3:  m_formula = res.mknf; break;
    default: m_formula = res.mdnf; break;
    }
    if (m_formula.isEmpty())
        m_formula = res.note.isEmpty() ? QStringLiteral("0") : QStringLiteral("…");
    update();
}

int CircuitItem::partAt(const QPointF &scenePos) const
{
    if (!hasParts())
        return 0;
    const qreal len = length();
    if (len <= 0.0)
        return 0;
    const QPointF u((m_p2.x() - m_p1.x()) / len, (m_p2.y() - m_p1.y()) / len);
    const QPointF n(-u.y(), u.x());
    const QPointF d = scenePos - QPointF(m_p1);
    const qreal x = d.x() * u.x() + d.y() * u.y();
    const qreal y = d.x() * n.x() + d.y() * n.y();
    return SymbolPainter::partAt(m_type, m_variant, len, QPointF(x, m_flip ? -y : y));
}

bool CircuitItem::hasProperties() const
{
    if (isWire())
        return false;
    const auto &ps = elementInfo(m_type).params;
    if (ps.empty())
        return false;
    return !(isToggle() && ps.size() == 1);
}

namespace {

// Главная характеристика, которую крутит колесо (primary) и вторичная (Shift + колесо).
struct WheelEntry
{
    const char *id;
    const char *primary;
    const char *secondary;
};

const WheelEntry kWheel[] = {
    { "resistor", "resistance", nullptr },
    { "resistor_var", "position", "resistance" },
    { "potentiometer", "position", "resistance" },
    { "capacitor", "capacitance", nullptr },
    { "capacitor_polar", "capacitance", nullptr },
    { "capacitor_var", "position", "capacitance" },
    { "inductor", "inductance", nullptr },
    { "choke", "inductance", nullptr },
    { "inductor_var", "position", "inductance" },
    { "fuse", "current", nullptr },
    { "lamp", "voltage", nullptr },
    { "vsource_dc", "voltage", nullptr },
    { "battery", "voltage", nullptr },
    { "rail_dc", "voltage", nullptr },
    { "rail_var", "position", "vmax" },
    { "vsource_ac", "amplitude", "frequency" },
    { "rail_ac", "amplitude", "frequency" },
    { "rail_square", "amplitude", "frequency" },
    { "rail_clock", "frequency", "vhigh" },
    { "generator", "amplitude", "frequency" },
    { "antenna", "amplitude", "frequency" },
    { "microphone", "amplitude", "frequency" },
    { "isource", "current", nullptr },
    { "zener", "zenerVoltage", nullptr },
    { "regulator", "vout", nullptr },
    { "tl431", "vref", nullptr },
    { "thermistor", "r25", nullptr },
    { "photoresistor", "illum", nullptr },
    { "photodiode", "illum", nullptr },
    { "phototransistor", "illum", nullptr },
    { "crystal", "frequency", nullptr },
    { "speaker", "impedance", nullptr },
    { "monostable", "pulse", nullptr },
    { "delay_line", "delay", nullptr },
};

const WheelEntry *wheelEntry(ElementType type)
{
    const char *id = elementInfo(type).id;
    for (const WheelEntry &e : kWheel)
        if (std::strcmp(e.id, id) == 0)
            return &e;
    return nullptr;
}

const ParamDef *findParam(ElementType type, const QString &key)
{
    for (const ParamDef &pd : elementInfo(type).params)
        if (key == QLatin1String(pd.key))
            return &pd;
    return nullptr;
}

double round3(double v)
{
    return v == 0.0 ? 0.0 : QString::number(v, 'g', 3).toDouble();
}

// Следующее значение ряда (в десятичной мантиссе) выше/ниже v.
double stepSeries(double v, int dir, const double *ser, int n, bool zeroBelowOne)
{
    if (v == 0.0)
        return dir > 0 ? 1.0 : -1.0;
    const bool neg = v < 0.0;
    const int md = neg ? -dir : dir;                      // куда двигается модуль
    const double a = std::fabs(v);
    if (md < 0 && zeroBelowOne && a <= 1.0001)
        return 0.0;
    int e = int(std::floor(std::log10(a)));
    double m = a / std::pow(10.0, e);
    if (m >= 9.9999) { m /= 10.0; ++e; }
    double nm = 0.0;
    if (md > 0) {
        int i = 0;
        while (i < n && ser[i] <= m * (1 + 1e-6))
            ++i;
        if (i == n) { nm = ser[0]; ++e; } else { nm = ser[i]; }
    } else {
        int i = n - 1;
        while (i >= 0 && ser[i] >= m * (1 - 1e-6))
            --i;
        if (i < 0) { nm = ser[n - 1]; --e; } else { nm = ser[i]; }
    }
    const double r = round3(nm * std::pow(10.0, e));
    return neg ? -r : r;
}

const double kE12[] = { 1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2 };
const double kNice[] = { 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };

} // namespace

QString CircuitItem::wheelKey(bool secondary) const
{
    if (isWire())
        return {};
    const WheelEntry *e = wheelEntry(m_type);
    if (!e)
        return {};
    const char *k = secondary ? e->secondary : e->primary;
    if (!k || !findParam(m_type, QString::fromLatin1(k)))
        return {};
    return QString::fromLatin1(k);
}

double CircuitItem::stepValue(const QString &unit, double minV, double maxV, bool integer, double value, int dir)
{
    if (dir == 0)
        return value;
    const int sign = dir > 0 ? 1 : -1;
    double r = value;
    for (int i = 0; i < std::abs(dir); ++i) {
        if (integer) {
            r = std::round(r) + sign;
        } else if (unit == QLatin1String("%")) {
            r = std::round(r / 5.0) * 5.0 + 5.0 * sign;           // движок и освещённость: шаг 5 %
        } else if (unit == QString(QChar(0x00B0))) {
            r = std::round(r / 5.0) * 5.0 + 5.0 * sign;           // углы: шаг 5°
        } else if (unit == QString(QChar(0x03A9)) || unit == QLatin1String("F") || unit == QLatin1String("H")) {
            r = stepSeries(r, sign, kE12, 12, false);              // ряд E12 как у настоящих деталей
        } else {
            r = stepSeries(r, sign, kNice, 11, minV <= 0.0);
        }
        r = std::clamp(r, minV, maxV);
    }
    return r;
}

double CircuitItem::wheelStep(const QString &key, int dir) const
{
    const ParamDef *pd = findParam(m_type, key);
    const double v = param(key);
    if (!pd || dir == 0)
        return v;
    return stepValue(QString::fromUtf8(pd->unit), pd->minValue, pd->maxValue, pd->integer, v, dir);
}

int CircuitItem::stateValue() const
{
    const auto I = [this](const char *k) { return int(std::lround(param(QString::fromLatin1(k)))); };
    switch (m_type) {
    case ElementType::Keypad:     return I("key");                                  // 0 — ничего не нажато
    case ElementType::DipSwitch:   return int(quint32(std::llround(param(QStringLiteral("mask")))));
    case ElementType::LCD:          return I("backlight");
    default: break;
    }
    if (m_type == ElementType::Generator)
        return int(param(QStringLiteral("waveform")));
    if (const ParamDef *pd = toggleParam(m_type))
        return std::max(0, int(param(QString::fromUtf8(pd->key))));
    return 0;
}

// ---------------------------------------------------------------- геометрия

qreal CircuitItem::length() const
{
    return std::hypot(qreal(m_p2.x() - m_p1.x()), qreal(m_p2.y() - m_p1.y()));
}

qreal CircuitItem::minLength() const { return SymbolPainter::minLength(m_type, m_variant); }

qreal CircuitItem::angleDegrees() const
{
    return qRadiansToDegrees(std::atan2(qreal(m_p2.y() - m_p1.y()), qreal(m_p2.x() - m_p1.x())));
}

QPointF CircuitItem::toScene(qreal x, qreal y) const
{
    const qreal len = length();
    QPointF u(1, 0);
    if (len > 0.0)
        u = QPointF((m_p2.x() - m_p1.x()) / len, (m_p2.y() - m_p1.y()) / len);
    const QPointF n(-u.y(), u.x());
    return QPointF(m_p1) + u * x + n * (m_flip ? -y : y);
}

void CircuitItem::setGeometry(const QPoint &a, const QPoint &bIn, bool flip)
{
    const QPoint b = SymbolPainter::normalizeEnd(m_type, a, bIn, m_variant);
    if (a == m_p1 && b == m_p2 && flip == m_flip)
        return;

    prepareGeometryChange();
    m_p1 = a;
    m_p2 = b;
    m_flip = flip;
    m_shapeDirty = true;
    updateLabelRect();
    notifyScene();
    update();
}

void CircuitItem::setEndpoint(int index, const QPoint &p)
{
    if (index == 0)
        setGeometry(SymbolPainter::normalizeEnd(m_type, m_p2, p, m_variant), m_p2, m_flip);
    else
        setGeometry(m_p1, p, m_flip);
}

bool CircuitItem::acceptsEndpoint(int index, const QPoint &p) const
{
    QPoint a = m_p1, b = m_p2;
    (index == 0 ? a : b) = p;
    return SymbolPainter::isValidGeometry(m_type, a, b, m_variant);
}

QPoint CircuitItem::pinPoint(int pin) const
{
    if (isWire())
        return pin == 0 ? m_p1 : m_p2;
    const QPointF l = SymbolPainter::pinLocal(m_type, pin, length(), false, m_variant);
    const QPointF s = toScene(l.x(), l.y());
    return QPoint(qRound(s.x()), qRound(s.y()));
}

QVector<QPoint> CircuitItem::connectionPoints() const
{
    const int n = pinCount();
    QVector<QPoint> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out.append(pinPoint(i));
    return out;
}

int CircuitItem::endpointAt(const QPointF &pos, qreal tol) const
{
    if (isMultiPin())
        return -1;
    const qreal d1 = std::hypot(pos.x() - m_p1.x(), pos.y() - m_p1.y());
    const qreal d2 = std::hypot(pos.x() - m_p2.x(), pos.y() - m_p2.y());
    if (d1 > tol && d2 > tol)
        return -1;
    return d1 <= d2 ? 0 : 1;
}

// ---------------------------------------------------------------- параметры

void CircuitItem::setParam(const QString &key, double value)
{
    prepareGeometryChange();
    m_params[key] = value;
    refreshVariant();
    recomputeKarnaugh();
    m_label = computeLabel();
    updateLabelRect();
    update();
}

void CircuitItem::setParams(const QMap<QString, double> &p)
{
    prepareGeometryChange();
    for (auto it = p.constBegin(); it != p.constEnd(); ++it)
        m_params[it.key()] = it.value();
    refreshVariant();
    recomputeKarnaugh();
    m_label = computeLabel();
    updateLabelRect();
    update();
}

QString CircuitItem::computeLabel() const
{
    using Units::format;
    const auto P = [this](const char *key) { return param(QString::fromLatin1(key)); };
    const QString ohm(QChar(0x03A9));

    // переменные элементы: к номиналу добавляем положение движка
    const auto withPos = [&](const QString &base) {
        return base + QStringLiteral(" · ") + QString::number(qRound(P("position"))) + QLatin1Char('%');
    };

    if (m_type == ElementType::TextLabel)
        return m_text;

    if (!m_readout.isEmpty()) {                         // измерительные приборы показывают измеренное значение
        switch (m_type) {
        case ElementType::Voltmeter:
        case ElementType::Ammeter:
        case ElementType::Ohmmeter:
        case ElementType::Probe:
        case ElementType::TestPoint:
            return m_readout;
        default:
            break;
        }
    }

    switch (m_type) {
    case ElementType::VariableResistor:
    case ElementType::Potentiometer:
        return withPos(format(P("resistance"), ohm));
    case ElementType::VariableCapacitor:
        return withPos(format(P("capacitance"), QStringLiteral("F")));
    case ElementType::Resistor:
        return format(P("resistance"), ohm);
    case ElementType::Capacitor:
    case ElementType::PolarCapacitor:
        return format(P("capacitance"), QStringLiteral("F"));
    case ElementType::Inductor:
    case ElementType::Choke:
        return format(P("inductance"), QStringLiteral("H"));
    case ElementType::Fuse:
        return format(P("current"), QStringLiteral("A"));
    case ElementType::Lamp:
        return format(P("voltage"), QStringLiteral("V"));
    case ElementType::VoltageSourceDC:
    case ElementType::Battery:
        return format(P("voltage"), QStringLiteral("V"));
    case ElementType::VoltageSourceAC:
    case ElementType::Generator:
        return format(P("amplitude"), QStringLiteral("V")) + QLatin1Char(' ')
             + format(P("frequency"), QStringLiteral("Hz"));
    case ElementType::CurrentSource:
        return format(P("current"), QStringLiteral("A"));
    case ElementType::ZenerDiode:
        return format(P("zenerVoltage"), QStringLiteral("V"));
    case ElementType::RailDC:
        return format(P("voltage"), QStringLiteral("V"));
    case ElementType::RailAC:
    case ElementType::RailSquare:
        return format(P("amplitude"), QStringLiteral("V")) + QLatin1Char(' ')
             + format(P("frequency"), QStringLiteral("Hz"));
    case ElementType::RailClock:
        return format(P("frequency"), QStringLiteral("Hz"));
    case ElementType::Crystal:
        return format(P("frequency"), QStringLiteral("Hz"));
    case ElementType::Speaker:
        return format(P("impedance"), ohm);
    case ElementType::Thermistor:
        return format(P("r25"), ohm);
    case ElementType::Varistor:
        return format(P("vclamp"), QStringLiteral("V"));
    case ElementType::Regulator:
        return format(P("vout"), QStringLiteral("V"));
    case ElementType::Varactor:
        return format(P("cj0"), QStringLiteral("F"));
    case ElementType::InductorVar:
        return withPos(format(P("inductance"), QStringLiteral("H")));
    case ElementType::Photoresistor:
        return format(P("light"), ohm) + QStringLiteral("…") + format(P("dark"), ohm);
    default: {                                           // источники, предохранитель и т.п. — главный параметр
        const QString k = wheelKey(false);
        if (k.isEmpty())
            return {};
        const ParamDef *pd = findParam(m_type, k);
        const QString unit = pd ? QString::fromUtf8(pd->unit) : QString();
        if (unit == QLatin1String("%"))
            return QString::number(qRound(param(k))) + QLatin1Char('%');
        return format(param(k), unit);
    }
    }
}

void CircuitItem::updateLabelRect()
{
    m_shapeDirty = true;
    if (m_label.isEmpty()) {
        m_labelRect = QRectF();
        return;
    }
    const QFontMetricsF fm(labelFont(isText() ? int(param(QStringLiteral("size"))) : 12, !isText()));
    const qreal w = fm.horizontalAdvance(m_label) + 8.0;
    const qreal h = fm.height() + 2.0;

    if (isText()) {                                            // надпись — по центру между p1 и p2
        const QPointF mid((m_p1.x() + m_p2.x()) / 2.0, (m_p1.y() + m_p2.y()) / 2.0);
        m_labelRect = QRectF(mid.x() - w / 2.0, mid.y() - h / 2.0, w, h);
        return;
    }

    if (isSinglePin()) {                                       // подпись за знаком на конце ножки
        const qreal dx = m_p2.x() - m_p1.x();
        const qreal dy = m_p2.y() - m_p1.y();
        const qreal len = std::hypot(dx, dy);
        const QPointF dir = len > 0.0 ? QPointF(dx / len, dy / len) : QPointF(0, -1);
        const QPointF center = QPointF(m_p2) + dir * 8.0 + QPointF(dir.x() * w / 2.0, dir.y() * h / 2.0);
        m_labelRect = QRectF(center.x() - w / 2.0, center.y() - h / 2.0, w, h);
        return;
    }

    const QPointF c((m_p1.x() + m_p2.x()) / 2.0, (m_p1.y() + m_p2.y()) / 2.0);
    const qreal dx = m_p2.x() - m_p1.x();
    const qreal dy = m_p2.y() - m_p1.y();
    const qreal len = std::hypot(dx, dy);
    QPointF n(0, -1);
    if (len > 0.0) {
        n = QPointF(dy / len, -dx / len);                      // нормаль к оси
        if (n.y() > 1e-6 || (std::fabs(n.y()) <= 1e-6 && n.x() < 0))
            n = -n;                                            // подпись сверху / справа
    }
    const qreal offset = SymbolPainter::spec(m_type, m_variant).halfHeight + 6.0;
    const QPointF anchor = c + n * offset;
    const QPointF center = anchor + QPointF(n.x() * w / 2.0, n.y() * h / 2.0);
    m_labelRect = QRectF(center.x() - w / 2.0, center.y() - h / 2.0, w, h);
}

// ---------------------------------------------------------------- состояние

void CircuitItem::setPreview(bool on)
{
    if (m_preview == on)
        return;
    m_preview = on;
    notifyScene();
    update();
}

void CircuitItem::setHover(bool body, int endpoint)
{
    if (m_hoverBody == body && m_hoverEndpoint == endpoint)
        return;
    m_hoverBody = body;
    m_hoverEndpoint = endpoint;
    update();
}

void CircuitItem::setNearMiss(quint64 mask)
{
    if (m_nearMask == mask)
        return;
    m_nearMask = mask;
    update();
}

void CircuitItem::setElectrical(const QVector<double> &pinVoltages, double current)
{
    m_hasElectrical = true;
    m_pinV = pinVoltages;
    m_current = current;
    if (m_type == ElementType::LED) {                    // зажигается по току: прямой ток / максимальный ток
        const double maxI = std::max(1e-9, param(QStringLiteral("maxCurrent")));
        m_light = { std::clamp(current / maxI, 0.0, 1.0) };
    }
    update();
    if (m_probe) {
        const double v = pinVoltages.size() >= 2 ? pinVoltages[0] - pinVoltages[1]
                                                 : pinVoltages.value(0);
        m_probe->push(s_simTime, v, current);
    }
}

void CircuitItem::clearElectrical()
{
    if (!m_hasElectrical)
        return;
    m_hasElectrical = false;
    m_pinV.clear();
    m_current = 0;
    m_light.clear();
    update();
}

bool CircuitItem::advanceFlow(qreal dt)
{
    if (!m_hasElectrical || std::fabs(m_current) < 1e-12 || isMultiPin() || isSinglePin())
        return false;
    m_flowPhase = std::fmod(m_flowPhase + (m_current > 0 ? 1 : -1) * s_flowSpeed * dt, kFlowSpacing);
    if (m_flowPhase < 0)
        m_flowPhase += kFlowSpacing;
    update();
    return true;
}

void CircuitItem::notifyScene()
{
    if (auto *s = qobject_cast<CircuitScene *>(scene()))
        s->invalidateConnectivity();
    if (m_probe)
        m_probe->targetMoved();          // линия связи осциллограммы на схеме следует за элементом
}

QVariant CircuitItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSceneChange || change == ItemSceneHasChanged)
        notifyScene();
    else if (change == ItemSelectedHasChanged)
        update();
    return QGraphicsItem::itemChange(change, value);
}

// ---------------------------------------------------------------- отрисовка

QRectF CircuitItem::boundingRect() const
{
    // Область перерисовки обязана накрывать всё, что рисуется: тело, ореол выделения, выводы.
    // Иначе у крупных элементов (память, матрицы) края остаются «гореть» после снятия выделения.
    QRectF r = QRectF(QPointF(m_p1), QPointF(m_p2)).normalized()
                   .adjusted(-kMargin, -kMargin, kMargin, kMargin);
    if (!isWire())
        r = r.united(shape().boundingRect().adjusted(-6, -6, 6, 6));
    if (!m_labelRect.isNull())
        r = r.united(m_labelRect.adjusted(-2, -2, 2, 2));
    if (isLightEmitter() && !isSinglePin())
        r = r.adjusted(-34, -34, 34, 34);                // ореол свечения выходит за корпус
    return r;
}

QPainterPath CircuitItem::shape() const
{
    if (!m_shapeDirty)
        return m_shapeCache;

    // Фигура собирается через addPath с правилом WindingFill: это дёшево, в отличие от united() на каждый вывод
    // (у модулей на 64 вывода пересборка при каждом шаге переноса давала заметные подвисания).
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    if (!isMultiPin()) {
        // У многовыводных элементов ось не выступает за корпус: её «трубка» давала лишний хвост в ореоле выделения.
        QPainterPath axis;
        axis.moveTo(m_p1);
        axis.lineTo(m_p2);
        QPainterPathStroker st;
        st.setWidth(isWire() || m_type == ElementType::Line ? 12.0 : 22.0);
        st.setCapStyle(Qt::RoundCap);
        path = st.createStroke(axis);
        path.setFillRule(Qt::WindingFill);
    }

    if (isBox()) {                                             // только контур рамки, середина прозрачна для кликов
        const QRectF r = QRectF(QPointF(m_p1), QPointF(m_p2)).normalized();
        QPainterPath outline;
        outline.addRect(r);
        QPainterPathStroker bs;
        bs.setWidth(12.0);
        path = bs.createStroke(outline);
    }
    if (isText() && !m_labelRect.isNull())
        path.addRect(m_labelRect);

    if (!isWire() && !isDecoration()) {
        const SymbolPainter::Spec s = SymbolPainter::spec(m_type, m_variant);
        const qreal len = length();
        const qreal cx = len / 2.0;
        const qreal hw = (isSinglePin() ? 16.0 : s.body / 2.0) + 3.0;
        const qreal hh = s.halfHeight + 2.0;
        const qreal x0 = isSinglePin() ? len - 16.0 : cx - hw;
        const qreal x1 = isSinglePin() ? len + 3.0 : cx + hw;
        QPolygonF body;
        body << toScene(x0, -hh) << toScene(x1, -hh) << toScene(x1, hh) << toScene(x0, hh);
        QPainterPath bodyPath;
        bodyPath.addPolygon(body);
        bodyPath.closeSubpath();
        path.addPath(bodyPath);

        if (isMultiPin()) {                                   // выводы вне оси
            QPainterPathStroker ls;
            ls.setWidth(14.0);
            ls.setCapStyle(Qt::RoundCap);
            for (int i = 0; i < s.pinCount; ++i) {
                const QPointF l = SymbolPainter::pinLocal(m_type, i, len, false, m_variant);
                const QPointF in = SymbolPainter::pinInward(m_type, i, m_variant);
                QPainterPath lead;
                lead.moveTo(toScene(l.x(), l.y()));
                lead.lineTo(toScene(l.x() + in.x() * 18.0, l.y() + in.y() * 18.0));
                path.addPath(ls.createStroke(lead));
            }
        }
    }

    // кружки у выводов/ручек — чтобы за концы было удобно хватать
    QPainterPath dots;
    if (isMultiPin()) {
        for (const QPoint &pt : connectionPoints())
            dots.addEllipse(QPointF(pt), 8.0, 8.0);
    } else {
        dots.addEllipse(QPointF(m_p1), 7.0, 7.0);
        dots.addEllipse(QPointF(m_p2), 7.0, 7.0);
    }
    path.addPath(dots);

    m_shapeCache = path;
    m_shapeDirty = false;
    return m_shapeCache;
}

void CircuitItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    Theme &theme = Theme::instance();
    const Palette &pal = theme.palette();
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform());

    p->setRenderHint(QPainter::Antialiasing, true);
    if (m_preview)
        p->setOpacity(0.65);

    const qreal w = std::max<qreal>(2.0, 1.4 / lod);
    const QPointF a(m_p1);
    const QPointF b(m_p2);
    const qreal len = length();
    const int pins = pinCount();

    // --- ореол выделения / наведения -----------------------------------
    if (isSelected() || m_hoverBody) {
        QColor halo = pal.accent;
        halo.setAlpha(isSelected() ? 58 : 28);
        p->setPen(Qt::NoPen);
        p->setBrush(halo);
        p->drawPath(shape());
    }

    // --- проблема: красный ореол всего элемента -------------------------------
    if (m_problemBody && !m_preview) {
        QColor red = pal.negative;
        red.setAlpha(80);
        p->setPen(QPen(pal.negative, 1.5));
        p->setBrush(red);
        p->drawPath(shape());
    }

    // --- свечение индикатора: мягкий ореол цвета, который выбрал пользователь ---
    if (!m_preview && !m_light.isEmpty() && m_type != ElementType::SegDisplay && m_type != ElementType::LedMatrix) {
        const double lv = std::clamp(m_type == ElementType::LedRGB
                                         ? std::max({ m_light.value(0), m_light.value(1), m_light.value(2) })
                                         : m_light[0], 0.0, 1.0);
        if (lv > 0.02) {
            const QColor g = glowColor();
            const QPointF center = isSinglePin() ? a + (b - a) * 0.8 : (a + b) / 2.0;
            const qreal radius = isSinglePin() ? 22.0 : 32.0;
            QRadialGradient grad(center, radius);
            QColor c0 = g, c1 = g, c2 = g;
            c0.setAlphaF(0.55 * lv);
            c1.setAlphaF(0.22 * lv);
            c2.setAlpha(0);
            grad.setColorAt(0.0, c0);
            grad.setColorAt(0.55, c1);
            grad.setColorAt(1.0, c2);
            p->setPen(Qt::NoPen);
            p->setBrush(grad);
            p->drawEllipse(center, radius, radius);
        }
    }

    // --- цвета выводов по напряжению ------------------------------------
    QColor pinColor[SymbolPainter::kMaxPins];
    for (QColor &pc : pinColor)
        pc = pal.wire;
    if (m_hasElectrical) {
        for (int i = 0; i < pins && i < m_pinV.size() && i < SymbolPainter::kMaxPins; ++i)
            pinColor[i] = theme.voltageColor(m_pinV[i]);
        if (isWire() && m_pinV.size() >= 2) {
            const QColor avg = theme.voltageColor((m_pinV[0] + m_pinV[1]) / 2.0);
            pinColor[0] = pinColor[1] = avg;
        }
    }

    // --- символ в локальной системе -------------------------------------
    p->save();
    p->translate(a);
    p->rotate(angleDegrees());
    if (m_flip)
        p->scale(1.0, -1.0);

    if (isWire()) {
        p->setPen(QPen(pinColor[0], w, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(0, 0), QPointF(len, 0));
    } else {
        SymbolPainter::PaintContext ctx;
        ctx.p = p;
        ctx.type = m_type;
        ctx.length = len;
        ctx.angle = angleDegrees();
        ctx.flip = m_flip;
        ctx.variant = m_variant;
        ctx.state = stateValue();
        ctx.width = w;
        ctx.body = pal.element;
        if (isLightEmitter() || m_params.contains(QStringLiteral("glow"))) {
            ctx.glow = glowColor();
            ctx.light = m_light;
        }
        ctx.readout = m_readout;
        if (isKarnaugh()) {
            ctx.cells = m_cells;
            ctx.result = m_formula;
        }
        if (m_type == ElementType::DecimalDisplay)
            ctx.digits = int(param(QStringLiteral("digits")));
        for (int i = 0; i < SymbolPainter::kMaxPins; ++i)
            ctx.pin[i] = pinColor[i];
        SymbolPainter::paint(ctx);
    }

    // --- красные концы: слишком близко, но не соединено -------------------
    if (m_nearMask != 0 && !m_preview) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(pal.nearMiss, w + 1.2, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < pins; ++i) {
            if (!(m_nearMask & (quint64(1) << i)))
                continue;
            const QPointF at = SymbolPainter::pinLocal(m_type, i, len, false, m_variant);
            const QPointF in = SymbolPainter::pinInward(m_type, i, m_variant);
            const qreal lead = isWire() ? std::min<qreal>(len, kRedLead) : kRedLead;
            p->drawLine(at, at + in * lead);
        }
    }
    p->restore();

    // --- проблема: красные кольца у выводов -------------------------------
    if (m_problemMask != 0 && !m_preview) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(pal.negative, 2.2));
        const QVector<QPoint> pts = connectionPoints();
        for (int i = 0; i < pts.size() && i < SymbolPainter::kMaxPins; ++i)
            if (m_problemMask & (quint64(1) << i))
                p->drawEllipse(QPointF(pts[i]), 7.5, 7.5);
    }

    // --- точки тока ------------------------------------------------------
    if (m_hasElectrical && len > 0.0 && lod > 0.3 && std::fabs(m_current) >= 1e-12
        && !isMultiPin() && !isSinglePin()) {
        const QPointF unit((b.x() - a.x()) / len, (b.y() - a.y()) / len);
        p->setPen(Qt::NoPen);
        p->setBrush(pal.current);
        for (qreal s = m_flowPhase; s < len; s += kFlowSpacing)
            p->drawEllipse(a + unit * s, 2.3, 2.3);
    }

    // --- маркеры выводов / ручек -----------------------------------------
    if (!m_preview && (isSelected() || m_hoverEndpoint >= 0)) {
        if (isMultiPin()) {
            const QVector<QPoint> pts = connectionPoints();
            QColor fill = pal.accent;
            fill.setAlpha(210);
            p->setPen(Qt::NoPen);
            p->setBrush(fill);
            for (const QPoint &pt : pts)
                p->drawEllipse(QPointF(pt), 3.6, 3.6);
        } else {
            for (int i = 0; i < 2; ++i) {
                const bool hot = (i == m_hoverEndpoint);
                const QPointF pt = i == 0 ? a : b;
                QColor fill = pal.accent;
                fill.setAlpha(hot ? 255 : 200);
                p->setPen(hot ? QPen(pal.element, 1.5) : QPen(Qt::NoPen));
                p->setBrush(fill);
                p->drawEllipse(pt, hot ? 6.0 : 3.8, hot ? 6.0 : 3.8);
            }
        }
    }

    // --- подпись ---------------------------------------------------------
    if (!m_label.isEmpty() && (isText() || (theme.showLabels() && lod > 0.45))) {
        p->setFont(labelFont(isText() ? int(param(QStringLiteral("size"))) : 12, !isText()));
        p->setPen(isSelected() ? pal.accent : pal.text);
        p->drawText(m_labelRect, Qt::AlignCenter, m_label);
    }
}
