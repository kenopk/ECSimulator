#include "tools/KarnaughWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QThreadPool>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

#include "theme/Theme.h"

namespace {

constexpr int kMaxSide = 1024;      // технический предел одной стороны карты

// Цвета карты берём из темы приложения: фон, подписи (панель), линии (рамка), подсветка искомых клеток (акцент).
QColor pageColor()   { return Theme::instance().palette().bg; }
QColor headerColor() { return Theme::instance().palette().panel; }
QColor lineColor()   { return Theme::instance().palette().border; }
QColor textColor()   { return Theme::instance().palette().text; }
QColor hitColor()
{
    QColor c = Theme::instance().palette().accent;
    c.setAlpha(60);                          // клетки, входящие в результат
    return c;
}

quint8 nextValue(quint8 v, bool forward)
{
    // 0 → 1 → X → 0
    return forward ? quint8((v + 1) % 3) : quint8((v + 2) % 3);
}

QString cellText(quint8 v)
{
    switch (v) {
    case Karnaugh::One:       return QStringLiteral("1");
    case Karnaugh::DontCare:  return QStringLiteral("X");
    default:                  return QStringLiteral("0");
    }
}

// Имена слитно (AB), а если есть длинные — через запятую (Ra,Rb).
QString joinNames(const QStringList &names)
{
    for (const QString &n : names)
        if (n.size() > 1)
            return names.join(QLatin1Char(','));
    return names.join(QString());
}

} // namespace

// ================================================================ KarnaughCanvas

KarnaughCanvas::KarnaughCanvas(QWidget *parent)
    : QWidget(parent)
{
    m_cells.fill(Karnaugh::Zero, m_w * m_h);
    m_names = Karnaugh::variableNames(Karnaugh::bitsFor(m_h) + Karnaugh::bitsFor(m_w));
    setCursor(Qt::PointingHandCursor);
    updateGeometryHints();
}

void KarnaughCanvas::resizeMap(int width, int height)
{
    width = std::clamp(width, 1, kMaxSide);
    height = std::clamp(height, 1, kMaxSide);
    if (width == m_w && height == m_h)
        return;

    QVector<quint8> cells(qsizetype(width) * height, Karnaugh::Zero);
    for (int r = 0; r < std::min(height, m_h); ++r)
        for (int c = 0; c < std::min(width, m_w); ++c)
            cells[qsizetype(r) * width + c] = m_cells[qsizetype(r) * m_w + c];

    m_w = width;
    m_h = height;
    m_cells = cells;
    updateGeometryHints();
    update();
    emit geometryChanged();
    emit changed();
}

void KarnaughCanvas::setCells(const QVector<quint8> &cells)
{
    if (cells.size() != m_cells.size() || cells == m_cells)
        return;
    m_cells = cells;
    update();
    emit changed();
}

void KarnaughCanvas::fill(quint8 value)
{
    m_cells.fill(value);
    update();
    emit changed();
}

void KarnaughCanvas::setVarNames(const QStringList &names)
{
    if (names == m_names)
        return;
    m_names = names;
    updateGeometryHints();
    update();
    emit geometryChanged();
}

void KarnaughCanvas::setTarget(quint8 value)
{
    if (value == m_target)
        return;
    m_target = value;
    update();
}

void KarnaughCanvas::updateGeometryHints()
{
    QFont bold = font();
    bold.setPixelSize(18);
    bold.setBold(true);
    const QFontMetrics fm(bold);

    const int rowBits = Karnaugh::bitsFor(m_h);
    const QString rowSample = QString(std::max(rowBits, 1), QLatin1Char('0'));
    m_left = std::max(56, fm.horizontalAdvance(rowSample) + 28);

    QFont small = font();
    small.setPixelSize(12);
    const QFontMetrics sm(small);
    const int cornerNeed = sm.horizontalAdvance(joinNames(m_names.mid(0, rowBits)))
                         + sm.horizontalAdvance(joinNames(m_names.mid(rowBits))) + 18;
    m_left = std::max(m_left, cornerNeed);
    m_top = 56;
    setFixedSize(m_left + m_w * kCellW + 1, m_top + m_h * kCellH + 1);
}

QPoint KarnaughCanvas::cellAt(const QPoint &pos) const
{
    const int x = pos.x() - m_left;
    const int y = pos.y() - m_top;
    if (x < 0 || y < 0)
        return QPoint(-1, -1);
    const int c = x / kCellW;
    const int r = y / kCellH;
    if (c >= m_w || r >= m_h)
        return QPoint(-1, -1);
    return QPoint(c, r);
}

