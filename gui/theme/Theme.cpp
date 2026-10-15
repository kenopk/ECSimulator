#include "theme/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>
#include <QPalette>
#include <QSettings>
#include <QStyle>

#include <algorithm>
#include <cmath>

namespace {

struct Preset {
    const char *name;
    const char *bg, *panel, *alt, *border;
};

const Preset kPresets[] = {
    { "Полночь", "#0e1118", "#151922", "#1d2330", "#2a3145" },
    { "Графит",  "#131415", "#1a1b1d", "#242629", "#33363b" },
    { "Океан",   "#0a111f", "#101a2d", "#172640", "#243553" },
    { "Чисто чёрный", "#000000", "#0a0a0a", "#161616", "#2b2b2b" },
    { "Чисто белый",  "#ffffff", "#f3f4f6", "#e4e6ea", "#c9ced6" },
};
constexpr int kPresetCount = int(sizeof(kPresets) / sizeof(kPresets[0]));

QColor mix(const QColor &a, const QColor &b, double t)
{
    return QColor::fromRgbF(a.redF()   + (b.redF()   - a.redF())   * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF()  + (b.blueF()  - a.blueF())  * t);
}

bool isLightColor(const QColor &c)
{
    return (c.redF() * 0.299 + c.greenF() * 0.587 + c.blueF() * 0.114) > 0.55;
}

// Файл с галочкой для QCheckBox::indicator:checked (QSS умеет только картинки).
// Имя зависит от цвета, поэтому при смене темы получается новый файл.
QString ensureCheckImage(const QColor &color)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (dir.isEmpty())
        return QString();
    const QString base = dir + QStringLiteral("/govno_check_") + color.name().mid(1);

    auto render = [&](int px, const QString &file) {
        QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = px / 16.0;
        QPen pen(color, 2.1 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({ QPointF(3.7 * s, 8.4 * s), QPointF(6.9 * s, 11.4 * s), QPointF(12.4 * s, 4.9 * s) }));
        p.end();
        return img.save(file, "PNG");
    };

    const QString file1 = base + QStringLiteral(".png");
    if (!QFile::exists(file1) && !render(16, file1))
        return QString();
    const QString file2 = base + QStringLiteral("@2x.png");
    if (!QFile::exists(file2))
        render(32, file2);
    return QDir::fromNativeSeparators(file1);
}

} // namespace

Theme &Theme::instance()
{
    static Theme t;
    return t;
}

Theme::Theme()
{
    rebuild();
}

QStringList Theme::presetNames()
{
    QStringList names;
    for (const Preset &p : kPresets)
        names << QString::fromUtf8(p.name);
    names << QStringLiteral("Свой цвет");
    return names;
}

int Theme::customPresetIndex()
{
    return kPresetCount;
}

