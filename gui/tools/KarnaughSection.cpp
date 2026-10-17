#include "tools/KarnaughSection.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QStackedLayout>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "project/ProjectFile.h"
#include "tools/KarnaughWidget.h"
#include "widgets/DetachableTabBar.h"

// QTabWidget с полосой DetachableTabBar вида 1 (карты Карно): вкладку можно бросить в любое место области вкладок.
class KarnaughTabs : public QTabWidget
{
public:
    explicit KarnaughTabs(QWidget *parent = nullptr)
        : QTabWidget(parent), m_bar(new DetachableTabBar(this))
    {
        setTabBar(m_bar);
        m_bar->setKind(1);
        setAcceptDrops(true);
    }
    DetachableTabBar *bar() const { return m_bar; }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (DetachableTabBar::canAccept(e->mimeData(), 1))
            e->acceptProposedAction();
        else
            e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *e) override
    {
        if (DetachableTabBar::canAccept(e->mimeData(), 1))
            e->acceptProposedAction();
        else
            e->ignore();
    }
    void dropEvent(QDropEvent *e) override
    {
        if (!DetachableTabBar::canAccept(e->mimeData(), 1)) {
            e->ignore();
            return;
        }
        e->setDropAction(Qt::MoveAction);
        e->accept();
        m_bar->dropAtEnd();
    }

private:
    DetachableTabBar *m_bar;
};

KarnaughSection::KarnaughSection(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QStackedLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new KarnaughTabs(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(false);          // свои кнопки закрытия, как у схем
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->bar()->installEventFilter(this);
    connect(m_tabs->bar(), &QTabBar::tabMoved, this, [] { ProjectHub::instance().notify(); });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (QWidget *w = m_tabs->widget(index))
            ProjectHub::instance().setActive(ProjectHub::projectOf(w));
        ProjectHub::instance().notify();
    });
    m_layout->addWidget(m_tabs);

    // пустое состояние: все карты закрыты
    auto *empty = new QWidget(this);
    auto *lay = new QVBoxLayout(empty);
    lay->addStretch(1);
    auto *text = new QLabel(QStringLiteral("Нет открытых карт Карно"), empty);
    text->setAlignment(Qt::AlignCenter);
    lay->addWidget(text);
    auto *button = new QPushButton(QStringLiteral("Новая карта Карно"), empty);
    button->setCursor(Qt::PointingHandCursor);
    auto *row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(button);
    row->addStretch(1);
    lay->addLayout(row);
    lay->addStretch(1);
    connect(button, &QPushButton::clicked, this, [this] { newTab(); });
    m_layout->addWidget(empty);

    syncEmptyState();
}

DetachableTabBar *KarnaughSection::bar() const
{
    return m_tabs->bar();
}

int KarnaughSection::tabCount() const { return m_tabs->count(); }
QWidget *KarnaughSection::page(int index) const { return m_tabs->widget(index); }
QString KarnaughSection::pageName(int index) const { return ProjectHub::nameOf(m_tabs->widget(index)); }
QWidget *KarnaughSection::currentPage() const { return m_tabs->currentWidget(); }
bool KarnaughSection::contains(QWidget *page) const { return page && m_tabs->indexOf(page) >= 0; }

void KarnaughSection::syncEmptyState()
{
    bool anyVisible = false;
    for (int i = 0; i < m_tabs->count(); ++i)
        anyVisible = anyVisible || m_tabs->bar()->isTabVisible(i);
    m_layout->setCurrentIndex(anyVisible ? 0 : 1);
}