void KarnaughCanvas::setCell(const QPoint &c, quint8 value)
{
    quint8 &cell = m_cells[qsizetype(c.y()) * m_w + c.x()];
    if (cell == value)
        return;
    cell = value;
    update(m_left + c.x() * kCellW, m_top + c.y() * kCellH, kCellW + 1, kCellH + 1);
    emit changed();
}

void KarnaughCanvas::mousePressEvent(QMouseEvent *e)
{
    const QPoint c = cellAt(e->pos());
    if (c.x() < 0 || (e->button() != Qt::LeftButton && e->button() != Qt::RightButton)) {
        QWidget::mousePressEvent(e);
        return;
    }
    const quint8 cur = m_cells[qsizetype(c.y()) * m_w + c.x()];
    m_paintValue = nextValue(cur, e->button() == Qt::LeftButton);
    m_painting = true;
    setCell(c, m_paintValue);
}

void KarnaughCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_painting)
        return;
    const QPoint c = cellAt(e->pos());
    if (c.x() >= 0)
        setCell(c, m_paintValue);
}

void KarnaughCanvas::mouseReleaseEvent(QMouseEvent *)
{
    m_painting = false;
}

void KarnaughCanvas::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(e->rect(), pageColor());

    const int rowBits = Karnaugh::bitsFor(m_h);
    const int colBits = Karnaugh::bitsFor(m_w);

    QFont normal = Theme::monoFont(17);                      // значения клеток и коды Грея — моноширинным
    QFont bold = normal;
    bold.setBold(true);
    QFont small = font();
    small.setPixelSize(12);
    small.setBold(true);

    // подписи (фон gray-900)
    p.fillRect(0, 0, m_left, m_top, headerColor());
    p.fillRect(m_left, 0, m_w * kCellW, m_top, headerColor());
    p.fillRect(0, m_top, m_left, m_h * kCellH, headerColor());

    // угол: диагональ, имена строк слева внизу, столбцов справа вверху
    p.setPen(textColor());
    p.drawLine(6, 6, m_left - 6, m_top - 6);
    p.setFont(small);
    p.drawText(QRect(6, m_top - 24, m_left - 12, 20), Qt::AlignLeft | Qt::AlignBottom,
               joinNames(m_names.mid(0, rowBits)));
    p.drawText(QRect(6, 4, m_left - 12, 20), Qt::AlignRight | Qt::AlignTop,
               joinNames(m_names.mid(rowBits)));

    const int c0 = std::max(0, (e->rect().left() - m_left) / kCellW);
    const int c1 = std::min(m_w - 1, (e->rect().right() - m_left) / kCellW);
    const int r0 = std::max(0, (e->rect().top() - m_top) / kCellH);
    const int r1 = std::min(m_h - 1, (e->rect().bottom() - m_top) / kCellH);

    p.setFont(bold);
    for (int c = c0; c <= c1; ++c)
        p.drawText(QRect(m_left + c * kCellW, 0, kCellW, m_top), Qt::AlignCenter,
                   Karnaugh::grayLabel(c, colBits));
    for (int r = r0; r <= r1; ++r)
        p.drawText(QRect(0, m_top + r * kCellH, m_left, kCellH), Qt::AlignCenter,
                   Karnaugh::grayLabel(r, rowBits));

    // клетки
    p.setFont(normal);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const QRect rc(m_left + c * kCellW, m_top + r * kCellH, kCellW, kCellH);
            const quint8 v = m_cells[qsizetype(r) * m_w + c];
            if (v == m_target || v == Karnaugh::DontCare)
                p.fillRect(rc, hitColor());
            p.setPen(textColor());
            p.drawText(rc, Qt::AlignCenter, cellText(v));
        }
    }

    // сетка
    p.setPen(QPen(lineColor(), 1));
    for (int c = 0; c <= m_w; ++c) {
        const int x = m_left + c * kCellW;
        if (x >= e->rect().left() - 1 && x <= e->rect().right() + 1)
            p.drawLine(x, 0, x, m_top + m_h * kCellH);
    }
    for (int r = 0; r <= m_h; ++r) {
        const int y = m_top + r * kCellH;
        if (y >= e->rect().top() - 1 && y <= e->rect().bottom() + 1)
            p.drawLine(0, y, m_left + m_w * kCellW, y);
    }
    p.drawLine(0, 0, m_left + m_w * kCellW, 0);
    p.drawLine(0, 0, 0, m_top + m_h * kCellH);
    p.drawLine(m_left, 0, m_left, m_top + m_h * kCellH);
    p.drawLine(0, m_top, m_left + m_w * kCellW, m_top);
}

