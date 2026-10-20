#include "pcb/PcbWidgets.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QRadioButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <vector>

// ================================================================================================
//  Форматирование
// ================================================================================================
namespace PcbFmt {

namespace {
QLocale numberLocale()
{
    QLocale l = QLocale::system();
    l.setNumberOptions(QLocale::OmitGroupSeparator);
    return l;
}
} // namespace

QString fixed(double v, int decimals)
{
    if (std::isnan(v))
        return QStringLiteral("—");
    if (std::isinf(v))
        return v > 0 ? QStringLiteral("∞") : QStringLiteral("−∞");
    return numberLocale().toString(v, 'f', std::max(0, decimals));
}

QString trimmed(double v)
{
    if (!std::isfinite(v))
        return QString();
    if (v == 0.0)
        return QStringLiteral("0");
    const double a = std::fabs(v);
    if (a >= 1e9 || a < 1e-6)
        return numberLocale().toString(v, 'g', 8);
    int decimals = 7 - int(std::floor(std::log10(a)));
    decimals = std::clamp(decimals, 0, 12);
    QString s = QString::number(v, 'f', decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (numberLocale().decimalPoint() != QLatin1Char('.'))
        s.replace(QLatin1Char('.'), numberLocale().decimalPoint());
    return s;
}

bool parse(const QString &text, double *out)
{
    QString t = text.trimmed();
    t.remove(QLatin1Char(' '));
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    if (t.isEmpty())
        return false;
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    *out = v;
    return true;
}

QString info(const QString &label, const QString &value, bool warn)
{
    const QString style = warn ? QStringLiteral(" style=\"color:#e5675d\"") : QString();
    return QStringLiteral("<div%1><span style=\"opacity:0.7\">%2</span><br><b>%3</b></div>").arg(style, label.toHtmlEscaped(), value.toHtmlEscaped());
}

} // namespace PcbFmt

// ================================================================================================
//  Единицы
// ================================================================================================
namespace PcbUnits {
QVector<PcbUnit> lengthMm()
{
    return { { QStringLiteral("мм"), 1.0 }, { QStringLiteral("мил"), 0.0254 }, { QStringLiteral("мкм"), 0.001 } };
}
QVector<PcbUnit> lengthUm()
{
    return { { QStringLiteral("мкм"), 0.001 }, { QStringLiteral("мил"), 0.0254 } };
}
QVector<PcbUnit> lengthCm()
{
    return { { QStringLiteral("см"), 10.0 }, { QStringLiteral("дюйм"), 25.4 } };
}
QVector<PcbUnit> areaMm2()
{
    return { { QStringLiteral("мм²"), 1.0 }, { QStringLiteral("дюйм²"), 645.16 } };
}
QVector<PcbUnit> areaSmall()
{
    return { { QStringLiteral("мм²"), 1.0 }, { QStringLiteral("мил²"), 0.00064516 } };
}
QVector<PcbUnit> lengthM()
{
    return { { QStringLiteral("м"), 1.0 }, { QStringLiteral("фут"), 0.3048 } };
}
} // namespace PcbUnits

// ================================================================================================
//  PcbField
// ================================================================================================
PcbField::PcbField(Kind kind, QWidget *parent)
    : QFrame(parent), m_kind(kind)
{
    setObjectName(QStringLiteral("pcbField"));
    setProperty("output", kind == Output);
    setFixedHeight(32);
    setMinimumWidth(110);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(6);

    m_edit = new QLineEdit(this);
    m_edit->setFrame(false);
    m_edit->setReadOnly(kind == Output);
    m_edit->setFocusPolicy(kind == Output ? Qt::ClickFocus : Qt::StrongFocus);
    lay->addWidget(m_edit, 1);

    m_unitLabel = new QLabel(this);
    m_unitLabel->setObjectName(QStringLiteral("pcbUnit"));
    m_unitLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_unitLabel);

    m_unitCombo = new QComboBox(this);
    m_unitCombo->setObjectName(QStringLiteral("pcbUnitCombo"));
    m_unitCombo->setFrame(false);
    m_unitCombo->hide();
    lay->addWidget(m_unitCombo);

    connect(m_edit, &QLineEdit::textEdited, this, [this] { onTextEdited(); });
    connect(m_unitCombo, &QComboBox::currentIndexChanged, this, [this](int i) { onChoiceChanged(i); });
}

void PcbField::setUnit(const QString &unit)
{
    m_units.clear();
    m_choiceMode = false;
    m_unitLabel->setText(unit);
    m_unitLabel->setVisible(!unit.isEmpty());
    m_unitCombo->hide();
}

void PcbField::setUnits(const QVector<PcbUnit> &units)
{
    m_units = units;
    m_choiceMode = false;
    m_index = 0;
    m_unitCombo->hide();
    m_unitLabel->show();
    m_unitLabel->setText(units.isEmpty() ? QString() : units.first().label);
    refresh();
}

void PcbField::setUnitMode(int mode)
{
    if (m_units.isEmpty() || m_choiceMode)
        return;
    m_index = mode >= m_units.size() ? 0 : std::max(0, mode);
    m_unitLabel->setText(m_units[m_index].label);
    refresh();
}

