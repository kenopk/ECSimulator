#include "pcb/PcbToolkitSection.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QButtonGroup>
#include <QDate>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "pcb/PcbOptions.h"
#include "pcb/PcbPage.h"
#include "theme/Theme.h"

// ================================================================================================
//  Полоса подвкладок в несколько рядов. Ряд с выбранной вкладкой уходит вниз, как у обычных вкладок Windows.
// ================================================================================================
class PcbTabs : public QWidget
{
    Q_OBJECT
public:
    explicit PcbTabs(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);
        m_group = new QButtonGroup(this);
        m_group->setExclusive(true);
        connect(m_group, &QButtonGroup::idClicked, this, [this](int id) { setCurrentIndex(id); });
    }

    void setTabs(const QStringList &titles, const QVector<int> &rowSizes)
    {
        int next = 0;
        for (int size : rowSizes) {
            auto *row = new QWidget(this);
            auto *lay = new QHBoxLayout(row);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->setSpacing(0);
            for (int i = 0; i < size && next < titles.size(); ++i, ++next) {
                auto *b = new QToolButton(row);
                b->setObjectName(QStringLiteral("pcbTab"));
                b->setCheckable(true);
                b->setText(titles[next]);
                b->setToolTip(titles[next]);
                b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                b->setMinimumWidth(40);
                b->setFocusPolicy(Qt::NoFocus);
                m_group->addButton(b, next);
                m_rowOf.append(m_rows.size());
                lay->addWidget(b, 1);
            }
            m_rows.append(row);
            m_order.append(m_rows.size() - 1);
            m_layout->addWidget(row);
        }
    }

    int currentIndex() const { return m_group->checkedId(); }

    void setCurrentIndex(int id)
    {
        if (id < 0 || id >= m_rowOf.size())
            return;
        if (QAbstractButton *b = m_group->button(id))
            b->setChecked(true);
        const int row = m_rowOf[id];                 // ряд выбранной вкладки — вниз
        m_order.removeAll(row);
        m_order.append(row);
        for (QWidget *r : std::as_const(m_rows))
            m_layout->removeWidget(r);
        for (int idx : std::as_const(m_order))
            m_layout->addWidget(m_rows[idx]);
        emit currentChanged(id);
    }

signals:
    void currentChanged(int index);

private:
    QVBoxLayout *m_layout = nullptr;
    QButtonGroup *m_group = nullptr;
    QVector<QWidget *> m_rows;
    QVector<int> m_order;
    QVector<int> m_rowOf;      // индекс вкладки → номер ряда
};