void Theme::rebuild()
{
    Palette &p = m_palette;
    m_preset = std::clamp(m_preset, 0, kPresetCount);

    if (m_preset == kPresetCount) {                    // свой цвет фона: остальное выводим из него
        const QColor bg = m_customBg.isValid() ? m_customBg : QColor("#16202e");
        m_light = isLightColor(bg);
        const QColor tint = m_light ? QColor(Qt::black) : QColor(Qt::white);
        p.bg       = bg;
        p.panel    = mix(bg, tint, m_light ? 0.04 : 0.045);
        p.panelAlt = mix(bg, tint, m_light ? 0.09 : 0.10);
        p.border   = mix(bg, tint, m_light ? 0.17 : 0.18);
    } else {
        const Preset &pr = kPresets[m_preset];
        p.bg       = QColor(pr.bg);
        p.panel    = QColor(pr.panel);
        p.panelAlt = QColor(pr.alt);
        p.border   = QColor(pr.border);
        m_light    = isLightColor(p.bg);
    }

    if (m_light) {
        p.gridMinor = QColor(0, 0, 0, 30);
        p.gridMajor = QColor(0, 0, 0, 75);
        p.wire      = QColor("#56627d");
        p.element   = QColor("#1b2233");
        p.junction  = QColor("#1b2233");
        p.openPin   = QColor(214, 70, 55, 210);
        p.accent    = QColor("#2b7de9");
        p.positive  = QColor("#13a35a");
        p.negative  = QColor("#d93a4a");
        p.current   = QColor("#d99a00");
        p.text      = QColor("#161b26");
        p.textDim   = QColor("#6a7385");
    } else {
        p.gridMinor = QColor(255, 255, 255, 26);
        p.gridMajor = QColor(255, 255, 255, 66);
        p.wire      = QColor("#8d99b5");
        p.element   = QColor("#eef2fb");
        p.junction  = QColor("#dfe6f5");
        p.openPin   = QColor(255, 122, 107, 190);
        p.accent    = QColor("#4da3ff");
        p.positive  = QColor("#3ddc84");
        p.negative  = QColor("#ff5d6c");
        p.current   = QColor("#ffd24d");
        p.text      = QColor("#e4e9f5");
        p.textDim   = QColor("#8b95ab");
    }
    p.nearMiss = QColor("#ff1f1f");

    if (m_customAccent.isValid())
        p.accent = m_customAccent;
    const double y = p.accent.redF() * 0.299 + p.accent.greenF() * 0.587 + p.accent.blueF() * 0.114;
    p.onAccent = y > 0.5 ? QColor("#0b1020") : QColor("#ffffff");
}

void Theme::commit()
{
    rebuild();
    save();
    applyToApplication();
    emit changed();
}

void Theme::setPreset(int index)
{
    index = std::clamp(index, 0, kPresetCount);
    if (index == m_preset)
        return;
    m_preset = index;
    commit();
}

void Theme::setCustomBackground(const QColor &color)
{
    if (!color.isValid())
        return;
    m_customBg = color;
    m_preset = kPresetCount;
    commit();
}

void Theme::setCustomAccent(const QColor &color)
{
    if (!color.isValid() || color == m_customAccent)
        return;
    m_customAccent = color;
    commit();
}

void Theme::resetAccent()
{
    if (!m_customAccent.isValid())
        return;
    m_customAccent = QColor();
    commit();
}

void Theme::setGridStyle(GridStyle style)
{
    if (style == m_gridStyle)
        return;
    m_gridStyle = style;
    commit();
}

void Theme::setShowLabels(bool on)
{
    if (on == m_showLabels)
        return;
    m_showLabels = on;
    commit();
}

void Theme::setShowJunctions(bool on)
{
    if (on == m_showJunctions)
        return;
    m_showJunctions = on;
    commit();
}

void Theme::setShowNearMiss(bool on)
{
    if (on == m_showNearMiss)
        return;
    m_showNearMiss = on;
    commit();
}

QColor Theme::voltageColor(double volts) const
{
    const double t = std::clamp(volts / m_voltageScale, -1.0, 1.0);
    if (std::fabs(t) < 0.02)
        return m_palette.wire;
    return mix(m_palette.wire, t > 0 ? m_palette.positive : m_palette.negative, std::fabs(t));
}

void Theme::load()
{
    QSettings s;
    m_preset        = std::clamp(s.value("theme/preset", 0).toInt(), 0, kPresetCount);
    const QColor bg(s.value("theme/customBg", "#16202e").toString());
    if (bg.isValid())
        m_customBg = bg;
    const QString acc = s.value("theme/accent", QString()).toString();
    m_customAccent = acc.isEmpty() ? QColor() : QColor(acc);
    m_gridStyle     = static_cast<GridStyle>(std::clamp(s.value("theme/grid", 0).toInt(), 0, 2));
    m_showLabels    = s.value("theme/labels", true).toBool();
    m_showJunctions = s.value("theme/junctions", true).toBool();
    m_showNearMiss  = s.value("theme/nearMiss", true).toBool();
    rebuild();
}