// ================================================================ KarnaughWidget

namespace {

const char *kStyle = R"(
#karnaughPage, #kContent, #kTableHost { background: #1f2937; }
#karnaughPage QScrollArea { background: #1f2937; border: none; }
#karnaughPage QLabel { background: transparent; color: #ffffff; font-size: 16px; }
#karnaughPage QLabel#kTitle { font-size: 30px; }
#karnaughPage QLabel#kStatus { color: #9ca3af; font-size: 14px; }
#karnaughPage QLineEdit, #karnaughPage QTextEdit {
    background: #111827; color: #ffffff; border: 2px solid transparent; border-radius: 6px;
    padding: 6px 12px; font-size: 17px; selection-background-color: #374151;
}
#karnaughPage QLineEdit:focus, #karnaughPage QTextEdit:focus { border: 2px solid #3b82f6; }
#karnaughPage QPushButton {
    background: #111827; color: #ffffff; border: none; border-radius: 6px; padding: 8px 16px; font-size: 16px;
}
#karnaughPage QPushButton:hover { background: #030712; }
)";

QLabel *makeLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

// Обычный текст формулы -> HTML: ¬X превращается в X с чертой сверху.
QString richFormula(const QString &plain, const QStringList &names)
{
    QStringList sorted = names;
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });

    QString out;
    for (int i = 0; i < plain.size();) {
        if (plain[i] == QChar(0x00AC)) {                       // «¬»
            bool matched = false;
            for (const QString &n : std::as_const(sorted)) {
                if (!n.isEmpty() && plain.mid(i + 1, n.size()) == n) {
                    out += QStringLiteral("<span style=\"text-decoration: overline;\">%1</span>")
                               .arg(n.toHtmlEscaped());
                    i += 1 + n.size();
                    matched = true;
                    break;
                }
            }
            if (matched)
                continue;
        }
        out += QString(plain[i]).toHtmlEscaped();
        ++i;
    }
    return out;
}

} // namespace