// ================================================================================================
//  Раздел
// ================================================================================================
PcbToolkitSection::PcbToolkitSection(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(10);

    m_pages = createPcbPages(this);
    QStringList titles;
    for (PcbPage *p : std::as_const(m_pages))
        titles << p->tabTitle();

    // Полоса «связь со схемой»: показывается, когда калькуляторы привязаны к проводу схемы.
    m_linkBar = new QFrame(this);
    m_linkBar->setObjectName(QStringLiteral("pcbLinkBar"));
    auto *lb = new QHBoxLayout(m_linkBar);
    lb->setContentsMargins(12, 6, 8, 6);
    lb->setSpacing(10);
    m_linkLabel = new QLabel(m_linkBar);
    m_linkLabel->setTextFormat(Qt::RichText);
    lb->addWidget(m_linkLabel, 1);
    m_autoComment = new QCheckBox(QStringLiteral("Записывать расчёт в комментарий (пока он пуст)"), m_linkBar);
    m_autoComment->setChecked(true);
    lb->addWidget(m_autoComment);
    m_linkWrite = new QPushButton(QStringLiteral("В комментарий"), m_linkBar);
    auto *show = new QPushButton(QStringLiteral("Показать на схеме"), m_linkBar);
    auto *unlink = new QPushButton(QStringLiteral("Отвязать"), m_linkBar);
    lb->addWidget(m_linkWrite);
    lb->addWidget(show);
    lb->addWidget(unlink);
    m_linkBar->hide();
    outer->addWidget(m_linkBar);
    connect(m_linkWrite, &QPushButton::clicked, this, [this] {
        if (m_current >= 0)
            emit traceCommentRequested(m_pages[m_current]->traceSummary(), false);
    });
    connect(show, &QPushButton::clicked, this, &PcbToolkitSection::traceShowRequested);
    connect(unlink, &QPushButton::clicked, this, [this] {
        setTrace(PcbTrace());
        emit traceUnlinked();
    });
    m_autoTimer = new QTimer(this);
    m_autoTimer->setSingleShot(true);
    m_autoTimer->setInterval(450);
    connect(m_autoTimer, &QTimer::timeout, this, [this] {
        if (m_trace.valid && m_autoComment->isChecked() && m_current >= 0 && m_pages[m_current]->usesTrace()) {
            const QString text = m_pages[m_current]->traceSummary();
            if (!text.isEmpty())
                emit traceCommentRequested(text, true);
        }
    });
    connect(m_autoComment, &QCheckBox::toggled, this, [this](bool) { scheduleAutoComment(); });

    m_tabs = new PcbTabs(this);
    m_tabs->setTabs(titles, { 7, 7, 5 });
    outer->addWidget(m_tabs);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("pcbScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll, 1);

    m_body = new QWidget(scroll);
    m_body->setObjectName(QStringLiteral("pcbBody"));
    scroll->setWidget(m_body);
    auto *body = new QHBoxLayout(m_body);
    body->setContentsMargins(0, 4, 0, 0);
    body->setSpacing(14);

    m_stack = new QStackedWidget(m_body);
    for (PcbPage *p : std::as_const(m_pages))
        m_stack->addWidget(p);
    body->addWidget(m_stack, 1);

    auto *right = new QWidget(m_body);
    right->setFixedWidth(480);
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);
    m_options = new PcbOptions(right);
    rl->addWidget(m_options);
    auto *infoBox = makePcbGroup(QStringLiteral("Информация"), right);
    auto *il = new QVBoxLayout(infoBox);
    il->setContentsMargins(12, 10, 12, 10);
    m_info = new QLabel(infoBox);
    m_info->setTextFormat(Qt::RichText);
    m_info->setWordWrap(true);
    m_info->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    il->addWidget(m_info);
    rl->addWidget(infoBox);
    rl->addStretch(1);
    body->addWidget(right, 0, Qt::AlignTop);

    for (PcbPage *p : std::as_const(m_pages)) {
        connect(p, &PcbPage::changed, this, [this, p] {
            if (m_current >= 0 && m_pages[m_current] == p) {
                refreshInfo();
                scheduleAutoComment();
            }
        });
        connect(p, &PcbPage::sendValue, this, [this](int id, const QString &key, double value) {
            if (id < 0 || id >= m_pages.size())
                return;
            if (!key.isEmpty())
                m_pages[id]->receiveValue(key, value);
            m_tabs->setCurrentIndex(id);
        });
    }
    connect(m_options, &PcbOptions::contextChanged, this, &PcbToolkitSection::applyContext);
    connect(m_options, &PcbOptions::solveRequested, this, [this] {
        if (m_current >= 0)
            m_pages[m_current]->setContext(m_options->context());
    });
    connect(m_options, &PcbOptions::printRequested, this, &PcbToolkitSection::printCurrent);
    connect(m_tabs, &PcbTabs::currentChanged, this, &PcbToolkitSection::selectPage);
    connect(&Theme::instance(), &Theme::changed, this, &PcbToolkitSection::applyStyle);

    applyStyle();
    applyContext();
    const int saved = QSettings().value("pcb/page", 14).toInt();       // 14 — «Полоса и длина», как в исходной программе
    m_tabs->setCurrentIndex(std::clamp(saved, 0, int(m_pages.size()) - 1));
}

void PcbToolkitSection::showPage(int index)
{
    m_tabs->setCurrentIndex(index);
}

