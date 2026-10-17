#include "theme/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>

#include <algorithm>
#include <cmath>

namespace {

struct Preset {
    const char *name;
    const char *bg, *panel, *alt, *border;
};

const Preset kPresets[] = {
    // «Лаборатория»: матовый графит #121315, текстолит панелей #1a1c1f, янтарный акцент и «лучевой» зелёный (см. rebuild()).
    { "Лаборатория", "#121315", "#1a1c1f", "#23262b", "#2d3137" },
    { "Графит",  "#131415", "#1a1b1d", "#242629", "#33363b" },
    { "Океан",   "#0a111f", "#101a2d", "#172640", "#243553" },
    { "Чисто чёрный", "#000000", "#0a0a0a", "#161616", "#2b2b2b" },
    { "Чисто белый",  "#ffffff", "#f3f4f6", "#e4e6ea", "#c9ced6" },
};
constexpr int kPresetCount = int(sizeof(kPresets) / sizeof(kPresets[0]));
constexpr int kLabPreset = 0;

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

// Стрелка QComboBox::down-arrow: у стилизованного списка собственной стрелки нет, нужна картинка.
QString ensureArrowImage(const QColor &color)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (dir.isEmpty())
        return QString();
    const QString base = dir + QStringLiteral("/govno_arrow_") + color.name().mid(1);