KarnaughWidget::KarnaughWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("karnaughPage"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto applyStyle = [this] {
        const Palette &pal = Theme::instance().palette();
        QString css = QString::fromUtf8(kStyle);
        css.replace(QLatin1String("#1f2937"), pal.bg.name());
        css.replace(QLatin1String("#111827"), pal.panel.name());
        css.replace(QLatin1String("#030712"), pal.panelAlt.name());
        css.replace(QLatin1String("#374151"), pal.border.name());
        css.replace(QLatin1String("#9ca3af"), pal.textDim.name());
        css.replace(QLatin1String("#3b82f6"), pal.accent.name());
        css.replace(QLatin1String("#ffffff"), pal.text.name());
        setStyleSheet(css);
    };
    applyStyle();
    connect(&Theme::instance(), &Theme::changed, this, applyStyle);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *page = new QScrollArea(this);
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    outer->addWidget(page);

    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("kContent"));
    page->setWidget(content);
    auto *row = new QHBoxLayout(content);
    row->setContentsMargins(24, 24, 24, 24);
    row->addStretch(1);
    auto *col = new QWidget(content);
    col->setMaximumWidth(1200);
    row->addWidget(col, 1000);
    row->addStretch(1);

    auto *lay = new QVBoxLayout(col);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    // --- имена переменных
    lay->addWidget(makeLabel(QStringLiteral("Имена переменных (разделённые запятыми)"), col));
    m_namesEdit = new QLineEdit(col);
    lay->addWidget(m_namesEdit);

    // --- размер карты и заливка
    auto *sizeRow = new QHBoxLayout;
    sizeRow->setSpacing(12);
    auto addSize = [&](const QString &caption, QLineEdit **edit) {
        auto *box = new QVBoxLayout;
        box->setSpacing(4);
        box->addWidget(new QLabel(caption, col));
        *edit = new QLineEdit(QStringLiteral("4"), col);
        (*edit)->setValidator(new QIntValidator(1, kMaxSide, *edit));
        (*edit)->setFixedWidth(110);
        box->addWidget(*edit);
        sizeRow->addLayout(box);
    };
    addSize(QStringLiteral("Ширина"), &m_widthEdit);
    addSize(QStringLiteral("Высота"), &m_heightEdit);
    sizeRow->addSpacing(12);

    m_canvas = new KarnaughCanvas;
    auto addFill = [&](const QString &text, quint8 value) {
        auto *b = new QPushButton(text, col);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, value] { m_canvas->fill(value); });
        sizeRow->addWidget(b, 0, Qt::AlignBottom);
    };
    addFill(QStringLiteral("Все 0"), Karnaugh::Zero);
    addFill(QStringLiteral("Все 1"), Karnaugh::One);
    addFill(QStringLiteral("Все X"), Karnaugh::DontCare);
    sizeRow->addStretch(1);
    lay->addLayout(sizeRow);

    lay->addWidget(makeLabel(QStringLiteral(
        "Щёлкните по ячейке, чтобы изменить её значение: 0 → 1 → X (правая кнопка — в обратную сторону, "
        "протяжка закрашивает ячейки). Если размер не степень двойки, лишние наборы считаются безразличными (X)."), col));

    // --- карта
    m_tableScroll = new QScrollArea(col);
    m_tableScroll->setWidgetResizable(true);
    m_tableScroll->setFrameShape(QFrame::NoFrame);
    auto *host = new QWidget;
    host->setObjectName(QStringLiteral("kTableHost"));
    auto *hostLay = new QHBoxLayout(host);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->addStretch(1);
    hostLay->addWidget(m_canvas, 0, Qt::AlignTop);
    hostLay->addStretch(1);
    m_tableScroll->setWidget(host);
    lay->addWidget(m_tableScroll);

    m_status = new QLabel(col);
    m_status->setObjectName(QStringLiteral("kStatus"));
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);
    lay->addWidget(m_status);

    // --- результаты 2×2: сверху минимальные ДНФ и КНФ, снизу совершенные СДНФ и СКНФ
    struct Out { QString title; QTextEdit **edit; int row; int column; };
    const Out outs[] = {
        { QStringLiteral("ДНФ (минимальная)"), &m_mdnf, 0, 0 },
        { QStringLiteral("КНФ (минимальная)"), &m_mknf, 0, 1 },
        { QStringLiteral("СДНФ"), &m_sdnf, 1, 0 },
        { QStringLiteral("СКНФ"), &m_sknf, 1, 1 },
    };
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    for (const Out &o : outs) {
        auto *cell = new QVBoxLayout;
        cell->setSpacing(4);

        auto *head = new QHBoxLayout;
        head->addWidget(new QLabel(o.title, col), 1);
        auto *copy = new QPushButton(QStringLiteral("Копировать"), col);
        copy->setCursor(Qt::PointingHandCursor);
        head->addWidget(copy);
        cell->addLayout(head);

        QTextEdit *edit = new QTextEdit(col);
        edit->setReadOnly(true);
        edit->setMinimumHeight(110);
        *o.edit = edit;
        cell->addWidget(edit, 1);
        connect(copy, &QPushButton::clicked, this, [edit] {
            QApplication::clipboard()->setText(edit->property("plain").toString());
        });
        grid->addLayout(cell, o.row, o.column);
    }
    lay->addLayout(grid, 1);

    // --- логика
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(150);
    connect(m_debounce, &QTimer::timeout, this, &KarnaughWidget::recompute);

    connect(m_canvas, &KarnaughCanvas::changed, this, &KarnaughWidget::scheduleRecompute);
    connect(m_canvas, &KarnaughCanvas::geometryChanged, this, &KarnaughWidget::updateTableHeight);
    connect(m_widthEdit, &QLineEdit::textEdited, this, &KarnaughWidget::onSizeEdited);
    connect(m_heightEdit, &QLineEdit::textEdited, this, &KarnaughWidget::onSizeEdited);
    connect(m_namesEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_namesEdited = !text.trimmed().isEmpty();
        if (!m_namesEdited)
            syncDefaultNames();
    });
    connect(m_namesEdit, &QLineEdit::textChanged, this, &KarnaughWidget::onNamesChanged);

    syncDefaultNames();
    m_canvas->setVarNames(effectiveNames());
    updateTableHeight();
    scheduleRecompute();
}

KarnaughWidget::~KarnaughWidget()
{
    if (m_cancel)
        m_cancel->store(true);
}

KarnaughWidget::State KarnaughWidget::state() const
{
    State s;
    s.width = m_canvas->columns();
    s.height = m_canvas->rows();
    s.cells = m_canvas->cells();
    s.names = m_namesEdit->text();
    s.namesEdited = m_namesEdited;
    return s;
}