void PcbToolkitSection::setTrace(const PcbTrace &trace)
{
    if (trace == m_trace)
        return;
    m_trace = trace;
    for (PcbPage *p : std::as_const(m_pages))
        p->setTrace(trace);
    updateLinkBar();
    refreshInfo();
    scheduleAutoComment();                       // и при смене провода, и при смене его длины/ширины
}

void PcbToolkitSection::updateLinkBar()
{
    m_linkBar->setVisible(m_trace.valid);
    if (!m_trace.valid)
        return;
    const bool uses = m_current >= 0 && m_pages[m_current]->usesTrace();
    QString text = QStringLiteral("<b>%1</b> &nbsp;·&nbsp; L = %2 мм").arg(m_trace.name.toHtmlEscaped(), PcbFmt::fixed(m_trace.lengthMm, 2));
    if (m_trace.widthMm > 0)
        text += QStringLiteral(" &nbsp;·&nbsp; W = %1 мм").arg(PcbFmt::fixed(m_trace.widthMm, 3));
    if (m_trace.pair > 0) {
        text += QStringLiteral(" &nbsp;·&nbsp; дифф. пара №%1").arg(m_trace.pair);
        if (m_trace.pairSize < 2)
            text += QStringLiteral(" (второго проводника нет)");
        else if (m_trace.skewMm < 0)
            text += QStringLiteral(" · <span style=\"color:#ef4444\">короче на %1 мм</span>").arg(PcbFmt::fixed(-m_trace.skewMm, 2));
        else if (m_trace.skewMm > 0)
            text += QStringLiteral(" · длиннее на %1 мм").arg(PcbFmt::fixed(m_trace.skewMm, 2));
        else
            text += QStringLiteral(" · длины равны");
    }
    if (!uses)
        text += QStringLiteral(" &nbsp;·&nbsp; <span style=\"opacity:0.7\">эта вкладка не использует трассу</span>");
    m_linkLabel->setText(text);
    m_autoComment->setEnabled(uses);
    m_linkWrite->setEnabled(uses);
}

void PcbToolkitSection::scheduleAutoComment()
{    if (m_trace.valid && m_autoComment->isChecked() && m_current >= 0 && m_pages[m_current]->usesTrace())
        m_autoTimer->start();
}

void PcbToolkitSection::selectPage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return;
    m_current = index;
    m_stack->setCurrentIndex(index);
    PcbPage *page = m_pages[index];
    m_options->setPageConfig(page->config(), page->optionsWidget());
    page->setContext(m_options->context());
    refreshInfo();
    updateLinkBar();
    scheduleAutoComment();
    QSettings().setValue("pcb/page", index);
}

void PcbToolkitSection::applyContext()
{
    for (PcbPage *p : std::as_const(m_pages))
        p->setContext(m_options->context());
    refreshInfo();
}

void PcbToolkitSection::refreshInfo()
{
    if (m_current < 0)
        return;
    PcbPage *page = m_pages[m_current];
    const PcbPageConfig cfg = page->config();
    if (!page->optionsWidget())
        m_options->setPlaneThicknessEnabled(page->planeThicknessEnabled());

    const PcbContext &ctx = m_options->context();
    QStringList items;
    items << PcbFmt::info(QStringLiteral("Полная толщина меди"),
                          (cfg.base || cfg.plating) ? PcbFmt::trimmed(ctx.totalCuUm()) + QStringLiteral(" мкм") : QStringLiteral("Н/Д"));
    items << page->infoItems();
    QString html = QStringLiteral("<table width=\"100%\" cellspacing=\"6\">");
    for (int i = 0; i < items.size(); i += 2) {
        html += QStringLiteral("<tr><td width=\"50%\">%1</td><td width=\"50%\">%2</td></tr>")
                    .arg(items[i], i + 1 < items.size() ? items[i + 1] : QString());
    }
    html += QStringLiteral("</table>");
    m_info->setText(html);
}