void KarnaughSection::addPageTab(QWidget *page, int at)
{
    const QString label = ProjectHub::instance().tabLabel(page);
    const int index = at < 0 ? m_tabs->addTab(page, label) : m_tabs->insertTab(at, page, label);

    // Плоская кнопка «×», закрытие отложенное (как у вкладок схем).
    auto *close = new QToolButton(bar());
    close->setObjectName(QStringLiteral("tabClose"));
    close->setText(QStringLiteral("×"));
    close->setToolTip(QStringLiteral("Закрыть вкладку"));
    close->setAutoRaise(true);
    close->setCursor(Qt::ArrowCursor);
    bar()->setTabButton(index, QTabBar::RightSide, close);
    QPointer<QWidget> guard = page;
    connect(close, &QToolButton::clicked, this, [this, guard] {
        QTimer::singleShot(0, this, [this, guard] {
            if (guard)
                closeTab(m_tabs->indexOf(guard));
        });
    });

    m_tabs->setCurrentIndex(index);
    refreshLabels();
    syncEmptyState();
    ProjectHub::instance().notify();
}

KarnaughWidget *KarnaughSection::addMap(int projectId, const QString &name)
{
    ProjectHub &hub = ProjectHub::instance();
    auto *page = new KarnaughWidget;
    const QString unique = name.isEmpty() ? hub.nextName(projectId, ProjectHub::Map)
                                          : hub.uniqueName(projectId, ProjectHub::Map, name);
    ProjectHub::attach(page, projectId, ProjectHub::Map, unique);
    addPageTab(page, -1);
    return page;
}

void KarnaughSection::newTab()
{
    addMap(ProjectHub::instance().ensureActive());
}

QWidget *KarnaughSection::takePage(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return nullptr;
    m_tabs->removeTab(index);               // страница остаётся жить: её заберёт другое окно
    syncEmptyState();
    ProjectHub::instance().notify();
    return page;
}

void KarnaughSection::adoptPage(QWidget *page, int at)
{
    addPageTab(page, at);
}

bool KarnaughSection::select(QWidget *page)
{
    const int i = page ? m_tabs->indexOf(page) : -1;
    if (i < 0)
        return false;
    m_tabs->setCurrentIndex(i);
    return true;
}

void KarnaughSection::setPageVisible(QWidget *page, bool visible)
{
    const int i = page ? m_tabs->indexOf(page) : -1;
    if (i < 0)
        return;
    m_tabs->setTabVisible(i, visible);
    if (!visible && m_tabs->currentIndex() == i)
        for (int k = 0; k < m_tabs->count(); ++k)
            if (m_tabs->isTabVisible(k)) {
                m_tabs->setCurrentIndex(k);
                break;
            }
    syncEmptyState();
}

void KarnaughSection::refreshLabels()
{
    const ProjectHub &hub = ProjectHub::instance();
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget *w = m_tabs->widget(i);
        const QString label = hub.tabLabel(w);
        if (m_tabs->tabText(i) != label)
            m_tabs->setTabText(i, label);
        const ProjectInfo *p = hub.find(ProjectHub::projectOf(w));
        m_tabs->setTabToolTip(i, p ? QStringLiteral("%1 — %2").arg(p->name, ProjectHub::nameOf(w)) : ProjectHub::nameOf(w));
    }
}

void KarnaughSection::closeTab(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;
    m_tabs->removeTab(index);
    page->deleteLater();
    syncEmptyState();
    ProjectHub::instance().notify();
    if (m_tabs->count() == 0)
        emit becameEmpty();
}

void KarnaughSection::closeCurrent()
{
    closeTab(m_tabs->currentIndex());
}

void KarnaughSection::closeAll()
{
    while (m_tabs->count() > 0) {
        QWidget *page = m_tabs->widget(0);
        m_tabs->removeTab(0);
        page->deleteLater();
    }
    syncEmptyState();
    ProjectHub::instance().notify();
}

void KarnaughSection::closeWhere(const QList<QWidget *> &pages)
{
    for (QWidget *w : pages) {
        const int i = m_tabs->indexOf(w);
        if (i >= 0)
            closeTab(i);
    }
}

void KarnaughSection::closePage(QWidget *page)
{
    const int i = page ? m_tabs->indexOf(page) : -1;
    if (i >= 0)
        closeTab(i);
}

void KarnaughSection::renamePage(QWidget *page)
{
    renameTab(page ? m_tabs->indexOf(page) : -1);
}