void KarnaughWidget::setState(const State &state)
{
    m_canvas->resizeMap(state.width, state.height);
    m_widthEdit->setText(QString::number(m_canvas->columns()));
    m_heightEdit->setText(QString::number(m_canvas->rows()));
    m_canvas->setCells(state.cells);
    m_namesEdited = state.namesEdited && !state.names.trimmed().isEmpty();
    if (m_namesEdited) {
        m_syncingNames = true;
        m_namesEdit->setText(state.names);
        m_syncingNames = false;
    } else {
        syncDefaultNames();
    }
    m_canvas->setVarNames(effectiveNames());
    scheduleRecompute();
}

int KarnaughWidget::variableCount() const
{
    return Karnaugh::bitsFor(m_canvas->rows()) + Karnaugh::bitsFor(m_canvas->columns());
}

QStringList KarnaughWidget::effectiveNames() const
{
    QStringList parts;
    for (const QString &s : m_namesEdit->text().split(QLatin1Char(','))) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            parts << t;
    }
    const int n = variableCount();
    const QStringList defaults = Karnaugh::variableNames(n);
    QStringList out;
    for (int i = 0; i < n; ++i)
        out << (i < parts.size() ? parts[i] : defaults[i]);
    return out;
}

void KarnaughWidget::syncDefaultNames()
{
    if (m_namesEdited)
        return;
    m_syncingNames = true;
    m_namesEdit->setText(Karnaugh::variableNames(variableCount()).join(QLatin1Char(',')));
    m_syncingNames = false;
}

void KarnaughWidget::onSizeEdited()
{
    bool okW = false, okH = false;
    const int w = m_widthEdit->text().toInt(&okW);
    const int h = m_heightEdit->text().toInt(&okH);
    if (!okW || !okH || w < 1 || h < 1 || w > kMaxSide || h > kMaxSide)
        return;                          // пока вводят — ждём корректное число
    m_canvas->resizeMap(w, h);
    syncDefaultNames();
    m_canvas->setVarNames(effectiveNames());
}

void KarnaughWidget::onNamesChanged()
{
    if (m_syncingNames)
        return;
    m_canvas->setVarNames(effectiveNames());
    scheduleRecompute();
}

void KarnaughWidget::updateTableHeight()
{
    m_tableScroll->setFixedHeight(std::min(m_canvas->height() + 24, 560));
}

void KarnaughWidget::scheduleRecompute()
{
    if (m_cancel)
        m_cancel->store(true);           // прерываем устаревший расчёт сразу
    m_status->setText(QStringLiteral("Считается…"));
    m_debounce->start();
}

void KarnaughWidget::recompute()
{
    if (m_cancel)
        m_cancel->store(true);
    m_cancel = std::make_shared<std::atomic<bool>>(false);
    const int generation = ++m_generation;

    Karnaugh::Input in;
    in.width = m_canvas->columns();
    in.height = m_canvas->rows();
    in.cells = m_canvas->cells();
    in.names = effectiveNames();

    const auto cancel = m_cancel;
    QPointer<KarnaughWidget> self(this);
    QThreadPool::globalInstance()->start([self, in, cancel, generation] {
        const Karnaugh::Result r = Karnaugh::solve(in, cancel.get());
        if (r.cancelled)
            return;
        QMetaObject::invokeMethod(qApp, [self, r, generation] {
            if (self)
                self->applyResult(r, generation);
        }, Qt::QueuedConnection);
    });
}

void KarnaughWidget::applyResult(const Karnaugh::Result &r, int generation)
{
    if (generation != m_generation)
        return;                          // пришёл результат устаревшего расчёта

    auto show = [&](QTextEdit *edit, const QString &plain) {
        const QString text = plain.isEmpty() ? QStringLiteral("—") : plain;
        edit->setProperty("plain", text);
        edit->setHtml(QStringLiteral("<span style=\"font-size:17px;\">%1</span>").arg(richFormula(text, r.vars)));
    };
    show(m_sdnf, r.sdnf);
    show(m_mdnf, r.mdnf);
    show(m_sknf, r.sknf);
    show(m_mknf, r.mknf);

    QString status = QStringLiteral("Переменных: %1 (%2 по строкам + %3 по столбцам).")
                         .arg(r.vars.size()).arg(r.rowBits).arg(r.colBits);
    if (!r.note.isEmpty())
        status += QLatin1Char(' ') + r.note;
    m_status->setText(status);
}
