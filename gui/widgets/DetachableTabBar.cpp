#include "widgets/DetachableTabBar.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>

namespace {

const char *kMime = "application/x-govno-tab";

// Идущее сейчас перетаскивание: откуда и какая вкладка. Приложение одно, поэтому статика надёжна.
QPointer<DetachableTabBar> g_source;
int g_sourceIndex = -1;

constexpr int kDetachMargin = 28;     // на сколько пикселей надо выйти за полосу, чтобы начать перетаскивание

} // namespace

DetachableTabBar::DetachableTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setAcceptDrops(true);
}

void DetachableTabBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressPos = e->position().toPoint();
        m_pressIndex = tabAt(m_pressPos);
    }
    QTabBar::mousePressEvent(e);
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *e)
{
    m_pressIndex = -1;
    QTabBar::mouseReleaseEvent(e);
}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = e->position().toPoint();
    if ((e->buttons() & Qt::LeftButton) && m_pressIndex >= 0 && count() >= 1
        && !rect().adjusted(-kDetachMargin, -kDetachMargin, kDetachMargin, kDetachMargin).contains(pos)) {
        const int index = m_pressIndex;
        m_pressIndex = -1;

        // Закончить внутреннюю перестановку вкладки, прежде чем начать своё перетаскивание.
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(m_pressPos), e->globalPosition(),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QTabBar::mouseReleaseEvent(&release);

        startDrag(index);
        return;
    }
    QTabBar::mouseMoveEvent(e);
}

void DetachableTabBar::startDrag(int index)
{
    if (index < 0 || index >= count())
        return;

    auto *drag = new QDrag(this);
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(kMime), QByteArray("tab"));
    drag->setMimeData(mime);

    const QRect r = tabRect(index);
    drag->setPixmap(grab(r));
    drag->setHotSpot(m_pressPos - r.topLeft());

    QPointer<DetachableTabBar> self(this);
    g_source = this;
    g_sourceIndex = index;
    const Qt::DropAction action = drag->exec(Qt::MoveAction);
    g_source = nullptr;
    g_sourceIndex = -1;

    // Полоса могла исчезнуть, пока шёл перенос (вкладку унесли, а окно закрылось).
    if (!self)
        return;

    // Никто не принял вкладку — значит, её отпустили вне полос вкладок: открываем новое окно.
    // Если курсор вернулся на свою же полосу (или перетаскивание отменили там), ничего не делаем.
    if (action == Qt::IgnoreAction) {
        const QPoint global = QCursor::pos();
        const QRect own(mapToGlobal(QPoint(0, 0)), size());
        if (!own.contains(global))
            emit detachRequested(index, global);
    }
    emit dragEnded();
}

bool DetachableTabBar::dragActive()
{
    return !g_source.isNull();
}

bool DetachableTabBar::canAccept(const QMimeData *mime, int kind)
{
    return g_source && g_source->kind() == kind && mime && mime->hasFormat(QString::fromLatin1(kMime));
}

void DetachableTabBar::dropAtEnd()
{
    if (g_source)
        emit tabDropped(g_source, g_sourceIndex, count());
}

int DetachableTabBar::insertIndexAt(const QPoint &pos) const
{
    for (int i = 0; i < count(); ++i)
        if (pos.x() < tabRect(i).center().x())
            return i;
    return count();
}

void DetachableTabBar::dragEnterEvent(QDragEnterEvent *e)
{
    if (canAccept(e->mimeData(), m_kind))
        e->acceptProposedAction();
    else
        e->ignore();
}

void DetachableTabBar::dragMoveEvent(QDragMoveEvent *e)
{
    if (canAccept(e->mimeData(), m_kind))
        e->acceptProposedAction();
    else
        e->ignore();
}

void DetachableTabBar::dropEvent(QDropEvent *e)
{
    if (!canAccept(e->mimeData(), m_kind)) {
        e->ignore();
        return;
    }
    const QPointer<DetachableTabBar> source = g_source;
    const int sourceIndex = g_sourceIndex;
    const int target = insertIndexAt(e->position().toPoint());
    e->setDropAction(Qt::MoveAction);
    e->accept();
    emit tabDropped(source, sourceIndex, target);
}