void PcbField::setUnitChoices(const QVector<PcbUnit> &units, int index)
{
    m_units = units;
    m_choiceMode = true;
    m_index = std::clamp(index, 0, std::max(0, int(units.size()) - 1));
    m_unitLabel->hide();
    QSignalBlocker block(m_unitCombo);
    m_unitCombo->clear();
    for (const PcbUnit &u : units)
        m_unitCombo->addItem(u.label);
    m_unitCombo->setCurrentIndex(m_index);
    m_unitCombo->show();
    refresh();
}

void PcbField::setUnitIndex(int index)
{
    if (m_units.isEmpty())
        return;
    m_index = std::clamp(index, 0, int(m_units.size()) - 1);
    if (m_choiceMode) {
        QSignalBlocker block(m_unitCombo);
        m_unitCombo->setCurrentIndex(m_index);
    } else {
        m_unitLabel->setText(m_units[m_index].label);
    }
    refresh();
}

void PcbField::setEditable(bool on)
{
    if (m_kind == Output)
        return;
    setEnabled(on);
}

void PcbField::setPlaceholder(const QString &text) { m_edit->setPlaceholderText(text); }

void PcbField::setOutputMode(bool output)
{
    m_kind = output ? Output : Input;
    m_edit->setReadOnly(output);
    m_edit->setFocusPolicy(output ? Qt::ClickFocus : Qt::StrongFocus);
    setProperty("output", output);
    style()->unpolish(this);
    style()->polish(this);
    refresh();
}

void PcbField::setValue(double base)
{
    m_textOverride = false;
    m_base = base;
    refresh();
}

void PcbField::setText(const QString &text)
{
    m_textOverride = true;
    m_base = PcbFmt::kNaN;
    m_edit->setText(text);
    m_edit->setCursorPosition(0);
}

QString PcbField::text() const { return m_edit->text(); }

void PcbField::refresh()
{
    if (m_textOverride)
        return;
    const double shown = std::isfinite(m_base) ? m_base / factor() : m_base;
    const QString s = m_kind == Output ? PcbFmt::fixed(shown, m_decimals) : PcbFmt::trimmed(shown);
    if (m_edit->text() != s) {
        m_edit->setText(s);
        m_edit->setCursorPosition(0);
    }
}

void PcbField::onTextEdited()
{
    m_textOverride = false;
    double shown = 0;
    m_base = PcbFmt::parse(m_edit->text(), &shown) ? shown * factor() : PcbFmt::kNaN;
    emit edited();
}

void PcbField::onChoiceChanged(int index)
{
    m_index = std::clamp(index, 0, std::max(0, int(m_units.size()) - 1));
    if (m_kind == Output) {          // результат остаётся тем же, меняется только показ
        refresh();
        return;
    }
    double shown = 0;
    m_base = PcbFmt::parse(m_edit->text(), &shown) ? shown * factor() : PcbFmt::kNaN;
    emit edited();
}

// ================================================================================================
//  PcbChoice
// ================================================================================================
PcbChoice::PcbChoice(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
{
    setObjectName(QStringLiteral("pcbGroup"));
    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(12, 8, 12, 8);
    grid->setSpacing(2);
    connect(m_group, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (checked)
            emit currentChanged(id);
    });
}

void PcbChoice::addOption(const QString &text)
{
    auto *b = new QRadioButton(text, this);
    m_group->addButton(b, m_buttons.size());
    m_buttons.append(b);
    if (m_buttons.size() == 1)
        b->setChecked(true);
    relayout();
}

void PcbChoice::setOptions(const QStringList &texts)
{
    for (const QString &t : texts)
        addOption(t);
}

int PcbChoice::currentIndex() const { return m_group->checkedId(); }

void PcbChoice::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_buttons.size())
        m_buttons[index]->setChecked(true);
}

void PcbChoice::setOptionEnabled(int index, bool on)
{
    if (index >= 0 && index < m_buttons.size())
        m_buttons[index]->setEnabled(on);
}

void PcbChoice::setOptionVisible(int index, bool on)
{
    if (index >= 0 && index < m_buttons.size())
        m_buttons[index]->setVisible(on);
}

void PcbChoice::setColumns(int columns)
{
    m_columns = std::max(1, columns);
    relayout();
}

void PcbChoice::relayout()
{
    auto *grid = static_cast<QGridLayout *>(layout());
    for (int i = 0; i < m_buttons.size(); ++i)
        grid->addWidget(m_buttons[i], i / m_columns, i % m_columns);
}

// ================================================================================================
//  PcbStepSlider
// ================================================================================================
PcbStepSlider::PcbStepSlider(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setTickPosition(QSlider::TicksBelow);
    m_slider->setPageStep(1);
    lay->addWidget(m_slider, 1);
    m_display = new PcbField(PcbField::Output, this);
    m_display->setMinimumWidth(84);
    m_display->setMaximumWidth(96);
    lay->addWidget(m_display);
    connect(m_slider, &QSlider::valueChanged, this, [this](int i) {
        if (i >= 0 && i < m_labels.size())
            m_display->setText(m_labels[i]);
        emit changed();
    });
}