void Theme::save() const
{
    QSettings s;
    s.setValue("theme/preset", m_preset);
    s.setValue("theme/customBg", m_customBg.name());
    s.setValue("theme/accent", m_customAccent.isValid() ? m_customAccent.name() : QString());
    s.setValue("theme/grid", static_cast<int>(m_gridStyle));
    s.setValue("theme/labels", m_showLabels);
    s.setValue("theme/junctions", m_showJunctions);
    s.setValue("theme/nearMiss", m_showNearMiss);
}

void Theme::applyToApplication()
{
    if (!qApp)
        return;

    if (qApp->style()->objectName().compare(QLatin1String("fusion"), Qt::CaseInsensitive) != 0)
        QApplication::setStyle(QStringLiteral("Fusion"));

    const Palette &c = m_palette;
    QPalette pal;
    pal.setColor(QPalette::Window,          c.bg);
    pal.setColor(QPalette::WindowText,      c.text);
    pal.setColor(QPalette::Base,            c.bg);
    pal.setColor(QPalette::AlternateBase,   c.panel);
    pal.setColor(QPalette::Text,            c.text);
    pal.setColor(QPalette::Button,          c.panelAlt);
    pal.setColor(QPalette::ButtonText,      c.text);
    pal.setColor(QPalette::ToolTipBase,     c.panelAlt);
    pal.setColor(QPalette::ToolTipText,     c.text);
    pal.setColor(QPalette::PlaceholderText, c.textDim);
    pal.setColor(QPalette::Highlight,       c.accent);
    pal.setColor(QPalette::HighlightedText, c.onAccent);
    pal.setColor(QPalette::Disabled, QPalette::Text,       c.textDim);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, c.textDim);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, c.textDim);
    qApp->setPalette(pal);

    QFont f = qApp->font();
    f.setFamily(QStringLiteral("Segoe UI"));
    f.setPointSize(10);
    qApp->setFont(f);

    qApp->setStyleSheet(styleSheet());
}

QString Theme::styleSheet() const
{
    const Palette &c = m_palette;
    QString css = QStringLiteral(R"(
QMainWindow, QDialog { background: @panel@; }
QWidget#startScreen {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 @bg@, stop:1 @panelAlt@);
}
QLabel { background: transparent; }
QLabel#title    { font-size: 30px; font-weight: 700; color: @text@; }
QLabel#subtitle { font-size: 14px; color: @dim@; }
QLabel#hint     { color: @dim@; }
QLabel#panelTitle   { font-size: 14px; font-weight: 600; }
QLabel#sectionTitle { color: @dim@; font-size: 11px; font-weight: 600; letter-spacing: 1px; }
QLabel#valueLabel   { color: @dim@; }

QMenuBar { background: @panel@; padding: 2px 6px; }
QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: 5px; }
QMenuBar::item:selected { background: @alt@; }
QMenu { background: @panel@; border: 1px solid @border@; padding: 4px; icon-size: 24px; }
QMenu::item { padding: 5px 26px 5px 8px; border-radius: 4px; }
QMenu::item:selected { background: @alt@; }
QMenu::item:disabled { color: @dim@; }
QMenu::icon { padding-left: 4px; }
QMenu::separator { height: 1px; background: @border@; margin: 4px 6px; }

QToolBar { background: @panel@; border: none; spacing: 2px; padding: 4px; }
QToolBar::separator { background: @border@; width: 1px; height: 1px; margin: 5px 4px; }
QToolButton { background: transparent; border: none; border-radius: 6px; padding: 5px 8px; color: @text@; }
QToolButton:hover { background: @alt@; }
QToolButton:pressed { background: @accentSoft@; }
QToolButton:checked { background: @accentSoft@; }
QToolButton:disabled { color: @dim@; }
QToolButton::menu-indicator { image: none; }

