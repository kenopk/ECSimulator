#include "scope/ScopeWidget.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

#include "scope/ScopeDialog.h"
#include "scope/ScopeMenu.h"
#include "scope/ScopePlot.h"
#include "theme/Theme.h"

// ================================================================ ScopeView

ScopeView::ScopeView(ScopeSource *source, QWidget *parent)
    : QWidget(parent), m_source(source)
{
    setMinimumSize(220, 130);
    setMouseTracking(true);
    setAutoFillBackground(false);
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));

    // Перерисовка не чаще ~30 раз в секунду, сколько бы отсчётов ни пришло.
    m_timer = new QTimer(this);
    m_timer->setInterval(33);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_dirty) {
            m_dirty = false;
            update();
        }
    });
    m_timer->start();
    if (m_source)
        connect(m_source, &ScopeSource::changed, this, [this] { m_dirty = true; });
}

void ScopeView::paintEvent(QPaintEvent *)
{
    if (!m_source)
        return;
    QPainter p(this);
    ScopePlot::Options opt;
    opt.hover = m_hover;
    opt.gearHover = m_gearHover;
    ScopePlot::paint(&p, QRectF(rect()), m_source, opt);
}

void ScopeView::mouseMoveEvent(QMouseEvent *e)
{
    const QRectF plot = QRectF(rect()).adjusted(2, 2, -2, -2);
    const bool gear = ScopePlot::gearRect(QRectF(rect())).contains(e->position());
    m_hover = plot.width() > 0 && plot.contains(e->position()) && !gear
        ? (e->position().x() - plot.left()) / plot.width() : -1;
    if (gear != m_gearHover) {
        m_gearHover = gear;
        setCursor(gear ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    update();
}

void ScopeView::leaveEvent(QEvent *)
{
    m_hover = -1;
    m_gearHover = false;
    unsetCursor();
    update();
}

void ScopeView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && ScopePlot::gearRect(QRectF(rect())).contains(e->position())) {
        showProperties();                        // ЛКМ по шестерёнке — свойства осциллографа
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ScopeView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        showProperties();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void ScopeView::showProperties()
{
    ScopeDialog::edit(window(), m_source);
    update();
}

void ScopeView::contextMenuEvent(QContextMenuEvent *e)
{
    ScopeMenu::Actions a;
    a.remove = [this] { emit removeRequested(); };
    a.moveText = QStringLiteral("Переместить осциллограф в схему");
    a.move = [this] { emit moveToSchemeRequested(); };
    if (m_detached) {
        a.windowText = QStringLiteral("Вернуть в нижнюю панель");
    } else {
        a.windowText = QStringLiteral("Вынести в отдельное окно");
        a.arrange = [this](Qt::Orientation o) { emit arrangeRequested(o); };
        a.current = m_arrangement;
    }
    a.window = [this] { emit windowRequested(); };
    ScopeMenu::exec(window(), m_source, e->globalPos(), a);
    e->accept();
}

// ================================================================ ScopeBoard

ScopeBoard::ScopeBoard(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_split = new QSplitter(Qt::Horizontal, this);       // по умолчанию — рядом
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(3);
    lay->addWidget(m_split);
}

ScopeView *ScopeBoard::viewFor(ScopeSource *source) const
{
    for (int i = 0; i < m_split->count(); ++i)
        if (auto *v = qobject_cast<ScopeView *>(m_split->widget(i)); v && v->source() == source)
            return v;
    return nullptr;
}

bool ScopeBoard::contains(ScopeSource *source) const
{
    return viewFor(source) != nullptr;
}

int ScopeBoard::count() const
{
    return m_split->count();
}

ScopeView *ScopeBoard::addScope(ScopeSource *source)
{
    if (ScopeView *existing = viewFor(source))
        return existing;

    auto *view = new ScopeView(source, m_split);
    view->setArrangement(m_split->orientation());
    m_split->addWidget(view);
    connect(view, &ScopeView::removeRequested, this, [this, view] { remove(view); });
    connect(view, &ScopeView::moveToSchemeRequested, this, [this, source] { emit moveToSchemeRequested(source); });
    connect(view, &ScopeView::windowRequested, this, [this, source] { emit windowRequested(source); });
    connect(view, &ScopeView::arrangeRequested, this, [this](Qt::Orientation o) { arrange(o); });

    m_split->setSizes(QList<int>(m_split->count(), 1000));   // поровну
    return view;
}

void ScopeBoard::removeScope(ScopeSource *source)
{
    if (ScopeView *v = viewFor(source))
        remove(v);
}

void ScopeBoard::remove(ScopeView *view)
{
    view->hide();
    view->setParent(nullptr);
    view->deleteLater();
    if (!m_split->count())
        emit emptied();
    else
        m_split->setSizes(QList<int>(m_split->count(), 1000));
}

void ScopeBoard::arrange(Qt::Orientation o)
{
    m_split->setOrientation(o);
    for (int i = 0; i < m_split->count(); ++i)
        if (auto *v = qobject_cast<ScopeView *>(m_split->widget(i)))
            v->setArrangement(o);
    m_split->setSizes(QList<int>(m_split->count(), 1000));
}

// ================================================================ ScopeWindow

ScopeWindow::ScopeWindow(ScopeSource *source, QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("Осциллограмма — ") + source->title());
    resize(620, 340);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_view = new ScopeView(source, this);
    m_view->setDetached(true);
    lay->addWidget(m_view);

    connect(m_view, &ScopeView::removeRequested, this, &QWidget::close);
    connect(m_view, &ScopeView::windowRequested, this, [this, source] { emit dockRequested(source); });
    connect(m_view, &ScopeView::moveToSchemeRequested, this, [this, source] { emit moveToSchemeRequested(source); });
    connect(source, &ScopeSource::changed, this, [this, source] {
        setWindowTitle(QStringLiteral("Осциллограмма — ") + source->title());
    });
}