    auto render = [&](int px, const QString &file) {
        QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = px / 16.0;
        p.setPen(QPen(color, 1.8 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({ QPointF(4 * s, 6 * s), QPointF(8 * s, 10 * s), QPointF(12 * s, 6 * s) }));
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

QStringList Theme::uiFamilies()
{
    return { QStringLiteral("Geist"), QStringLiteral("Inter"), QStringLiteral("Segoe UI"), QStringLiteral("Arial") };
}

QStringList Theme::monoFamilies()
{
    return { QStringLiteral("JetBrains Mono"), QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"),
             QStringLiteral("Courier New") };
}

QFont Theme::uiFont(int pixelSize)
{
    QFont f;
    f.setFamilies(uiFamilies());
    f.setPixelSize(pixelSize);
    return f;
}

QFont Theme::monoFont(int pixelSize)
{
    QFont f;
    f.setFamilies(monoFamilies());
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(pixelSize);
    return f;
}

void Theme::loadBundledFonts()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    const QString base = QCoreApplication::applicationDirPath();
    for (const QString &rel : { QStringLiteral("/fonts"), QStringLiteral("/../fonts"), QStringLiteral("/../../fonts") }) {
        const QDir dir(base + rel);
        if (!dir.exists())
            continue;
        const QStringList files = dir.entryList({ QStringLiteral("*.ttf"), QStringLiteral("*.otf") }, QDir::Files);
        for (const QString &file : files)
            QFontDatabase::addApplicationFont(dir.filePath(file));
    }
}

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
    p.focus = p.accent;
    if (m_preset == kLabPreset && !m_light) {
        // «Цифровая лаборатория»: экран измерительного прибора. Янтарь газоразрядных ламп — для выбора и наведения,
        // лазерно-зелёный — только для живых сигналов (ток, напряжение) и рамки поля ввода в фокусе.
        p.gridMinor = QColor(226, 228, 233, 18);
        p.gridMajor = QColor(226, 228, 233, 46);
        p.wire      = QColor("#8d939c");
        p.element   = QColor("#e2e4e9");
        p.junction  = QColor("#cfd3da");
        p.openPin   = QColor(229, 103, 93, 190);
        p.accent    = QColor("#ff9f0a");
        p.focus     = QColor("#00e676");
        p.positive  = QColor("#00e676");
        p.negative  = QColor("#e5675d");
        p.current   = QColor("#00e676");
        p.text      = QColor("#e2e4e9");
        p.textDim   = QColor("#8b9099");
    }
    p.nearMiss = QColor("#ff1f1f");

    if (m_customAccent.isValid()) {
        p.accent = m_customAccent;
        p.focus = m_customAccent;
    }
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

    // Заголовки окон (в том числе оторванных вкладок) — в цвет темы.
    if (QStyleHints *hints = QGuiApplication::styleHints())
        hints->setColorScheme(m_light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);

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

    loadBundledFonts();
    QFont f = qApp->font();
    f.setFamilies(uiFamilies());
    f.setPointSize(10);
    qApp->setFont(f);
    // Платформенная тема Windows задаёт отдельные шрифты меню, подсказок, диалогов, заголовков таблиц и т. п. —
    // перекрываем их, чтобы Geist стоял действительно везде.
    static const char *const kClasses[] = {
        "QMenu", "QMenuBar", "QMessageBox", "QStatusBar", "QTipLabel", "QToolTip", "QHeaderView", "QLabel",
        "QPushButton", "QToolButton", "QCheckBox", "QRadioButton", "QComboBox", "QLineEdit", "QSpinBox",
        "QDoubleSpinBox", "QTextEdit", "QPlainTextEdit", "QListView", "QListWidget", "QTreeView", "QTableView",
        "QTabBar", "QTabWidget", "QDockWidget", "QGroupBox", "QDialog", "QSlider", "QSplitter", "QAbstractItemView",
        "QAbstractButton", "QToolBar", "QWidget"
    };
    for (const char *cls : kClasses)
        QApplication::setFont(f, cls);

    qApp->setStyleSheet(styleSheet());
}

QString Theme::styleSheet() const
{
    const Palette &c = m_palette;
    // Плоские блоки, линии в 1 px, небольшие скругления, никаких теней и градиентов.
    // Числа, номиналы и показания — моноширинным шрифтом; интервалы заданы явно, без «кучности» Qt.
    QString css = QStringLiteral(R"(
* { outline: none; }
QMainWindow, QDialog { background: @panel@; }
QWidget#startScreen { background: @bg@; }
QLabel { background: transparent; padding: 1px 0; }
QLabel#title    { font-size: 30px; font-weight: 600; color: @text@; padding: 2px 0; }
QLabel#subtitle { font-size: 14px; color: @dim@; padding: 2px 0 6px 0; }
QLabel#hint     { color: @dim@; }
QLabel#panelTitle   { font-size: 14px; font-weight: 600; }
QLabel#sectionTitle { color: @dim@; font-size: 11px; font-weight: 600; letter-spacing: 1px; padding: 2px 0 4px 0; }
QLabel#valueLabel   { color: @dim@; font-family: @mono@; }

QMenuBar { background: @panel@; padding: 4px 8px; border-bottom: 1px solid @border@; }
QMenuBar::item { background: transparent; padding: 6px 12px; border-radius: 4px; color: @text@; }
QMenuBar::item:selected { background: @alt@; }
QMenuBar::item:pressed { background: @accentSoft@; }
QMenu { background: @panel@; border: 1px solid @border@; padding: 4px; icon-size: 16px; font-size: 12px; }
QMenu::item { padding: 5px 22px 5px 8px; border-radius: 3px; color: @text@; }
QMenu::item:selected { background: @alt@; }
QMenu::item:disabled { color: @dim@; }
QMenu::icon { padding-left: 4px; }
QMenu::separator { height: 1px; background: @border@; margin: 4px 6px; }

QToolBar { background: @panel@; border: none; border-right: 1px solid @border@; spacing: 4px; padding: 8px 6px; }
QToolBar::separator { background: @border@; width: 1px; height: 1px; margin: 8px 6px; }
QToolButton { background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 7px 9px; color: @text@; }
QToolButton:hover { background: @alt@; border-color: @border@; }
QToolButton:pressed { background: @accentSoft@; border-color: @accent@; }
QToolButton:checked { background: @accentSoft@; border-color: @accentLine@; }
QToolButton:focus { border-color: @accentLine@; }
QToolButton:disabled { color: @dim@; }
QToolButton::menu-indicator { image: none; }

QTabWidget::pane { border: none; border-top: 1px solid @border@; }
QTabBar { background: @panel@; qproperty-drawBase: 0; }
QTabBar::tab { background: transparent; color: @dim@; padding: 9px 18px; margin-right: 2px;
               border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: @text@; border-bottom: 2px solid @accent@; }
QTabBar::tab:hover:!selected { background: @alt@; color: @text@; }
QToolButton#sectionHead { padding: 4px 6px; }
QToolButton#sectionHead:checked { background: transparent; border-color: transparent; }
QToolButton#sectionHead:hover { background: @alt@; }
QToolButton#tabClose { border: none; border-radius: 3px; padding: 0px 6px 2px 6px; margin-left: 6px; color: @dim@; font-size: 15px; }
QToolButton#tabClose:hover { background: @alt@; color: @text@; }
QToolButton#tabClose:pressed { background: @accentSoft@; }

QStatusBar { background: @panel@; color: @dim@; border-top: 1px solid @border@; padding: 2px 6px; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @dim@; padding: 0 10px; font-family: @mono@; font-size: 11px; }

QDockWidget { color: @text@; }
QDockWidget::title { background: @panel@; padding: 9px 14px; border-bottom: 1px solid @border@; text-align: left;
                     font-size: 11px; font-weight: 600; }
QWidget#panelBody, QWidget#dockRoot { background: @panel@; }
QWidget#simPanel { background: @panel@; }
QScrollArea#dockScroll, QScrollArea#dockScroll > QWidget > QWidget { background: @panel@; border: none; }
QListWidget#issueList { background: @bg@; border: 1px solid @border@; border-radius: 4px; padding: 4px;
                        font-family: @mono@; font-size: 12px; }
QListWidget#issueList::item { padding: 5px 6px; border-bottom: 1px solid @border@; }
QListWidget#issueList::item:selected { background: @alt@; }
QLabel#simStatus { font-family: @mono@; font-size: 12px; padding: 4px 0; }
QLabel#simStatus[state="idle"] { color: @dim@; }
QLabel#simStatus[state="running"] { color: @positive@; }
QLabel#simStatus[state="error"] { color: @negative@; font-weight: 600; }
QPushButton#bannerClose { padding: 4px 0; }
QFrame#separator { background: @border@; max-height: 1px; min-height: 1px; border: none; }
QSplitter::handle { background: @border@; }
QMainWindow::separator { background: @border@; width: 1px; height: 1px; }

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {
    background: @bg@; color: @text@; border: 1px solid @border@; border-radius: 4px; padding: 4px 8px; min-height: 20px;
    selection-background-color: @accent@; selection-color: @onAccent@;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit { font-family: @mono@; }
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QTextEdit:hover, QPlainTextEdit:hover { border-color: @borderHover@; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus, QPlainTextEdit:focus { border: 1px solid @focus@; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: @dim@; background: @panel@; }
QLineEdit:read-only { color: @dim@; }
QComboBox { padding-right: 26px; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { @arrowImage@ width: 10px; height: 10px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @border@; selection-background-color: @alt@;
                              selection-color: @text@; padding: 3px; outline: none; }

QPushButton { background: transparent; border: 1px solid @border@; border-radius: 4px; padding: 6px 14px; min-height: 18px; color: @text@; }
QPushButton:hover { background: transparent; border-color: @accent@; }
QPushButton:pressed { background: @accentSoft@; border-color: @accent@; }
QPushButton:focus { border-color: @accent@; }
QPushButton:checked { background: @accent@; border-color: @accent@; color: @onAccent@; font-weight: 600; }
QPushButton:checked:hover { background: @accentHover@; border-color: @accentHover@; }
QPushButton:disabled { color: @dim@; background: @panel@; }
QPushButton[primary="true"] { background: @accent@; color: @onAccent@; border: 1px solid @accent@; font-weight: 600; }
QPushButton[primary="true"]:hover { background: @accentHover@; border-color: @accentHover@; }
QPushButton[primary="true"]:pressed { background: @accent@; border-color: @accent@; }
QPushButton[menu="true"] { min-width: 280px; font-size: 14px; padding: 14px 24px; border-radius: 6px; }

QCheckBox, QRadioButton { spacing: 10px; background: transparent; padding: 3px 0; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid @border@; background: @bg@; }
QCheckBox::indicator:hover { border-color: @accentLine@; }
QCheckBox::indicator:checked { background: @accent@; border: 1px solid @accent@; @checkImage@ }
QRadioButton::indicator { width: 14px; height: 14px; border-radius: 8px; border: 1px solid @border@; background: @bg@; }
QRadioButton::indicator:hover { border-color: @accentLine@; }
QRadioButton::indicator:checked { width: 8px; height: 8px; border: 4px solid @accent@; background: @bg@; }
QCheckBox:disabled, QRadioButton:disabled { color: @dim@; }

QSlider::groove:horizontal { height: 3px; background: @border@; border-radius: 1px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 1px; }
QSlider::handle:horizontal { background: @text@; width: 12px; height: 12px; margin: -5px 0; border-radius: 6px; }
QSlider::handle:horizontal:hover { background: @accent@; }
QSlider::handle:horizontal:pressed { background: @accentHover@; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @border@; border-radius: 3px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: @borderHover@; }
QScrollBar::handle:vertical:pressed { background: @accentLine@; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border@; border-radius: 3px; min-width: 28px; }
QScrollBar::handle:horizontal:hover { background: @borderHover@; }
QScrollBar::handle:horizontal:pressed { background: @accentLine@; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip { background: @alt@; color: @text@; border: 1px solid @border@; padding: 6px 10px; }
QRubberBand { border: 1px solid @accent@; background: @accentFaint@; }
)");

    css.replace(QLatin1String("@bg@"),         c.bg.name());
    css.replace(QLatin1String("@panelAlt@"),   c.panelAlt.name());
    css.replace(QLatin1String("@panel@"),      c.panel.name());
    css.replace(QLatin1String("@alt@"),        c.panelAlt.name());
    css.replace(QLatin1String("@border@"),     c.border.name());
    css.replace(QLatin1String("@text@"),       c.text.name());
    css.replace(QLatin1String("@positive@"),   c.positive.name());
    css.replace(QLatin1String("@negative@"),   c.negative.name());
    css.replace(QLatin1String("@dim@"),        c.textDim.name());
    auto rgba = [](const QColor &col, int a) {
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(col.red()).arg(col.green()).arg(col.blue()).arg(a);
    };
    const QString check = ensureCheckImage(c.onAccent);
    css.replace(QLatin1String("@checkImage@"), check.isEmpty() ? QString()
                                                               : QStringLiteral("image: url(\"%1\");").arg(check));
    const QColor tint = m_light ? QColor(Qt::black) : QColor(Qt::white);
    css.replace(QLatin1String("@altHover@"),    mix(c.panelAlt, tint, 0.05).name());
    css.replace(QLatin1String("@borderHover@"), mix(c.border, tint, 0.22).name());
    css.replace(QLatin1String("@accentLine@"),  rgba(c.accent, 150));
    css.replace(QLatin1String("@mono@"), QStringLiteral("\"JetBrains Mono\", \"Cascadia Mono\", Consolas, \"Courier New\", monospace"));
    css.replace(QLatin1String("@focus@"), c.focus.name());
    const QString arrow = ensureArrowImage(c.textDim);
    css.replace(QLatin1String("@arrowImage@"), arrow.isEmpty() ? QString()
                                                               : QStringLiteral("image: url(\"%1\");").arg(arrow));
    css.replace(QLatin1String("@accentSoft@"), rgba(c.accent, m_light ? 50 : 60));
    css.replace(QLatin1String("@accentFaint@"), rgba(c.accent, 40));
    css.replace(QLatin1String("@accentHover@"), mix(c.accent, m_light ? QColor(Qt::black) : QColor(Qt::white), 0.15).name());
    css.replace(QLatin1String("@onAccent@"),    c.onAccent.name());
    css.replace(QLatin1String("@accent@"),     c.accent.name());
    return css;
}