void PcbStepSlider::setSteps(const QVector<double> &values, const QStringList &labels, int index)
{
    m_values = values;
    m_labels = labels;
    QSignalBlocker block(m_slider);
    m_slider->setRange(0, std::max(0, int(values.size()) - 1));
    m_slider->setTickInterval(1);
    m_slider->setValue(std::clamp(index, 0, std::max(0, int(values.size()) - 1)));
    if (!labels.isEmpty())
        m_display->setText(labels[m_slider->value()]);
}

int PcbStepSlider::index() const { return m_slider->value(); }
double PcbStepSlider::value() const { return m_values.value(m_slider->value(), 1.0); }
void PcbStepSlider::setIndex(int index) { m_slider->setValue(index); }

// ================================================================================================
//  Группы и подписи
// ================================================================================================
QGroupBox *makePcbGroup(const QString &title, QWidget *parent)
{
    auto *g = new QGroupBox(title, parent);
    g->setObjectName(QStringLiteral("pcbGroup"));
    return g;
}

QWidget *pcbLabeled(const QString &label, QWidget *field, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(3);
    auto *l = new QLabel(label, w);
    l->setObjectName(QStringLiteral("pcbLabel"));
    l->setWordWrap(true);
    lay->addWidget(l);
    lay->addWidget(field);
    return w;
}