void PcbToolkitSection::applyStyle()
{
    const Palette &c = Theme::instance().palette();
    QString css = QStringLiteral(R"(
QGroupBox#pcbGroup { border: 1px solid @border@; border-radius: 6px; margin-top: 11px; padding-top: 8px; background: transparent; }
QGroupBox#pcbGroup::title { subcontrol-origin: margin; subcontrol-position: top left; left: 12px; padding: 0 6px; color: @text@; }
QGroupBox#pcbGroup:disabled::title { color: @dim@; }
QFrame#pcbField { background: @bg@; border: 1px solid @border@; border-radius: 4px; }
QFrame#pcbField[output="true"] { background: @alt@; }
QFrame#pcbField:disabled { background: @panel@; }
QFrame#pcbField QLineEdit { border: none; background: transparent; padding: 0; min-height: 0; font-size: 13px; font-weight: 600; color: @text@; }
QFrame#pcbField QLineEdit:read-only { color: @text@; }
QFrame#pcbField QLineEdit:disabled { color: @dim@; }
QFrame#pcbField QLabel#pcbUnit { color: @text@; font-weight: 600; padding: 0; }
QFrame#pcbField:disabled QLabel#pcbUnit { color: @dim@; }
QFrame#pcbField QComboBox#pcbUnitCombo { border: none; background: transparent; font-weight: 600; padding: 0 18px 0 4px; min-height: 0; }
QLabel#pcbLabel { color: @dim@; padding: 0; }
QLabel#pcbHint { color: @dim@; font-size: 11px; }
QProgressBar#pcbBar { border: none; background: @border@; border-radius: 4px; }
QProgressBar#pcbBar::chunk { background: @positive@; border-radius: 4px; }
QToolButton#pcbTab { background: @panel@; border: 1px solid @border@; border-radius: 0; padding: 7px 6px; color: @dim@; font-size: 12px; }
QToolButton#pcbTab:hover { background: @alt@; color: @text@; }
QToolButton#pcbTab:checked { background: @bg@; color: @text@; border-bottom: 2px solid @accent@; font-weight: 600; }
QScrollArea#pcbScroll, QWidget#pcbBody { background: transparent; }
QToolButton#pcbHelp { background: @bg@; color: @dim@; border: 1px solid @border@; border-radius: 11px; font-weight: 700; font-size: 12px; padding: 0; }
QToolButton#pcbHelp:hover { color: @onAccent@; background: @accent@; border-color: @accent@; }
QFrame#pcbLinkBar { background: @alt@; border: 1px solid @accent@; border-radius: 6px; }
)");
    css.replace(QLatin1String("@bg@"), c.bg.name());
    css.replace(QLatin1String("@panel@"), c.panel.name());
    css.replace(QLatin1String("@alt@"), c.panelAlt.name());
    css.replace(QLatin1String("@border@"), c.border.name());
    css.replace(QLatin1String("@text@"), c.text.name());
    css.replace(QLatin1String("@dim@"), c.textDim.name());
    css.replace(QLatin1String("@positive@"), c.positive.name());
    css.replace(QLatin1String("@accent@"), c.accent.name());
    css.replace(QLatin1String("@onAccent@"), c.onAccent.name());
    setStyleSheet(css);
}

void PcbToolkitSection::printCurrent()
{
    if (m_current < 0)
        return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QPixmap shot = m_body->grab();
    QPainter painter(&printer);
    const QRect page = painter.viewport();
    QFont f = painter.font();
    f.setPointSize(12);
    f.setBold(true);
    painter.setFont(f);
    const QString head = QStringLiteral("%1 — %2").arg(m_pages[m_current]->tabTitle(), QDate::currentDate().toString(Qt::ISODate));
    painter.drawText(QRect(page.left(), page.top(), page.width(), 400), Qt::AlignLeft | Qt::AlignTop, head);
    QSize target = shot.size();
    target.scale(page.width(), page.height() - 500, Qt::KeepAspectRatio);
    painter.drawPixmap(QRect(QPoint(page.left(), page.top() + 500), target), shot);
}

#include "PcbToolkitSection.moc"