void KarnaughSection::renameTab(int index)
{
    if (index < 0 || index >= m_tabs->count())
        return;
    QWidget *page = m_tabs->widget(index);
    ProjectHub &hub = ProjectHub::instance();
    bool ok = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("Переименовать вкладку"),
                                               QStringLiteral("Название:"), QLineEdit::Normal,
                                               ProjectHub::nameOf(page), &ok).trimmed();
    if (!ok || text.isEmpty())
        return;
    if (hub.nameTaken(ProjectHub::projectOf(page), ProjectHub::Map, text, page)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Карта с таким именем уже есть в этом проекте."));
        return;
    }
    ProjectHub::setNameOf(page, text);
    refreshLabels();
    hub.notify();
}

void KarnaughSection::showTabMenu(int index, const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *aNew = menu.addAction(QStringLiteral("Новая карта Карно"));

    if (index < 0 || index >= m_tabs->count()) {            // пустое место на полосе вкладок
        if (menu.exec(globalPos) == aNew)
            newTab();
        return;
    }

    QAction *aRename = menu.addAction(QStringLiteral("Переименовать…"));
    menu.addSeparator();
    QAction *aClose = menu.addAction(QStringLiteral("Закрыть"));
    QAction *aOthers = menu.addAction(QStringLiteral("Закрыть остальные"));
    QAction *aRight = menu.addAction(QStringLiteral("Закрыть справа"));
    QAction *aAll = menu.addAction(QStringLiteral("Закрыть все"));
    aOthers->setEnabled(m_tabs->count() > 1);
    aRight->setEnabled(index < m_tabs->count() - 1);

    QPointer<QWidget> target = m_tabs->widget(index);
    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == aNew) {
        newTab();
        return;
    }
    const int idx = m_tabs->indexOf(target);
    if (idx < 0)
        return;

    if (chosen == aRename) {
        renameTab(idx);
    } else if (chosen == aClose) {
        closeTab(idx);
    } else if (chosen == aOthers) {
        QList<QWidget *> list;
        for (int i = 0; i < m_tabs->count(); ++i)
            if (i != idx)
                list.append(m_tabs->widget(i));
        closeWhere(list);
    } else if (chosen == aRight) {
        QList<QWidget *> list;
        for (int i = idx + 1; i < m_tabs->count(); ++i)
            list.append(m_tabs->widget(i));
        closeWhere(list);
    } else if (chosen == aAll) {
        QList<QWidget *> list;
        for (int i = 0; i < m_tabs->count(); ++i)
            list.append(m_tabs->widget(i));
        closeWhere(list);
    }
}

bool KarnaughSection::eventFilter(QObject *obj, QEvent *ev)
{
    if (m_tabs && obj == bar()) {
        switch (ev->type()) {
        case QEvent::ContextMenu: {                          // ПКМ по вкладке
            auto *ce = static_cast<QContextMenuEvent *>(ev);
            const int index = bar()->tabAt(ce->pos());
            const QPoint global = ce->globalPos();
            QTimer::singleShot(0, this, [this, index, global] { showTabMenu(index, global); });
            return true;
        }
        case QEvent::MouseButtonDblClick: {                  // двойной клик: переименовать / новая
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() != Qt::LeftButton)
                break;
            const int index = bar()->tabAt(me->position().toPoint());
            QTimer::singleShot(0, this, [this, index] {
                if (index >= 0)
                    renameTab(index);
                else
                    newTab();
            });
            return true;
        }
        case QEvent::MouseButtonRelease: {                   // СКМ — закрыть вкладку
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() != Qt::MiddleButton)
                break;
            const int index = bar()->tabAt(me->position().toPoint());
            if (index >= 0) {
                QPointer<QWidget> w = m_tabs->widget(index);
                QTimer::singleShot(0, this, [this, w] {
                    if (w)
                        closeTab(m_tabs->indexOf(w));
                });
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, ev);
}
