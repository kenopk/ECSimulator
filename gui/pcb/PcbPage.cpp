#include "pcb/PcbPage.h"

#include <QCursor>
#include <QEvent>
#include <QGroupBox>
#include <QResizeEvent>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include "pcb/PcbDialogs.h"

PcbPage::PcbPage(const QString &tabTitle, const QString &groupTitle, QWidget *parent)
    : QWidget(parent), m_tabTitle(tabTitle)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_frame = makePcbGroup(groupTitle, this);
    outer->addWidget(m_frame);

    // «?» в углу рамки: подсказка при наведении, полная справка по щелчку.
    m_help = new QToolButton(m_frame);
    m_help->setObjectName(QStringLiteral("pcbHelp"));
    m_help->setText(QStringLiteral("?"));
    m_help->setCursor(Qt::PointingHandCursor);
    m_help->setFixedSize(22, 22);
    connect(m_help, &QToolButton::clicked, this, [this] {
        const QString html = helpHtml();
        if (!html.isEmpty())
            PcbDialogs::info(this, QStringLiteral("Справка: ") + m_tabTitle, html);
    });
    m_help->installEventFilter(this);
    m_frame->installEventFilter(this);
    m_help->raise();
}

// Краткая подсказка — первый абзац справки.
static QString helpSummary(const QString &html)
{
    const int a = html.indexOf(QStringLiteral("<p>"));
    const int b = html.indexOf(QStringLiteral("</p>"));
    if (a < 0 || b < a)
        return html;
    return html.mid(a, b - a + 4) + QStringLiteral("<p style=\"color:#8b9099\">Нажмите «?», чтобы открыть полную справку.</p>");
}

bool PcbPage::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_help && ev->type() == QEvent::Enter) {
        const QString html = helpHtml();
        if (!html.isEmpty())
            QToolTip::showText(QCursor::pos(), QStringLiteral("<div style=\"max-width:360px\">%1</div>").arg(helpSummary(html)), m_help);
    }
    if (obj == m_frame && ev->type() == QEvent::Resize)
        placeHelpButton();
    return QWidget::eventFilter(obj, ev);
}

void PcbPage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    placeHelpButton();
}

void PcbPage::placeHelpButton()
{
    if (m_help && m_frame)
        m_help->move(m_frame->width() - m_help->width() - 10, 2);
    if (m_help)
        m_help->raise();
}

void PcbPage::setContext(const PcbContext &ctx)
{
    m_ctx = ctx;
    contextChanged();
    recalc();
}

void PcbPage::recalc()
{
    solve();
    emit changed();
}

PcbField *PcbPage::makeInput(double base, const QString &unit)
{
    auto *f = new PcbField(PcbField::Input, m_frame);
    if (!unit.isEmpty())
        f->setUnit(unit);
    f->setValue(base);
    connect(f, &PcbField::edited, this, [this] { recalc(); });
    return f;
}

PcbField *PcbPage::makeOutput(const QString &unit, int decimals)
{
    auto *f = new PcbField(PcbField::Output, m_frame);
    if (!unit.isEmpty())
        f->setUnit(unit);
    f->setDecimals(decimals);
    return f;
}