QTabWidget::pane { border: none; }
QTabBar { background: @panel@; qproperty-drawBase: 0; }
QTabBar::tab { background: transparent; color: @dim@; padding: 7px 16px; margin-right: 1px;
               border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: @text@; border-bottom: 2px solid @accent@; }
QTabBar::tab:hover:!selected { background: @alt@; color: @text@; }
QToolButton#tabClose { border-radius: 4px; padding: 0px 5px 2px 5px; margin-left: 4px; color: @dim@; font-size: 15px; }
QToolButton#tabClose:hover { background: @alt@; color: @text@; }

QStatusBar { background: @panel@; color: @dim@; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @dim@; padding: 0 8px; }

QDockWidget { color: @text@; }
QWidget#panelBody, QWidget#dockRoot { background: @panel@; }
QWidget#simPanel { background: @panel@; }
QFrame#separator { background: @border@; max-height: 1px; min-height: 1px; border: none; }

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: @bg@; border: 1px solid @border@; border-radius: 6px; padding: 4px 8px;
    selection-background-color: @accent@; selection-color: @onAccent@;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid @accent@; }
QLineEdit:read-only { color: @dim@; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @border@; selection-background-color: @alt@; outline: none; }

QPushButton { background: @alt@; border: 1px solid transparent; border-radius: 6px; padding: 7px 14px; color: @text@; }
QPushButton:hover { border: 1px solid @accent@; }
QPushButton:pressed { background: @accentSoft@; }
QPushButton:checked { background: @accent@; color: @onAccent@; font-weight: 600; }
QPushButton[primary="true"] { background: @accent@; color: @onAccent@; border: 1px solid @accent@; font-weight: 600; }
QPushButton[primary="true"]:hover { background: @accentHover@; border: 1px solid @accentHover@; }
QPushButton[menu="true"] { min-width: 260px; font-size: 14px; padding: 12px 20px; border-radius: 9px; }

QCheckBox { spacing: 8px; background: transparent; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid @border@; background: @bg@; }
QCheckBox::indicator:checked { background: @accent@; border: 1px solid @accent@; @checkImage@ }

QSlider::groove:horizontal { height: 3px; background: @border@; border-radius: 1px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 1px; }
QSlider::handle:horizontal { background: @text@; width: 12px; height: 12px; margin: -5px 0; border-radius: 6px; }
QSlider::handle:horizontal:hover { background: @accent@; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @border@; border-radius: 5px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }

QToolTip { background: @alt@; color: @text@; border: 1px solid @border@; padding: 5px 8px; }
QRubberBand { border: 1px solid @accent@; background: @accentFaint@; }
)");

    css.replace(QLatin1String("@bg@"),         c.bg.name());
    css.replace(QLatin1String("@panelAlt@"),   c.panelAlt.name());
    css.replace(QLatin1String("@panel@"),      c.panel.name());
    css.replace(QLatin1String("@alt@"),        c.panelAlt.name());
    css.replace(QLatin1String("@border@"),     c.border.name());
    css.replace(QLatin1String("@text@"),       c.text.name());
    css.replace(QLatin1String("@dim@"),        c.textDim.name());
    auto rgba = [](const QColor &col, int a) {
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(col.red()).arg(col.green()).arg(col.blue()).arg(a);
    };
    const QString check = ensureCheckImage(c.onAccent);
    css.replace(QLatin1String("@checkImage@"), check.isEmpty() ? QString()
                                                               : QStringLiteral("image: url(\"%1\");").arg(check));
    css.replace(QLatin1String("@accentSoft@"), rgba(c.accent, m_light ? 50 : 60));
    css.replace(QLatin1String("@accentFaint@"), rgba(c.accent, 40));
    css.replace(QLatin1String("@accentHover@"), mix(c.accent, m_light ? QColor(Qt::black) : QColor(Qt::white), 0.15).name());
    css.replace(QLatin1String("@onAccent@"),    c.onAccent.name());
    css.replace(QLatin1String("@accent@"),     c.accent.name());
    return css;
}