// ================================================================================================
//  Схемы-иллюстрации
// ================================================================================================
namespace {

const QColor kCard(0xf1, 0xf3, 0xf7);
const QColor kBoard(0x2f, 0x9e, 0x57);
const QColor kBoardDark(0x1f, 0x7a, 0x41);
const QColor kBoardLight(0x49, 0xb4, 0x6c);
const QColor kCopper(0x2f, 0x6f, 0xe0);
const QColor kCopperDark(0x1d, 0x4c, 0xa8);
const QColor kInk(0x23, 0x2a, 0x36);
const QColor kDash(0x5a, 0x64, 0x78);

void bubble(QPainter &p, const QPointF &c, const QString &t)
{
    QFont f = p.font();
    f.setPixelSize(11);
    f.setBold(true);
    p.save();
    p.setFont(f);
    const QFontMetricsF fm(f);
    const QRectF r(c.x() - fm.horizontalAdvance(t) / 2 - 5, c.y() - 9, fm.horizontalAdvance(t) + 10, 18);
    p.setPen(QPen(QColor(0xb9, 0xc0, 0xcc), 1));
    p.setBrush(Qt::white);
    p.drawRoundedRect(r, 5, 5);
    p.setPen(kInk);
    p.drawText(r, Qt::AlignCenter, t);
    p.restore();
}

void arrowHead(QPainter &p, const QPointF &tip, const QPointF &dir)
{
    const QPointF n(-dir.y(), dir.x());
    QPolygonF poly;
    poly << tip << tip - dir * 6 + n * 3 << tip - dir * 6 - n * 3;
    p.save();
    p.setBrush(kInk);
    p.setPen(Qt::NoPen);
    p.drawPolygon(poly);
    p.restore();
}

void dimH(QPainter &p, double x1, double x2, double y, const QString &t)
{
    p.save();
    p.setPen(QPen(kInk, 1.2));
    p.drawLine(QPointF(x1, y), QPointF(x2, y));
    p.drawLine(QPointF(x1, y - 4), QPointF(x1, y + 4));
    p.drawLine(QPointF(x2, y - 4), QPointF(x2, y + 4));
    arrowHead(p, QPointF(x1, y), QPointF(-1, 0));
    arrowHead(p, QPointF(x2, y), QPointF(1, 0));
    p.restore();
    bubble(p, QPointF((x1 + x2) / 2, y), t);
}

void dimV(QPainter &p, double x, double y1, double y2, const QString &t)
{
    p.save();
    p.setPen(QPen(kInk, 1.2));
    p.drawLine(QPointF(x, y1), QPointF(x, y2));
    p.drawLine(QPointF(x - 4, y1), QPointF(x + 4, y1));
    p.drawLine(QPointF(x - 4, y2), QPointF(x + 4, y2));
    arrowHead(p, QPointF(x, y1), QPointF(0, -1));
    arrowHead(p, QPointF(x, y2), QPointF(0, 1));
    p.restore();
    bubble(p, QPointF(x, (y1 + y2) / 2), t);
}

void trapezoid(QPainter &p, double xc, double yBottom, double w, double h, const QColor &fill)
{
    QPolygonF poly;
    poly << QPointF(xc - w / 2, yBottom) << QPointF(xc - w / 2 + h * 0.5, yBottom - h)
         << QPointF(xc + w / 2 - h * 0.5, yBottom - h) << QPointF(xc + w / 2, yBottom);
    p.save();
    p.setPen(QPen(kCopperDark, 1));
    p.setBrush(fill);
    p.drawPolygon(poly);
    p.restore();
}

// Разрез платы: слой диэлектрика и нижняя плоскость. Возвращает прямоугольник диэлектрика.
QRectF drawBoardCut(QPainter &p, const QRectF &r, double top, double dielectricH, bool bottomPlane = true)
{
    const QRectF board(r.left() + r.width() * 0.08, top, r.width() * 0.84, dielectricH);
    p.save();
    p.setPen(QPen(kBoardDark, 1));
    p.setBrush(kBoard);
    p.drawRect(board);
    if (bottomPlane) {
        p.setPen(QPen(kCopperDark, 1));
        p.setBrush(kCopper);
        p.drawRect(QRectF(board.left(), board.bottom(), board.width(), 6));
    }
    p.restore();
    return board;
}

void drawMicrostrip(QPainter &p, const QRectF &r, int traces, bool embedded)
{
    const double top = r.top() + r.height() * 0.42;
    const double h = r.height() * 0.34;
    const QRectF board = drawBoardCut(p, r, top, h);
    const double tw = r.width() * (traces == 2 ? 0.17 : 0.26);
    const double cx = board.center().x();
    double gap = r.width() * 0.10;
    std::vector<double> centers;
    if (traces == 2)
        centers = { cx - (tw + gap) / 2, cx + (tw + gap) / 2 };
    else
        centers = { cx };
    if (embedded) {
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(kBoardLight);
        p.drawRect(QRectF(board.left(), top - 14, board.width(), 14));
        p.restore();
    }
    for (double c : centers)
        trapezoid(p, c, top, tw, 8, kCopper);
    dimH(p, centers.front() - tw / 2, centers.front() + tw / 2, top - (embedded ? 24 : 16), QStringLiteral("W"));
    if (traces == 2) {
        dimH(p, centers.front() + tw / 2, centers.back() - tw / 2, top - 16, QStringLiteral("S"));
        dimH(p, centers.back() - tw / 2, centers.back() + tw / 2, top - 32, QStringLiteral("W"));
    }
    dimV(p, board.right() - 18, top, board.bottom(), QStringLiteral("H"));
    if (embedded)
        dimV(p, board.left() + 18, top - 14, top, QStringLiteral("B"));
}

void drawStripline(QPainter &p, const QRectF &r, int kind)     // 0 — симметричная, 1 — асимметричная, 2 — двойная
{
    const double top = r.top() + r.height() * 0.22;
    const double h = r.height() * 0.56;
    const QRectF board(r.left() + r.width() * 0.08, top, r.width() * 0.84, h);
    p.save();
    p.setPen(QPen(kBoardDark, 1));
    p.setBrush(kBoard);
    p.drawRect(board);
    p.setPen(QPen(kCopperDark, 1));
    p.setBrush(kCopper);
    p.drawRect(QRectF(board.left(), board.bottom(), board.width(), 6));
    p.drawRect(QRectF(board.left(), board.top() - 6, board.width(), 6));
    p.restore();
    const double tw = r.width() * 0.22;
    const double cx = board.center().x() - 12;
    double y = board.center().y();
    if (kind == 1)
        y = board.bottom() - h * 0.28;
    if (kind == 2)
        y = board.bottom() - h * 0.22;
    trapezoid(p, cx, y + 4, tw, 8, kCopper);
    if (kind == 2)
        trapezoid(p, cx + tw * 0.5, y - h * 0.3 + 4, tw, 8, kCopper);
    dimH(p, cx - tw / 2, cx + tw / 2, y - 14, QStringLiteral("W"));
    dimV(p, board.right() - 16, y + 4, board.bottom(), QStringLiteral("H"));
    if (kind != 0)
        dimV(p, board.left() + 16, board.top(), y - 4, kind == 1 ? QStringLiteral("H2") : QStringLiteral("D"));
}

void drawCoplanar(QPainter &p, const QRectF &r)
{
    const double top = r.top() + r.height() * 0.42;
    const double h = r.height() * 0.34;
    const QRectF board = drawBoardCut(p, r, top, h);
    const double tw = r.width() * 0.16, gap = r.width() * 0.08, gw = r.width() * 0.16;
    const double cx = board.center().x();
    trapezoid(p, cx, top, tw, 8, kCopper);
    trapezoid(p, cx - tw / 2 - gap - gw / 2, top, gw, 8, kCopper);
    trapezoid(p, cx + tw / 2 + gap + gw / 2, top, gw, 8, kCopper);
    dimH(p, cx - tw / 2, cx + tw / 2, top - 18, QStringLiteral("W"));
    dimH(p, cx + tw / 2, cx + tw / 2 + gap, top - 34, QStringLiteral("S"));
    dimV(p, board.right() - 14, top, board.bottom(), QStringLiteral("H"));
}

void drawRiseTime(QPainter &p, const QRectF &r)
{
    const double x0 = r.left() + 14, x1 = r.right() - 14;
    const double yLow = r.bottom() - 34, yHigh = r.top() + 30;
    p.save();
    QFont f = p.font(); f.setPixelSize(10); f.setBold(true); p.setFont(f);
    p.setPen(kInk);
    p.drawText(QRectF(r.left(), r.top() + 6, r.width() - 12, 14), Qt::AlignRight, QStringLiteral("RISE TIME"));
    QPainterPath path;
    path.moveTo(x0, yLow);
    path.lineTo(x0 + (x1 - x0) * 0.25, yLow);
    path.cubicTo(x0 + (x1 - x0) * 0.40, yLow, x0 + (x1 - x0) * 0.42, yHigh - 8, x0 + (x1 - x0) * 0.55, yHigh - 4);
    path.cubicTo(x0 + (x1 - x0) * 0.62, yHigh - 2, x0 + (x1 - x0) * 0.70, yHigh + 2, x1, yHigh);
    p.setPen(QPen(kCopper, 2.2));
    p.drawPath(path);
    QPen dash(kDash, 1, Qt::DashLine);
    p.setPen(dash);
    const double y10 = yLow - (yLow - yHigh) * 0.10, y90 = yLow - (yLow - yHigh) * 0.90;
    p.drawLine(QPointF(x0, y10), QPointF(x1, y10));
    p.drawLine(QPointF(x0, y90), QPointF(x1, y90));
    p.setPen(kInk);
    p.drawText(QPointF(x1 - 26, y90 - 3), QStringLiteral("90%"));
    p.drawText(QPointF(x1 - 26, y10 - 3), QStringLiteral("10%"));
    const double xa = x0 + (x1 - x0) * 0.33, xb = x0 + (x1 - x0) * 0.50;
    dimH(p, xa, xb, yLow + 14, QStringLiteral("tr"));
    p.restore();
}

void drawPeriod(QPainter &p, const QRectF &r)
{
    p.save();
    QFont f = p.font(); f.setPixelSize(10); f.setBold(true); p.setFont(f);
    p.setPen(kInk);
    p.drawText(QRectF(r.left(), r.top() + 6, r.width() - 12, 14), Qt::AlignRight, QStringLiteral("PERIOD"));
    const double x0 = r.left() + 14, x1 = r.right() - 14, yc = r.center().y() + 6, amp = r.height() * 0.28;
    QPainterPath path;
    for (int i = 0; i <= 200; ++i) {
        const double t = i / 200.0;
        const QPointF pt(x0 + (x1 - x0) * t, yc - amp * std::sin(t * 2 * 3.14159265 * 2));
        i == 0 ? path.moveTo(pt) : path.lineTo(pt);
    }
    p.setPen(QPen(kCopper, 2.2));
    p.drawPath(path);
    p.setPen(QPen(kDash, 1));
    p.drawLine(QPointF(x0, yc), QPointF(x1, yc));
    dimH(p, x0 + (x1 - x0) * 0.25, x0 + (x1 - x0) * 0.75, r.top() + 26, QStringLiteral("T"));
    p.restore();
}

void drawVia(QPainter &p, const QRectF &r)
{
    const double d = std::min(r.width() * 0.5, r.height()) * 0.82;
    const QPointF c(r.left() + r.width() * 0.28, r.center().y());
    p.save();
    p.setPen(QPen(kBoardDark, 1));
    p.setBrush(kBoard);
    p.drawEllipse(c, d / 2, d / 2);
    p.setBrush(kBoardLight);
    p.drawEllipse(c, d * 0.38, d * 0.38);
    p.setPen(QPen(kCopperDark, 1));
    p.setBrush(kCopper);
    p.drawEllipse(c, d * 0.24, d * 0.24);
    p.setBrush(QColor(0x15, 0x18, 0x1e));
    p.drawEllipse(c, d * 0.12, d * 0.12);
    p.restore();
    // боковой разрез
    const QRectF cut(r.left() + r.width() * 0.58, r.top() + r.height() * 0.12, r.width() * 0.36, r.height() * 0.76);
    p.save();
    p.setPen(QPen(kBoardDark, 1));
    p.setBrush(kBoard);
    p.drawRect(cut);
    p.setPen(QPen(kCopperDark, 1));
    p.setBrush(kCopper);
    const double bw = cut.width() * 0.16;
    p.drawRect(QRectF(cut.center().x() - bw / 2, cut.top() - 4, bw, cut.height() + 8));
    for (double fy : { 0.30, 0.55 }) {
        p.drawRect(QRectF(cut.left(), cut.top() + cut.height() * fy, cut.width() * 0.34, 3));
        p.drawRect(QRectF(cut.right() - cut.width() * 0.34, cut.top() + cut.height() * fy, cut.width() * 0.34, 3));
    }
    p.restore();
    dimV(p, cut.right() + 6 > r.right() - 4 ? r.right() - 6 : cut.right() + 6, cut.top(), cut.bottom(), QStringLiteral("H"));
}

void drawPlanar(QPainter &p, const QRectF &r)
{
    const double s = std::min(r.width(), r.height()) * 0.86;
    const QRectF sq(r.center().x() - s / 2, r.center().y() - s / 2, s, s);
    p.save();
    p.setPen(QPen(kBoardDark, 1));
    p.setBrush(QColor(0xf8, 0xf9, 0xfb));
    p.drawRoundedRect(sq, 6, 6);
    QPainterPath path;
    const int turns = 5;
    const double step = s * 0.075;
    double x = sq.left() + s * 0.06, y = sq.top() + s * 0.06;
    double w = s * 0.88, hgt = s * 0.88;
    path.moveTo(x, y + hgt);
    path.lineTo(x, y);
    path.lineTo(x + w, y);
    path.lineTo(x + w, y + hgt);
    path.lineTo(x + step, y + hgt);
    double left = x + step, top = y + step, right = x + w - step, bottom = y + hgt - step;
    for (int i = 1; i < turns * 2 - 2; ++i) {
        switch (i % 4) {
        case 1: path.lineTo(left, top); left += step; break;
        case 2: path.lineTo(right, top); top += step; break;
        case 3: path.lineTo(right, bottom); right -= step; break;
        case 0: path.lineTo(left, bottom); bottom -= step; break;
        }
    }
    p.setPen(QPen(kCopper, 3.4, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.restore();
}

void drawPadstack(QPainter &p, const QRectF &r)
{
    const double d = std::min(r.width(), r.height()) * 0.86;
    const QPointF c = r.center();
    p.save();
    p.setPen(QPen(kDash, 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, d / 2, d / 2);
    p.setPen(QPen(kCopperDark, 1));
    p.setBrush(kCopper);
    const double ro = d * 0.40, ri = d * 0.24;
    for (int i = 0; i < 4; ++i) {                          // четыре сегмента тепловой развязки
        QPainterPath seg;
        const double a0 = 20 + i * 90, span = 50;
        seg.arcMoveTo(QRectF(c.x() - ro, c.y() - ro, ro * 2, ro * 2), a0);
        seg.arcTo(QRectF(c.x() - ro, c.y() - ro, ro * 2, ro * 2), a0, span);
        seg.arcTo(QRectF(c.x() - ri, c.y() - ri, ri * 2, ri * 2), a0 + span, -span);
        seg.closeSubpath();
        p.drawPath(seg);
    }
    p.setBrush(QColor(0x15, 0x18, 0x1e));
    p.drawEllipse(c, d * 0.13, d * 0.13);
    p.restore();
}

void drawPdn(QPainter &p, const QRectF &r)
{
    const QRectF plot(r.left() + 30, r.top() + 12, r.width() - 42, r.height() - 36);
    p.save();
    p.setPen(QPen(QColor(0xc4, 0xca, 0xd4), 1));
    for (int i = 0; i <= 4; ++i)
        p.drawLine(QPointF(plot.left(), plot.top() + plot.height() * i / 4), QPointF(plot.right(), plot.top() + plot.height() * i / 4));
    p.setPen(kInk);
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    p.drawText(QRectF(r.left(), plot.bottom() + 6, r.width(), 12), Qt::AlignCenter, QStringLiteral("Frequency (Hz)"));
    auto curve = [&](std::function<double(double)> fn, const QColor &col) {
        QPolygonF poly;
        for (int i = 0; i <= 160; ++i) {
            const double t = i / 160.0;
            poly << QPointF(plot.left() + plot.width() * t, plot.bottom() - plot.height() * std::clamp(fn(t), 0.03, 0.97));
        }
        p.setPen(QPen(col, 1.8));
        p.drawPolyline(poly);
    };
    auto bump = [](double t, double c, double w) { return std::exp(-((t - c) / w) * ((t - c) / w)); };
    curve([&](double t) { return 0.30 + 0.45 * t + 0.05 * t * std::sin(24 * t); }, QColor(0x8b, 0x5c, 0xf6));
    curve([&](double t) { return 0.26 + 0.45 * t * t + 0.20 * bump(t, 0.22, 0.05) - 0.18 * bump(t, 0.50, 0.10); }, QColor(0xe9, 0x3f, 0x7d));
    curve([&](double t) { return 0.18 + 0.06 * t + 0.40 * t * t * t; }, QColor(0x14, 0xa0, 0x8a));
    p.setPen(QPen(kDash, 1, Qt::DashLine));
    p.drawLine(QPointF(plot.left(), plot.top() + plot.height() * 0.58), QPointF(plot.right(), plot.top() + plot.height() * 0.58));
    p.restore();
}

void drawCrystal(QPainter &p, const QRectF &r)
{
    p.save();
    const QRectF body(r.center().x() - r.width() * 0.2, r.top() + r.height() * 0.18, r.width() * 0.4, r.height() * 0.5);
    p.setPen(QPen(QColor(0x7a, 0x82, 0x90), 1.2));
    p.setBrush(QColor(0xc9, 0xcf, 0xd8));
    p.drawRoundedRect(body, 4, 4);
    p.setBrush(QColor(0xe6, 0xea, 0xf0));
    p.drawRoundedRect(body.adjusted(5, 5, -5, -5), 3, 3);
    p.setPen(QPen(QColor(0x7a, 0x82, 0x90), 2));
    p.drawLine(QPointF(body.center().x() - body.width() * 0.25, body.bottom()), QPointF(body.center().x() - body.width() * 0.25, r.bottom() - 8));
    p.drawLine(QPointF(body.center().x() + body.width() * 0.25, body.bottom()), QPointF(body.center().x() + body.width() * 0.25, r.bottom() - 8));
    p.restore();
}

void drawThermalChip(QPainter &p, const QRectF &r)
{
    p.save();
    const QRectF board(r.left() + 12, r.bottom() - r.height() * 0.28, r.width() - 24, r.height() * 0.18);
    p.setPen(QPen(kBoardDark, 1));
    p.setBrush(kBoard);
    p.drawRect(board);
    const QRectF body(r.center().x() - r.width() * 0.22, r.top() + r.height() * 0.28, r.width() * 0.44, r.height() * 0.28);
    p.setPen(QPen(QColor(0x30, 0x34, 0x3c), 1));
    p.setBrush(QColor(0x3b, 0x40, 0x4a));
    p.drawRoundedRect(body, 4, 4);
    p.setPen(QPen(QColor(0xb0, 0xb6, 0xc0), 2));
    p.drawLine(QPointF(body.left(), body.bottom() - 4), QPointF(body.left() - 14, board.top()));
    p.drawLine(QPointF(body.right(), body.bottom() - 4), QPointF(body.right() + 14, board.top()));
    p.setBrush(QColor(0xf0, 0x8a, 0x2a));
    p.setPen(Qt::NoPen);
    p.drawEllipse(body.center(), 9, 6);
    p.restore();
    bubble(p, QPointF(body.center().x() + 24, r.top() + r.height() * 0.16), QStringLiteral("TJ"));
}

void drawHeatSink(QPainter &p, const QRectF &r)
{
    p.save();
    const QRectF base(r.center().x() - r.width() * 0.22, r.bottom() - r.height() * 0.28, r.width() * 0.44, r.height() * 0.14);
    p.setPen(QPen(QColor(0x45, 0x4b, 0x56), 1));
    p.setBrush(QColor(0x6d, 0x74, 0x80));
    p.drawRect(base);
    for (int i = 0; i < 6; ++i) {
        const double x = base.left() + 4 + i * (base.width() - 12) / 5.0;
        p.drawRect(QRectF(x, r.top() + r.height() * 0.12, 6, base.top() - r.top() - r.height() * 0.12));
    }
    p.restore();
}

void drawFusing(QPainter &p, const QRectF &r)
{
    p.save();
    const QRectF board = r.adjusted(12, 12, -12, -12);
    p.setPen(Qt::NoPen);
    p.setBrush(kBoard);
    p.drawRoundedRect(board, 8, 8);
    p.setBrush(kCopper);
    const double y = board.center().y();
    p.drawRect(QRectF(board.left(), y - 5, board.width() * 0.42, 10));
    p.drawRect(QRectF(board.right() - board.width() * 0.42, y - 5, board.width() * 0.42, 10));
    QRadialGradient g(board.center(), 26);
    g.setColorAt(0, QColor(0xff, 0xd2, 0x4d));
    g.setColorAt(0.5, QColor(0xf0, 0x6a, 0x1a, 200));
    g.setColorAt(1, QColor(0xf0, 0x6a, 0x1a, 0));
    p.setBrush(g);
    p.drawEllipse(board.center(), 26, 18);
    p.restore();
}

void drawEmbeddedResistor(QPainter &p, const QRectF &r)
{
    p.save();
    const QRectF board = r.adjusted(10, 10, -10, -10);
    p.setPen(Qt::NoPen);
    p.setBrush(kBoard);
    p.drawRoundedRect(board, 8, 8);
    const QRectF res(board.left() + board.width() * 0.2, board.center().y() - board.height() * 0.24, board.width() * 0.6, board.height() * 0.48);
    p.setBrush(kCopper);
    p.drawRect(QRectF(res.left() - 12, res.top() - 6, 24, res.height() + 12));
    p.drawRect(QRectF(res.right() - 12, res.top() - 6, 24, res.height() + 12));
    p.setBrush(QColor(0xc8, 0x8a, 0x66));
    p.drawRect(QRectF(res.left() + 4, res.top(), 10, res.height()));
    p.drawRect(QRectF(res.right() - 14, res.top(), 10, res.height()));
    p.setBrush(QColor(0xc9, 0xcc, 0xd2));
    p.drawRect(QRectF(res.left() + 14, res.top(), res.width() - 28, res.height()));
    p.restore();
    dimH(p, res.left() + 4, res.right() - 4, res.top() - 14, QStringLiteral("L"));
    dimV(p, res.right() + 22, res.top(), res.bottom(), QStringLiteral("W"));
}

void drawReactance(QPainter &p, const QRectF &r)
{
    p.save();
    p.setPen(QPen(kInk, 2));
    const double yc = r.center().y();
    // конденсатор
    const double cx = r.left() + r.width() * 0.28;
    p.drawLine(QPointF(cx - 30, yc), QPointF(cx - 6, yc));
    p.drawLine(QPointF(cx + 6, yc), QPointF(cx + 30, yc));
    p.drawLine(QPointF(cx - 6, yc - 18), QPointF(cx - 6, yc + 18));
    p.drawLine(QPointF(cx + 6, yc - 18), QPointF(cx + 6, yc + 18));
    // катушка
    const double lx = r.left() + r.width() * 0.68;
    QPainterPath coil;
    coil.moveTo(lx - 36, yc);
    for (int i = 0; i < 4; ++i)
        coil.arcTo(QRectF(lx - 36 + i * 18, yc - 9, 18, 18), 180, -180);
    p.setBrush(Qt::NoBrush);
    p.drawPath(coil);
    p.restore();
}

void drawSpacing(QPainter &p, const QRectF &r)
{
    const double top = r.top() + r.height() * 0.38;
    const QRectF board = drawBoardCut(p, r, top, r.height() * 0.38, false);
    const double tw = board.width() * 0.26, gap = board.width() * 0.14;
    const double cx = board.center().x();
    trapezoid(p, cx - (tw + gap) / 2, top, tw, 8, kCopper);
    trapezoid(p, cx + (tw + gap) / 2, top, tw, 8, kCopper);
    dimH(p, cx - gap / 2, cx + gap / 2, top - 20, QStringLiteral("s"));
}

void drawOhmWheel(QPainter &p, const QRectF &r)
{
    const double d = std::min(r.width(), r.height()) * 0.98;
    const QPointF c = r.center();
    const double R = d / 2, rin = R * 0.40, rmid = R * 0.60;
    static const char *const kFormulas[12] = {
        "I = √(P/R)", "E = √(P×R)", "E = P/I", "E = I×R", "R = E/I", "R = E²/P",
        "R = P/I²", "P = E²/R", "P = I²×R", "P = E×I", "I = E/R", "I = P/E" };
    static const QColor kRing[4] = { QColor(0xf8, 0xe0, 0xe6), QColor(0xf6, 0xe7, 0xc8), QColor(0xe4, 0xe0, 0xf3), QColor(0xd9, 0xee, 0xd9) };
    p.save();
    QFont f = p.font();
    f.setPixelSize(std::max(7, int(R * 0.085)));
    f.setBold(true);
    p.setFont(f);
    for (int i = 0; i < 12; ++i) {                                   // кольцо формул: 12 секторов по 30°
        const double a0 = 90 - i * 30;
        QPainterPath seg;
        seg.arcMoveTo(QRectF(c.x() - R, c.y() - R, R * 2, R * 2), a0);
        seg.arcTo(QRectF(c.x() - R, c.y() - R, R * 2, R * 2), a0, -30);
        seg.arcTo(QRectF(c.x() - rmid, c.y() - rmid, rmid * 2, rmid * 2), a0 - 30, 30);
        seg.closeSubpath();
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(kRing[i / 3]);
        p.drawPath(seg);
        const double am = (a0 - 15) * 3.14159265 / 180.0, rr = (R + rmid) / 2;
        const QPointF tp(c.x() + rr * std::cos(am), c.y() - rr * std::sin(am));
        p.setPen(kInk);
        p.drawText(QRectF(tp.x() - 34, tp.y() - 8, 68, 16), Qt::AlignCenter, QString::fromUtf8(kFormulas[i]));
    }
    static const char *const kLetters[4] = { "I", "E", "R", "P" };
    static const QColor kQuad[4] = { QColor(0x5b, 0x5f, 0x9a), QColor(0xd0, 0x8a, 0x3a), QColor(0x7a, 0x62, 0xc8), QColor(0x3f, 0x8f, 0x5a) };
    f.setPixelSize(std::max(10, int(R * 0.22)));
    p.setFont(f);
    for (int i = 0; i < 4; ++i) {                                    // центр: четыре величины
        const double a0 = 90 - i * 90;
        QPainterPath q;
        q.moveTo(c);
        q.arcTo(QRectF(c.x() - rmid, c.y() - rmid, rmid * 2, rmid * 2), a0, -90);
        q.closeSubpath();
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(kQuad[i]);
        p.drawPath(q);
        const double am = (a0 - 45) * 3.14159265 / 180.0;
        p.setPen(Qt::white);
        p.drawText(QRectF(c.x() + rin * 0.9 * std::cos(am) - 14, c.y() - rin * 0.9 * std::sin(am) - 14, 28, 28), Qt::AlignCenter, QString::fromUtf8(kLetters[i]));
    }
    p.restore();
}

} // namespace

PcbDiagram::PcbDiagram(Kind kind, QWidget *parent)
    : QWidget(parent), m_kind(kind)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void PcbDiagram::setKind(Kind kind)
{
    m_kind = kind;
    update();
}

void PcbDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(kCard);
    p.drawRoundedRect(r, 10, 10);
    QFont f = font();
    f.setPixelSize(11);
    p.setFont(f);
    const QRectF in = r.adjusted(6, 6, -6, -6);
    switch (m_kind) {
    case None: break;
    case RiseTime: drawRiseTime(p, in); break;
    case Period: drawPeriod(p, in); break;
    case Microstrip: drawMicrostrip(p, in, 1, false); break;
    case MicrostripEmbed: drawMicrostrip(p, in, 1, true); break;
    case DiffPair: drawMicrostrip(p, in, 2, false); break;
    case Stripline: drawStripline(p, in, 0); break;
    case StriplineAsym: drawStripline(p, in, 1); break;
    case DualStripline: drawStripline(p, in, 2); break;
    case Coplanar: drawCoplanar(p, in); break;
    case Via: drawVia(p, in); break;
    case Planar: drawPlanar(p, in); break;
    case Padstack: drawPadstack(p, in); break;
    case Pdn: drawPdn(p, in); break;
    case Crystal: drawCrystal(p, in); break;
    case ThermalChip: drawThermalChip(p, in); break;
    case HeatSink: drawHeatSink(p, in); break;
    case Fusing: drawFusing(p, in); break;
    case EmbeddedResistor: drawEmbeddedResistor(p, in); break;
    case Reactance: drawReactance(p, in); break;
    case Spacing: drawSpacing(p, in); break;
    case LedBias: break;
    case OhmWheel: drawOhmWheel(p, in); break;
    }
}
