#include "app/EditorWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QDockWidget>
#include <QFrame>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>
#include <QHBoxLayout>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "canvas/CircuitScene.h"
#include "canvas/CircuitView.h"
#include "dialogs/ElementDialog.h"
#include "dialogs/SettingsDialog.h"
#include "items/CircuitItem.h"
#include "items/SymbolPainter.h"
#include "scope/ScopeDialog.h"
#include "sim/IssueLog.h"
#include "sim/Preflight.h"
#include "scope/ScopeItem.h"
#include "scope/ScopeMenu.h"
#include "scope/ScopeWidget.h"
#include "project/ProjectFile.h"
#include "project/ProjectPanel.h"
#include "theme/Theme.h"
#include "tools/KarnaughSection.h"
#include "tools/KarnaughWidget.h"
#include "widgets/PropertiesPanel.h"
#include "widgets/SimulationPanel.h"

namespace {

const QString kSelectHint = QStringLiteral(
    "ЛКМ — выбрать и переместить · Shift — тащить элемент целиком, даже за вывод · "
    "Alt — вместе с проводами · ПКМ — все элементы · двойной клик — свойства");

// Горячие клавиши быстрых элементов (S — «Выбор», остальные клавиши свободны).
struct KeyDef {
    ElementType type;
    const char *key;
};

const KeyDef kKeys[] = {
    { ElementType::Wire,            "W" },
    { ElementType::Resistor,        "R" },
    { ElementType::Capacitor,       "C" },
    { ElementType::PolarCapacitor,  "E" },
    { ElementType::Inductor,        "L" },
    { ElementType::VoltageSourceDC, "V" },
    { ElementType::VoltageSourceAC, "A" },
    { ElementType::CurrentSource,   "I" },
    { ElementType::Ground,          "G" },
    { ElementType::Diode,           "D" },
    { ElementType::NPN,             "T" },
    { ElementType::OpAmp,           "O" },
    { ElementType::Switch,          "K" },
};

QString keyFor(ElementType type)
{
    for (const KeyDef &k : kKeys)
        if (k.type == type)
            return QString::fromLatin1(k.key);
    return QString();
}

// Иконка сразу в нескольких размерах: меню берёт мелкий, панель — крупный.
QIcon elementIcon(ElementType type, const QColor &color)
{
    QIcon icon;
    for (int s : { 16, 24, 32, 48 })
        icon.addPixmap(SymbolPainter::icon(type, s, color));
    return icon;
}

QIcon selectionIcon(const QColor &color)
{
    QIcon icon;
    for (int s : { 16, 24, 32, 48 })
        icon.addPixmap(SymbolPainter::selectIcon(s, color));
    return icon;
}

// Значок «карта Карно»: сетка 3×3 с двумя закрашенными клетками.
QIcon karnaughIcon(const QColor &color)
{
    QIcon icon;
    for (int size : { 24, 32, 48 }) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal margin = size * 0.16;
        const qreal cell = (size - 2 * margin) / 3.0;
        QPen pen(color, std::max(1.0, size / 22.0));
        pen.setJoinStyle(Qt::MiterJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(margin, margin, cell * 3, cell * 3));
        for (int i = 1; i < 3; ++i) {
            p.drawLine(QPointF(margin + i * cell, margin), QPointF(margin + i * cell, margin + 3 * cell));
            p.drawLine(QPointF(margin, margin + i * cell), QPointF(margin + 3 * cell, margin + i * cell));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(margin + cell, margin, cell, cell));
        p.drawRect(QRectF(margin + 2 * cell, margin + cell, cell, cell));
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

// Все открытые окна редактора: имена вкладок и их нумерация общие, чтобы при переносе вкладки
// из одного окна в другое имена не конфликтовали.
QList<EditorWindow *> &allEditors()
{
    static QList<EditorWindow *> list;
    return list;
}

QVector<CircuitView *> allViews(const QTabWidget *tabs)
{
    QVector<CircuitView *> out;
    for (int i = 0; i < tabs->count(); ++i)
        if (auto *v = qobject_cast<CircuitView *>(tabs->widget(i)))
            out.append(v);
    return out;
}

QVector<QWidget *> allPages(const QTabWidget *tabs)
{
    QVector<QWidget *> out;
    for (int i = 0; i < tabs->count(); ++i)
        out.append(tabs->widget(i));
    return out;
}

constexpr qreal kBaseFlowSpeed = 40.0;      // пикселей/с при множителе 1×

} // namespace

// ================================================================ создание

EditorWindow::EditorWindow(QWidget *parent, bool standalone)
    : QMainWindow(parent), m_standalone(standalone)
{
    if (standalone) {                 // отдельное окно: закрывается и удаляется само
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QStringLiteral("Симулятор электрических цепей"));
        resize(1200, 800);
    } else {
        setWindowFlags(Qt::Widget);   // вложенное окно внутри MainWindow
    }

    allEditors().append(this);
    m_undoGroup = new QUndoGroup(this);

    m_tabs = new EditorTabs(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(false);          // свои кнопки закрытия — см. newTab()
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);

    // Два раздела: «Симулятор электронных цепей» (вкладки схем) и «Карты Карно» (свои вкладки).
    m_karnaugh = new KarnaughSection(this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_tabs);
    m_stack->addWidget(m_karnaugh);
    setCentralWidget(m_stack);

    buildActions();
    buildMenus();
    buildToolBars();
    buildDock();
    buildStatusBar();

    connect(m_tabs, &QTabWidget::currentChanged, this, &EditorWindow::onTabChanged);
    connect(m_tabs->bar(), &QTabBar::tabMoved, this, [] { ProjectHub::instance().notify(); });
    m_tabs->bar()->installEventFilter(this);
    m_tabs->bar()->setOwner(this);
    connect(m_tabs->bar(), &DetachableTabBar::detachRequested, this, &EditorWindow::detachTab);
    connect(m_tabs->bar(), &DetachableTabBar::tabDropped, this, &EditorWindow::onTabDropped);
    auto onDragEnded = [this] {
        if (!m_closeAfterDrag)
            return;
        m_closeAfterDrag = false;
        QTimer::singleShot(0, this, [this] { afterPagesChanged(); });
    };
    connect(m_tabs->bar(), &DetachableTabBar::dragEnded, this, onDragEnded);

    // карты Карно тоже можно отрывать в новое окно и переносить между окнами
    m_karnaugh->bar()->setOwner(this);
    connect(m_karnaugh->bar(), &DetachableTabBar::detachRequested, this, &EditorWindow::detachMap);
    connect(m_karnaugh->bar(), &DetachableTabBar::tabDropped, this, &EditorWindow::onMapDropped);
    connect(m_karnaugh->bar(), &DetachableTabBar::dragEnded, this, onDragEnded);
    connect(m_karnaugh, &KarnaughSection::becameEmpty, this, [this] {
        QTimer::singleShot(0, this, [this] { afterPagesChanged(); });
    });
    connect(&ProjectHub::instance(), &ProjectHub::changed, this, &EditorWindow::refreshTabLabels);

    if (standalone) {
        m_menuAction->setText(QStringLiteral("Закрыть окно"));
        connect(this, &EditorWindow::backToMainMenu, this, &QWidget::close);   // закрыли последнюю вкладку
    }

    connect(&Theme::instance(), &Theme::changed, this, &EditorWindow::refreshIcons);

    if (!standalone) {
        buildProjectDock();
        newTab();
        refreshProjectPanel();
    }                     // в оторванное окно вкладку принесут сами
}

void EditorWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    if (m_docksSized || !m_simDock)
        return;
    m_docksSized = true;
    // До показа окна Qt игнорирует размеры панелей, поэтому задаём их при первом показе.
    resizeDocks({ m_simDock, m_propDock }, { 340, 340 }, Qt::Horizontal);
    resizeDocks({ m_scopeDock }, { 240 }, Qt::Vertical);
    if (m_projectDock && m_navDock)
        resizeDocks({ m_navDock, m_projectDock }, { 64, 230 }, Qt::Horizontal);
}

EditorWindow::~EditorWindow()
{
    allEditors().removeAll(this);
    ProjectHub::instance().notify();
    // Вкладки удаляются вместе с окном; к этому моменту наши обработчики
    // не должны больше реагировать на сигналы умирающих видов.
    for (const QMetaObject::Connection &c : std::as_const(m_viewConnections))
        QObject::disconnect(c);
    m_viewConnections.clear();
    m_view = nullptr;
}

void EditorWindow::buildActions()
{
    // --- инструменты: «Выбор» + по действию на каждый элемент реестра ---------
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    m_selectAction = new QAction(QStringLiteral("Выбор"), this);
    m_selectAction->setCheckable(true);
    m_selectAction->setChecked(true);
    m_selectAction->setShortcut(QKeySequence(QStringLiteral("S")));
    m_selectAction->setToolTip(QStringLiteral("Выбор (S)"));
    m_toolGroup->addAction(m_selectAction);
    addAction(m_selectAction);
    connect(m_selectAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView())
            v->setSelectMode();
        syncToolActions();
    });

    m_elementActions.fill(nullptr, int(ElementType::Count_));
    for (const ElementInfo &info : allElements()) {
        const QString name = QString::fromUtf8(info.name);
        const QString key = keyFor(info.type);

        auto *a = new QAction(name, this);
        a->setCheckable(true);
        if (!key.isEmpty()) {
            a->setShortcut(QKeySequence(key));
            a->setToolTip(QStringLiteral("%1 (%2)").arg(name, key));
        } else {
            a->setToolTip(name);
        }
        m_toolGroup->addAction(a);
        addAction(a);                          // чтобы горячая клавиша работала всегда

        const ElementType type = info.type;
        connect(a, &QAction::triggered, this, [this, type] {
            if (CircuitView *v = currentView())
                v->setPlaceMode(type);
            syncToolActions();
        });
        m_elementActions[int(info.type)] = a;
    }
    refreshIcons();

    // Трассированный провод (как «Add Routed Wire» у Falstad): Г-образный провод одним движением.
    m_routedWireAction = new QAction(QStringLiteral("Трассированный провод (уголком)"), this);
    m_routedWireAction->setCheckable(true);
    m_routedWireAction->setShortcut(QKeySequence(QStringLiteral("Shift+W")));
    m_routedWireAction->setToolTip(QStringLiteral("Трассированный провод (Shift+W)"));
    m_toolGroup->addAction(m_routedWireAction);
    addAction(m_routedWireAction);
    connect(m_routedWireAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView())
            v->setPlaceMode(ElementType::Wire, true);
        syncToolActions();
    });

    m_escAction = new QAction(this);
    m_escAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_escAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView())
            v->escape();
        syncToolActions();
    });
    addAction(m_escAction);

    // --- файл ------------------------------------------------------------
    m_newTabAction = new QAction(QStringLiteral("Новая схема"), this);
    m_newTabAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(m_newTabAction, &QAction::triggered, this, [this] { newTab(); });

    m_karnaughAction = new QAction(QStringLiteral("Новая карта Карно"), this);
    connect(m_karnaughAction, &QAction::triggered, this, [this] { newKarnaughTab(); });

    m_newProjectAction = new QAction(QStringLiteral("Новый проект"), this);
    connect(m_newProjectAction, &QAction::triggered, this, [this] { newProject(); });

    m_openAction = new QAction(QStringLiteral("Открыть проект…"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, [this] { openProject(); });

    m_closeProjectAction = new QAction(QStringLiteral("Закрыть проект"), this);
    connect(m_closeProjectAction, &QAction::triggered, this, [this] { closeProject(0); });

    m_saveAction = new QAction(QStringLiteral("Сохранить проект"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, [this] { saveProject(0, false); });

    m_saveAsAction = new QAction(QStringLiteral("Сохранить проект как…"), this);
    m_saveAsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    connect(m_saveAsAction, &QAction::triggered, this, [this] { saveProject(0, true); });

    m_renameProjectAction = new QAction(QStringLiteral("Переименовать проект…"), this);
    connect(m_renameProjectAction, &QAction::triggered, this, [this] { renameProjectDialog(0); });

    m_closeTabAction = new QAction(QStringLiteral("Закрыть вкладку"), this);
    m_closeTabAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    connect(m_closeTabAction, &QAction::triggered, this, [this] {
        if (m_stack->currentIndex() == 1)
            m_karnaugh->closeCurrent();
        else
            closeTab(m_tabs->currentIndex());
    });

    m_menuAction = new QAction(QStringLiteral("В главное меню"), this);
    connect(m_menuAction, &QAction::triggered, this, [this] { emit backToMainMenu(); });

    m_settingsAction = new QAction(QStringLiteral("Настройки…"), this);
    connect(m_settingsAction, &QAction::triggered, this, [this] { showSettings(); });

    m_controlsAction = new QAction(QStringLiteral("Управление и горячие клавиши"), this);
    m_controlsAction->setShortcut(QKeySequence(Qt::Key_F1));
    connect(m_controlsAction, &QAction::triggered, this, [this] { showControls(); });

    m_aboutAction = new QAction(QStringLiteral("О программе"), this);
    connect(m_aboutAction, &QAction::triggered, this, [this] { showAbout(); });

    // --- правка ----------------------------------------------------------
    // Текст пунктов постоянный («Отменить»), а что именно отменится — в подсказке:
    // иначе кнопки на панели меняют ширину при каждом действии.
    m_undoAction = new QAction(QStringLiteral("Отменить"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, m_undoGroup, &QUndoGroup::undo);
    connect(m_undoGroup, &QUndoGroup::canUndoChanged, m_undoAction, &QAction::setEnabled);
    connect(m_undoGroup, &QUndoGroup::undoTextChanged, this, [this](const QString &t) {
        m_undoAction->setToolTip(t.isEmpty() ? QStringLiteral("Отменить")
                                             : QStringLiteral("Отменить: ") + t);
    });

    m_redoAction = new QAction(QStringLiteral("Повторить"), this);
    m_redoAction->setShortcuts({ QKeySequence(QKeySequence::Redo),
                                 QKeySequence(QStringLiteral("Ctrl+Shift+Z")) });
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered, m_undoGroup, &QUndoGroup::redo);
    connect(m_undoGroup, &QUndoGroup::canRedoChanged, m_redoAction, &QAction::setEnabled);
    connect(m_undoGroup, &QUndoGroup::redoTextChanged, this, [this](const QString &t) {
        m_redoAction->setToolTip(t.isEmpty() ? QStringLiteral("Повторить")
                                             : QStringLiteral("Повторить: ") + t);
    });

    m_cutAction = new QAction(QStringLiteral("Вырезать"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->cutSelected();
    });

    m_copyAction = new QAction(QStringLiteral("Копировать"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->copySelected();
    });

    m_pasteAction = new QAction(QStringLiteral("Вставить"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->pasteClipboard();
    });

    m_deleteAction = new QAction(QStringLiteral("Удалить"), this);
    m_deleteAction->setShortcuts({ QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace) });
    connect(m_deleteAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->deleteSelected();
    });

    m_selectAllAction = new QAction(QStringLiteral("Выделить всё"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->selectAll();
    });

    m_rotateCwAction = new QAction(QStringLiteral("Повернуть"), this);
    m_rotateCwAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    m_rotateCwAction->setToolTip(QStringLiteral("Повернуть по часовой стрелке (Ctrl+R)"));
    connect(m_rotateCwAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->rotateSelection(true);
    });

    m_rotateCcwAction = new QAction(QStringLiteral("Повернуть против часовой"), this);
    m_rotateCcwAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(m_rotateCcwAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->rotateSelection(false);
    });

    m_flipAction = new QAction(QStringLiteral("Отразить"), this);
    m_flipAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    m_flipAction->setToolTip(QStringLiteral("Отразить зеркально (Ctrl+F)"));
    connect(m_flipAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->flipSelection();
    });

    // Пробел обрабатывает сам холст, поэтому у пункта меню своей клавиши нет.
    m_toggleAction = new QAction(QStringLiteral("Переключить (Пробел)"), this);
    connect(m_toggleAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->toggleSelectedSwitches();
    });

    m_propsAction = new QAction(QStringLiteral("Свойства…"), this);
    m_propsAction->setShortcut(QKeySequence(Qt::Key_F2));
    connect(m_propsAction, &QAction::triggered, this, [this] {
        CircuitView *v = currentView();
        CircuitItem *item = v ? v->singleSelected() : nullptr;
        if (item)
            QTimer::singleShot(0, this, [this, item] { openElementDialog(item); });
    });

    // --- вид -------------------------------------------------------------
    m_zoomInAction = new QAction(QStringLiteral("Увеличить"), this);
    m_zoomInAction->setShortcuts({ QKeySequence(QKeySequence::ZoomIn),
                                   QKeySequence(QStringLiteral("Ctrl+=")) });
    connect(m_zoomInAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->zoomIn();
    });

    m_zoomOutAction = new QAction(QStringLiteral("Уменьшить"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->zoomOut();
    });

    m_fitAction = new QAction(QStringLiteral("Вписать схему"), this);
    m_fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+9")));
    connect(m_fitAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->fitToContent();
    });

    m_resetViewAction = new QAction(QStringLiteral("100%"), this);
    m_resetViewAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(m_resetViewAction, &QAction::triggered, this, [this] {
        if (CircuitView *v = currentView()) v->resetView();
    });
}

void EditorWindow::refreshIcons()
{
    const QColor color = Theme::instance().palette().text;

    if (m_selectAction)
        m_selectAction->setIcon(selectionIcon(color));
    for (const ElementInfo &info : allElements()) {
        QAction *a = m_elementActions.value(int(info.type), nullptr);
        if (a)
            a->setIcon(elementIcon(info.type, color));
    }
    if (m_navSimAction)
        m_navSimAction->setIcon(elementIcon(ElementType::Resistor, color));
    if (m_navKarnaughAction)
        m_navKarnaughAction->setIcon(karnaughIcon(color));
}

void EditorWindow::fillElementMenu(QMenu *menu)
{
    for (int c = 0; c < int(Category::Count_); ++c) {
        const Category cat = static_cast<Category>(c);
        QMenu *sub = nullptr;
        for (const ElementInfo &info : allElements()) {
            if (info.category != cat)
                continue;
            QAction *a = m_elementActions.value(int(info.type), nullptr);
            if (!a)
                continue;
            if (!sub)
                sub = menu->addMenu(QString::fromUtf8(categoryName(cat)));
            sub->addAction(a);
        }
    }
}

void EditorWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(QStringLiteral("Файл"));
    file->addAction(m_newTabAction);
    file->addAction(m_karnaughAction);
    file->addSeparator();
    file->addAction(m_newProjectAction);
    file->addAction(m_openAction);
    file->addAction(m_saveAction);
    file->addAction(m_saveAsAction);
    file->addAction(m_renameProjectAction);
    file->addAction(m_closeProjectAction);
    file->addSeparator();
    file->addAction(m_closeTabAction);
    file->addAction(m_menuAction);
    file->addSeparator();
    file->addAction(m_settingsAction);

    QMenu *edit = menuBar()->addMenu(QStringLiteral("Правка"));
    edit->addAction(m_undoAction);
    edit->addAction(m_redoAction);
    edit->addSeparator();
    edit->addAction(m_cutAction);
    edit->addAction(m_copyAction);
    edit->addAction(m_pasteAction);
    edit->addAction(m_deleteAction);
    edit->addSeparator();
    edit->addAction(m_selectAllAction);
    edit->addSeparator();
    edit->addAction(m_rotateCwAction);
    edit->addAction(m_rotateCcwAction);
    edit->addAction(m_flipAction);
    edit->addAction(m_toggleAction);
    edit->addAction(m_propsAction);

    m_elementsMenu = menuBar()->addMenu(QStringLiteral("Элементы"));
    m_elementsMenu->addAction(m_selectAction);
    m_elementsMenu->addSeparator();
    fillElementMenu(m_elementsMenu);

    m_viewMenu = menuBar()->addMenu(QStringLiteral("Вид"));
    m_viewMenu->addAction(m_zoomInAction);
    m_viewMenu->addAction(m_zoomOutAction);
    m_viewMenu->addAction(m_fitAction);
    m_viewMenu->addAction(m_resetViewAction);
    m_viewMenu->addSeparator();
    // пункт боковой панели добавляется в buildDock()

    QMenu *help = menuBar()->addMenu(QStringLiteral("Справка"));
    help->addAction(m_controlsAction);
    help->addAction(m_aboutAction);
}

void EditorWindow::buildToolBars()
{
    // Левая панель — переключатель разделов. Быстрых инструментов здесь нет:
    // элементы выбираются через меню «Элементы» и ПКМ по схеме.
    auto *nav = new QToolBar(QStringLiteral("Разделы"), this);
    nav->setObjectName(QStringLiteral("navBar"));
    nav->setOrientation(Qt::Vertical);
    nav->setMovable(false);
    nav->setFloatable(false);
    nav->setIconSize(QSize(28, 28));
    nav->setToolButtonStyle(Qt::ToolButtonIconOnly);       // узкая панель: только значки, подписи — в подсказках
    nav->setStyleSheet(QStringLiteral("QToolButton { padding: 10px; }"));

    m_navGroup = new QActionGroup(this);
    m_navGroup->setExclusive(true);

    m_navSimAction = new QAction(QStringLiteral("Симулятор\nэлектронных\nцепей"), this);
    m_navSimAction->setCheckable(true);
    m_navSimAction->setChecked(true);
    m_navSimAction->setToolTip(QStringLiteral("Симулятор электронных цепей"));
    m_navGroup->addAction(m_navSimAction);
    connect(m_navSimAction, &QAction::triggered, this, [this] { showSection(0); });

    m_navKarnaughAction = new QAction(QStringLiteral("Карты\nКарно"), this);
    m_navKarnaughAction->setCheckable(true);
    m_navKarnaughAction->setToolTip(QStringLiteral("Карты Карно"));
    m_navGroup->addAction(m_navKarnaughAction);
    connect(m_navKarnaughAction, &QAction::triggered, this, [this] { showSection(1); });

    nav->addAction(m_navSimAction);
    nav->addAction(m_navKarnaughAction);

    // Панель разделов — такая же док-панель, как остальные: её можно взять за заголовок сбоку,
    // перенести к любому краю окна или оставить плавающей. У верхнего и нижнего краёв кнопки
    // выстраиваются в ряд, у бокового и в плавающем виде — в столбик.
    auto *navDock = new QDockWidget(QStringLiteral("Разделы"), this);
    m_navDock = navDock;
    navDock->setObjectName(QStringLiteral("navDock"));
    navDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                         | QDockWidget::DockWidgetVerticalTitleBar);    // узкий заголовок сбоку, тянуть за него
    navDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    navDock->setWidget(nav);

    auto layoutNav = [nav](bool horizontal) {
        nav->setOrientation(horizontal ? Qt::Horizontal : Qt::Vertical);
        nav->setMaximumSize(horizontal ? QSize(QWIDGETSIZE_MAX, 64) : QSize(64, QWIDGETSIZE_MAX));
    };
    layoutNav(false);
    connect(navDock, &QDockWidget::dockLocationChanged, this, [layoutNav](Qt::DockWidgetArea area) {
        layoutNav(area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea);
    });
    connect(navDock, &QDockWidget::topLevelChanged, this, [layoutNav](bool floating) {
        if (floating)
            layoutNav(false);
    });
    addDockWidget(Qt::LeftDockWidgetArea, navDock);
    refreshIcons();
}

void EditorWindow::buildDock()
{
    m_sim = new SimulationPanel(this);
    m_panel = new PropertiesPanel(this);
    for (QWidget *w : { static_cast<QWidget *>(m_sim), static_cast<QWidget *>(m_panel) }) {
        w->setObjectName(QStringLiteral("dockRoot"));
        w->setAttribute(Qt::WA_StyledBackground, true);
        w->setMinimumWidth(260);
    }

    // Панели «Симуляция» и «Элемент» — отдельные окна-панели: их можно тянуть за заголовок,
    // закреплять у любого края окна, складывать во вкладки или оставлять плавающими (как в Visual Studio).
    setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);
    setDockNestingEnabled(true);

    auto makeDock = [this](const QString &title, const QString &name, QWidget *content) {
        auto *dock = new QDockWidget(title, this);
        dock->setObjectName(name);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setWidget(content);
        return dock;
    };
    // В тесном окне панель прокручивается, а не сжимает поля и кнопки друг на друга.
    auto scrollable = [this](QWidget *content) {
        auto *area = new QScrollArea(this);
        area->setObjectName(QStringLiteral("dockScroll"));
        area->setFrameShape(QFrame::NoFrame);
        area->setWidgetResizable(true);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        area->setMinimumWidth(300);                       // панель не должна схлопываться в узкую полоску
        area->setWidget(content);
        return area;
    };
    m_simDock = makeDock(QStringLiteral("Симуляция"), QStringLiteral("simDock"), scrollable(m_sim));
    m_propDock = makeDock(QStringLiteral("Элемент"), QStringLiteral("propDock"), scrollable(m_panel));
    addDockWidget(Qt::RightDockWidgetArea, m_simDock);
    addDockWidget(Qt::RightDockWidgetArea, m_propDock);
    resizeDocks({ m_simDock, m_propDock }, { 300, 300 }, Qt::Horizontal);

    // Нижняя панель осциллограмм: все осциллограммы сразу, рядом или друг над другом (меню ПКМ),
    // каждая со своими параметрами. Саму панель тоже можно двигать, закреплять и делать плавающей.
    m_scopeBoard = new ScopeBoard(this);
    connect(m_scopeBoard, &ScopeBoard::emptied, this, [this] { m_scopeDock->hide(); });
    connect(m_scopeBoard, &ScopeBoard::windowRequested, this, &EditorWindow::detachScope);
    connect(m_scopeBoard, &ScopeBoard::moveToSchemeRequested, this, &EditorWindow::scopeToScheme);
    m_scopeDock = makeDock(QStringLiteral("Осциллограммы"), QStringLiteral("scopeDock"), m_scopeBoard);
    addDockWidget(Qt::BottomDockWidgetArea, m_scopeDock);
    resizeDocks({ m_scopeDock }, { 240 }, Qt::Vertical);
    m_scopeDock->hide();                              // появляется, когда выбрали «Показать в нижней панели»

    // Журнал симуляции: почему остановили, какие элементы виноваты. Живёт во вкладке рядом с осциллограммами.
    m_log = new IssueLog(this);
    connect(m_log, &IssueLog::issueActivated, this, [this](quint64 uid, const QVector<int> &pins) {
        if (CircuitView *v = currentView())
            v->focusItem(uid, pins);
    });
    m_logDock = makeDock(QStringLiteral("Журнал симуляции"), QStringLiteral("logDock"), m_log);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    tabifyDockWidget(m_scopeDock, m_logDock);
    m_logDock->hide();

    QAction *logToggle = m_logDock->toggleViewAction();
    logToggle->setText(QStringLiteral("Панель «Журнал симуляции»"));
    m_viewMenu->addAction(logToggle);

    QAction *scopeToggle = m_scopeDock->toggleViewAction();
    scopeToggle->setText(QStringLiteral("Панель «Осциллограммы»"));
    m_viewMenu->addAction(scopeToggle);

    QAction *simToggle = m_simDock->toggleViewAction();
    simToggle->setText(QStringLiteral("Панель «Симуляция»"));
    m_viewMenu->addAction(simToggle);
    QAction *propToggle = m_propDock->toggleViewAction();
    propToggle->setText(QStringLiteral("Панель «Элемент»"));
    m_viewMenu->addAction(propToggle);

    // двойной клик / кнопка «Все характеристики…» -> окно свойств
    connect(m_panel, &PropertiesPanel::detailsRequested, this, [this](CircuitItem *item) {
        QTimer::singleShot(0, this, [this, item] { openElementDialog(item); });
    });

    // --- симуляция --------------------------------------------------------
    CircuitItem::setFlowSpeed(kBaseFlowSpeed * m_sim->currentSpeed());
    connect(m_sim, &SimulationPanel::currentSpeedChanged, this, [](double factor) {
        CircuitItem::setFlowSpeed(kBaseFlowSpeed * factor);
    });
    connect(m_sim, &SimulationPanel::runningChanged, this, &EditorWindow::onSimulationToggled);
    connect(m_sim, &SimulationPanel::resetClicked, this, [this] {
        for (CircuitView *v : allViews(m_tabs))
            v->resetSimulationState();
        clearIssues();
    });
    // simulationSpeedChanged и powerBrightnessChanged подключит ядро:
    // editor->simulationPanel() отдаёт панель целиком.
}

void EditorWindow::buildStatusBar()
{
    m_hintLabel = new QLabel(kSelectHint, this);
    m_posLabel = new QLabel(QStringLiteral("x: 0  y: 0"), this);
    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_countLabel = new QLabel(QStringLiteral("Элементов: 0"), this);
    m_posLabel->setMinimumWidth(110);
    m_zoomLabel->setMinimumWidth(56);

    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_hintLabel, 1);
    statusBar()->addPermanentWidget(m_countLabel);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

// ================================================================== вкладки

CircuitView *EditorWindow::currentView() const
{
    return m_view.data();
}

CircuitView *EditorWindow::addScheme(int projectId, const QString &name)
{
    ProjectHub &hub = ProjectHub::instance();
    if (!hub.find(projectId))
        projectId = hub.ensureActive();
    showSection(0, false);
    auto *view = new CircuitView;
    view->setFlowAnimation(m_sim && m_sim->isRunning());
    m_undoGroup->addStack(view->undoStack());
    const QString unique = name.isEmpty() ? hub.nextName(projectId, ProjectHub::Scheme)
                                          : hub.uniqueName(projectId, ProjectHub::Scheme, name);
    ProjectHub::attach(view, projectId, ProjectHub::Scheme, unique);
    connect(view->undoStack(), &QUndoStack::cleanChanged, view, [] { ProjectHub::instance().notify(); });   // «•» у проекта
    addPageTab(view);
    return view;
}

void EditorWindow::newTab()
{
    addScheme(ProjectHub::instance().ensureActive())->setFocus();
}

void EditorWindow::newKarnaughTab()
{
    const bool hadMaps = m_karnaugh->tabCount() > 0;
    showSection(1);                      // при первом входе в раздел он сам создаёт пустую карту
    if (hadMaps)
        m_karnaugh->newTab();
}

QWidget *EditorWindow::takeTab(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return nullptr;

    if (auto *view = qobject_cast<CircuitView *>(page)) {
        if (view == m_view)
            attachView(nullptr);
        m_undoGroup->removeStack(view->undoStack());
    }
    m_tabs->removeTab(index);           // страница остаётся жить: её заберёт другое окно
    ProjectHub::instance().notify();
    return page;
}

void EditorWindow::adoptTab(QWidget *page, int at, bool reveal)
{
    if (reveal)
        showSection(0, false);
    if (auto *view = qobject_cast<CircuitView *>(page)) {
        view->setFlowAnimation(m_sim && m_sim->isRunning());
        m_undoGroup->addStack(view->undoStack());
    }
    addPageTab(page, at);
}

void EditorWindow::adoptMap(QWidget *page, int at, bool reveal)
{
    m_karnaugh->adoptPage(page, at);
    if (reveal)
        showSection(1, false);
}

void EditorWindow::detachTab(int index, const QPoint &globalPos)
{
    if (m_tabs->count() < 2)
        return;                         // единственную вкладку окна не отрываем
    QWidget *page = takeTab(index);
    if (!page)
        return;

    auto *w = new EditorWindow(nullptr, true);
    w->resize(std::min(width(), 1200), std::min(height(), 800));
    w->move(globalPos - QPoint(80, 20));
    w->adoptTab(page);
    w->show();
    w->raise();
    w->activateWindow();
}

void EditorWindow::detachMap(int index, const QPoint &globalPos)
{
    if (m_karnaugh->tabCount() < 2)
        return;
    QWidget *page = m_karnaugh->takePage(index);
    if (!page)
        return;

    auto *w = new EditorWindow(nullptr, true);
    w->resize(std::min(width(), 1200), std::min(height(), 800));
    w->move(globalPos - QPoint(80, 20));
    w->adoptMap(page);
    w->show();
    w->raise();
    w->activateWindow();
}

void EditorWindow::checkEmptyAfterMove()
{
    if (m_tabs->count() > 0 || m_karnaugh->tabCount() > 0)
        return;
    // Унесли последнюю страницу — окно пустое: закрываем его (основное окно возвращается в меню).
    // Если перенос в нём ещё идёт, ждём его конца, чтобы не удалить полосу посреди перетаскивания.
    if (DetachableTabBar::dragActive())
        m_closeAfterDrag = true;
    else
        QTimer::singleShot(0, this, [this] { afterPagesChanged(); });
}

void EditorWindow::afterPagesChanged()
{
    const bool noSchemes = m_tabs->count() == 0;
    const bool noMaps = m_karnaugh->tabCount() == 0;
    if (noSchemes && noMaps) {
        if (m_standalone)
            close();
        else
            emit backToMainMenu();
        return;
    }
    if (m_section == 0 && noSchemes)
        showSection(1, false);
    else if (m_standalone && m_section == 1 && noMaps)
        showSection(0);
}

void EditorWindow::onTabDropped(DetachableTabBar *source, int sourceIndex, int targetIndex)
{
    QPointer<DetachableTabBar> src(source);
    // Переносим после выхода из обработчика drop: страница могла быть под курсором.
    QTimer::singleShot(0, this, [this, src, sourceIndex, targetIndex] {
        if (!src)
            return;
        auto *from = qobject_cast<EditorWindow *>(src->owner());
        if (!from)
            return;

        if (from == this) {             // вернули на ту же полосу — просто переставляем
            int to = targetIndex;
            if (to > sourceIndex)
                --to;
            if (to != sourceIndex && sourceIndex >= 0 && sourceIndex < m_tabs->count())
                m_tabs->bar()->moveTab(sourceIndex, std::min(to, m_tabs->count() - 1));
            return;
        }
        QWidget *page = from->takeTab(sourceIndex);
        if (!page)
            return;
        adoptTab(page, targetIndex);
        raise();
        activateWindow();
        from->checkEmptyAfterMove();
    });
}

void EditorWindow::onMapDropped(DetachableTabBar *source, int sourceIndex, int targetIndex)
{
    QPointer<DetachableTabBar> src(source);
    QTimer::singleShot(0, this, [this, src, sourceIndex, targetIndex] {
        if (!src)
            return;
        auto *from = qobject_cast<EditorWindow *>(src->owner());
        if (!from)
            return;

        if (from == this) {
            int to = targetIndex;
            if (to > sourceIndex)
                --to;
            DetachableTabBar *bar = m_karnaugh->bar();
            if (to != sourceIndex && sourceIndex >= 0 && sourceIndex < m_karnaugh->tabCount())
                bar->moveTab(sourceIndex, std::min(to, m_karnaugh->tabCount() - 1));
            return;
        }
        QWidget *page = from->m_karnaugh->takePage(sourceIndex);
        if (!page)
            return;
        adoptMap(page, targetIndex);
        raise();
        activateWindow();
        from->checkEmptyAfterMove();
    });
}

void EditorWindow::closeEvent(QCloseEvent *e)
{
    // Отрывное окно — лишь часть рабочей области: закрыли окно — его вкладки возвращаются в основное окно,
    // а схемы и карты остаются в своих проектах.
    if (m_standalone) {
        EditorWindow *main = nullptr;
        for (EditorWindow *w : std::as_const(allEditors()))
            if (!w->isStandalone())
                main = w;
        if (main) {
            while (m_tabs->count() > 0)
                if (QWidget *page = takeTab(0))
                    main->adoptTab(page, -1, false);
            while (m_karnaugh->tabCount() > 0)
                if (QWidget *page = m_karnaugh->takePage(0))
                    main->adoptMap(page, -1, false);
        }
    }
    QMainWindow::closeEvent(e);
}

void EditorWindow::showScopeInDock(CircuitItem *item)
{
    if (item)
        showScopeInDock(item->probe(true));
}

void EditorWindow::showScopeInDock(ScopeSource *source)
{
    if (!source)
        return;
    m_scopeBoard->addScope(source);
    m_scopeDock->show();
    m_scopeDock->raise();
}

void EditorWindow::detachScope(ScopeSource *source)
{
    if (!source)
        return;
    m_scopeBoard->removeScope(source);

    auto *win = new ScopeWindow(source);               // обычное окно приложения, его можно унести куда угодно
    QPointer<ScopeWindow> guard(win);
    connect(win, &ScopeWindow::dockRequested, this, [this, guard](ScopeSource *s) {
        showScopeInDock(s);
        if (guard)
            guard->close();
    });
    connect(win, &ScopeWindow::moveToSchemeRequested, this, [this, guard](ScopeSource *s) {
        scopeToScheme(s);
        if (guard)
            guard->close();
    });
    win->move(QCursor::pos() - QPoint(80, 30));
    win->show();
    win->raise();
    win->activateWindow();
}

void EditorWindow::scopeToScheme(ScopeSource *source)
{
    CircuitItem *target = source ? source->target() : nullptr;
    if (!target || !target->scene()) {
        QMessageBox::information(this, QStringLiteral("Осциллограмма"),
                                 QStringLiteral("Элемент этой осциллограммы удалён, на схему её не вернуть."));
        return;
    }
    CircuitView *owner = nullptr;                      // схема, где лежит элемент (не обязательно текущая)
    for (CircuitView *v : allViews(m_tabs))
        if (v->circuitScene() == target->scene())
            owner = v;
    if (!owner) {
        QMessageBox::information(this, QStringLiteral("Осциллограмма"),
                                 QStringLiteral("Элемент находится в другом окне, там и прикрепите осциллограмму."));
        return;
    }
    m_scopeBoard->removeScope(source);
    owner->addScope(target);
}

void EditorWindow::showScopeMenu(ScopeItem *item, const QPoint &globalPos)
{
    if (!item)
        return;
    ScopeSource *src = item->source();
    QPointer<ScopeItem> guard(item);
    QPointer<CircuitView> view(currentView());

    ScopeMenu::Actions a;
    a.remove = [view, guard] {
        if (view && guard)
            view->removeScope(guard);
    };
    a.moveText = QStringLiteral("Переместить осциллограф в нижнюю панель");
    a.move = [this, view, guard, src] {
        showScopeInDock(src);
        if (view && guard)
            view->removeScope(guard);
    };
    a.windowText = QStringLiteral("Вынести в отдельное окно");
    a.window = [this, view, guard, src] {
        if (view && guard)
            view->removeScope(guard);
        detachScope(src);
    };
    ScopeMenu::exec(this, src, globalPos, a);
}

void EditorWindow::onSimulationToggled(bool on)
{
    if (on) {
        // Проверки интерфейса до расчёта: сейчас — резистор у каждого подключённого вывода индикатора.
        if (CircuitView *v = currentView()) {
            const QVector<SimIssue> issues = Preflight::check(v->circuitScene());
            if (hasErrors(issues)) {
                stopSimulationWithIssues(issues);
                return;
            }
        }
        clearIssues();
        for (CircuitView *v : allViews(m_tabs))
            v->setFlowAnimation(true);
        m_log->addMessage(SimIssue::Severity::Info, QStringLiteral("Симуляция запущена"));
        // Здесь ядро получит схему и начнёт расчёт; результат — через CircuitItem::setElectrical / setLight,
        // а если оно найдёт проблему — через stopSimulationWithIssues().
    } else {
        for (CircuitView *v : allViews(m_tabs))
            v->setFlowAnimation(false);
        m_log->addMessage(SimIssue::Severity::Info, QStringLiteral("Симуляция остановлена"));
    }
}

void EditorWindow::stopSimulationWithIssues(const QVector<SimIssue> &issues)
{
    for (CircuitView *v : allViews(m_tabs))
        v->setFlowAnimation(false);                         // резко: точки тока и анимация гаснут сразу
    m_issues = issues;

    const SimIssue *first = nullptr;
    for (const SimIssue &i : issues)
        if (i.isError()) {
            first = &i;
            break;
        }
    if (!first && !issues.isEmpty())
        first = &issues.first();
    const QString reason = first ? first->text : QStringLiteral("причина не указана");

    m_sim->setStopped(reason);

    if (CircuitView *v = currentView()) {
        v->markProblems(issues);
        QString message = reason;
        if (issues.size() > 1)
            message += QStringLiteral(" (и ещё проблем: %1)").arg(issues.size() - 1);
        v->showStopBanner(QStringLiteral("Симуляция остановлена"), message);
    }

    m_log->addMessage(SimIssue::Severity::Error, QStringLiteral("Симуляция остановлена: проблем %1").arg(issues.size()));
    for (const SimIssue &i : issues)
        m_log->addIssue(i);
    m_logDock->show();
    m_logDock->raise();
}

void EditorWindow::clearIssues()
{
    m_issues.clear();
    for (CircuitView *v : allViews(m_tabs)) {
        v->clearProblems();
        v->hideStopBanner();
    }
    if (m_sim && !m_sim->isRunning())
        m_sim->setStatus(SimulationPanel::State::Idle, QStringLiteral("Готова к запуску"));
}

void EditorWindow::recheckIssues()
{
    CircuitView *v = currentView();
    if (!v || !m_sim || (!m_sim->isRunning() && m_issues.isEmpty()))
        return;
    const QVector<SimIssue> issues = Preflight::check(v->circuitScene());

    if (m_sim->isRunning()) {
        if (hasErrors(issues))
            stopSimulationWithIssues(issues);               // схему испортили на ходу — останавливаем
        return;
    }
    if (!hasErrors(issues)) {                               // после остановки всё исправили
        clearIssues();
        m_log->addMessage(SimIssue::Severity::Info, QStringLiteral("Проблемы устранены: симуляцию можно запускать"));
    } else if (issues.size() != m_issues.size()) {
        m_issues = issues;
        v->markProblems(issues);
    }
}

void EditorWindow::showSection(int section, bool autoCreate)
{
    section = section == 1 ? 1 : 0;
    m_stack->setCurrentIndex(section);
    if (m_navSimAction)
        m_navSimAction->setChecked(section == 0);
    if (m_navKarnaughAction)
        m_navKarnaughAction->setChecked(section == 1);

    if (section == 1 && autoCreate && m_karnaugh->tabCount() == 0)
        m_karnaugh->newTab();

    if (m_simDock && m_propDock && m_scopeDock && m_logDock) {   // панели относятся только к схемам
        if (section == 1 && m_section == 0) {
            m_simDockWas = !m_simDock->isHidden();
            m_propDockWas = !m_propDock->isHidden();
            m_scopeDockWas = !m_scopeDock->isHidden();
            m_logDockWas = !m_logDock->isHidden();
            m_logDock->hide();
            m_simDock->hide();
            m_propDock->hide();
            m_scopeDock->hide();
        } else if (section == 0 && m_section == 1) {
            m_simDock->setVisible(m_simDockWas);
            m_propDock->setVisible(m_propDockWas);
            m_scopeDock->setVisible(m_scopeDockWas);
            m_logDock->setVisible(m_logDockWas);
        }
        m_simDock->toggleViewAction()->setEnabled(section == 0);
        m_propDock->toggleViewAction()->setEnabled(section == 0);
        m_scopeDock->toggleViewAction()->setEnabled(section == 0);
        m_logDock->toggleViewAction()->setEnabled(section == 0);
    }
    m_section = section;
    if (section == 0)
        onTabChanged(m_tabs->currentIndex());
    else
        attachView(nullptr);
    QWidget *current = section == 1 ? m_karnaugh->currentPage() : m_tabs->currentWidget();
    if (current)
        ProjectHub::instance().setActive(ProjectHub::projectOf(current));
    if (m_projectPanel)
        m_projectPanel->setCurrentPage(current);
}

void EditorWindow::addPageTab(QWidget *page, int at)
{
    const QString label = ProjectHub::instance().tabLabel(page);
    const int index = at < 0 ? m_tabs->addTab(page, label) : m_tabs->insertTab(at, page, label);

    // Плоская кнопка «×» вместо стандартной иконки стиля. Закрытие — отложенно:
    // окно подтверждения не должно жить внутри обработчика клика по вкладке.
    auto *close = new QToolButton(m_tabs->bar());
    close->setObjectName(QStringLiteral("tabClose"));
    close->setText(QStringLiteral("×"));
    close->setToolTip(QStringLiteral("Закрыть вкладку"));
    close->setAutoRaise(true);
    close->setCursor(Qt::ArrowCursor);
    m_tabs->bar()->setTabButton(index, QTabBar::RightSide, close);
    QPointer<QWidget> guard = page;
    connect(close, &QToolButton::clicked, this, [this, guard] {
        QTimer::singleShot(0, this, [this, guard] {
            if (guard)
                closeTab(m_tabs->indexOf(guard));
        });
    });

    m_tabs->setCurrentIndex(index);
    refreshTabLabels();
    ProjectHub::instance().notify();
}

void EditorWindow::onTabChanged(int index)
{
    QWidget *page = m_tabs->widget(index);
    attachView(qobject_cast<CircuitView *>(page));
    if (page && m_section == 0)
        ProjectHub::instance().setActive(ProjectHub::projectOf(page));
    if (m_projectPanel)
        m_projectPanel->setCurrentPage(page);
}

void EditorWindow::attachView(CircuitView *view)
{
    for (const QMetaObject::Connection &c : std::as_const(m_viewConnections))
        QObject::disconnect(c);
    m_viewConnections.clear();

    m_view = view;
    m_panel->setView(view);

    if (!view) {
        m_undoGroup->setActiveStack(nullptr);
        syncEditActions();
        updateCount();
        return;
    }

    m_undoGroup->setActiveStack(view->undoStack());

    m_viewConnections << connect(view, &CircuitView::modeChanged, this, [this] { syncToolActions(); });
    m_viewConnections << connect(view, &CircuitView::selectionChanged, this, [this] { syncEditActions(); });
    m_viewConnections << connect(view, &CircuitView::cursorGridChanged, this, [this](const QPoint &p) {
        m_posLabel->setText(QStringLiteral("x: %1  y: %2").arg(p.x()).arg(p.y()));
    });
    m_viewConnections << connect(view, &CircuitView::zoomChanged, this, [this](qreal z) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(z * 100)));
    });
    // Двойной клик по элементу -> окно со всеми характеристиками. Открываем отложенно:
    // модальное окно не должно жить внутри обработчика мыши холста.
    m_viewConnections << connect(view, &CircuitView::itemActivated, this, [this](CircuitItem *item) {
        QTimer::singleShot(0, this, [this, item] { openElementDialog(item); });
    });
    m_viewConnections << connect(view, &CircuitView::contextMenuRequested,
                                 this, &EditorWindow::showContextMenu);
    m_viewConnections << connect(view, &CircuitView::scopeActivated, this, [this](ScopeItem *scope) {
        QPointer<ScopeItem> guard(scope);              // окно свойств — вне обработчика мыши холста
        QTimer::singleShot(0, this, [this, guard] {
            if (guard)
                ScopeDialog::edit(this, guard->source());
        });
    });
    m_viewConnections << connect(view, &CircuitView::scopeMenuRequested, this, &EditorWindow::showScopeMenu);
    m_viewConnections << connect(view->circuitScene(), &CircuitScene::topologyChanged, this, &EditorWindow::recheckIssues);
    m_viewConnections << connect(view, &CircuitView::bannerShowRequested, this, [this, view] {
        if (!m_issues.isEmpty())
            view->focusItem(m_issues.first().itemUid, m_issues.first().pins);
    });
    m_viewConnections << connect(view->undoStack(), &QUndoStack::indexChanged,
                                 this, [this] { updateCount(); });

    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(view->zoom() * 100)));
    syncToolActions();
    syncEditActions();
    updateCount();
}

void EditorWindow::removeTab(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;

    if (auto *view = qobject_cast<CircuitView *>(page)) {
        if (view == m_view)
            attachView(nullptr);
        m_undoGroup->removeStack(view->undoStack());
    }
    m_tabs->removeTab(index);       // currentChanged подключит соседнюю вкладку
    page->deleteLater();
    ProjectHub::instance().notify();
}

void EditorWindow::closeTab(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;

    auto *view = qobject_cast<CircuitView *>(page);
    if (view && !view->undoStack()->isClean()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Закрыть вкладку"),
            QStringLiteral("Вкладка «%1» содержит несохранённые изменения.\n"
                           "Закрыть без сохранения?").arg(ProjectHub::nameOf(page)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    removeTab(index);
    afterPagesChanged();                // закрыли последнюю страницу — возвращаемся в главное меню
}

void EditorWindow::closeTabsWhere(const QVector<QWidget *> &pages)
{
    if (pages.isEmpty())
        return;

    int dirty = 0;
    for (QWidget *w : pages)
        if (auto *v = qobject_cast<CircuitView *>(w); v && !v->undoStack()->isClean())
            ++dirty;
    if (dirty > 0) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Закрыть вкладки"),
            QStringLiteral("В закрываемых вкладках есть несохранённые изменения (%1).\n"
                           "Закрыть без сохранения?").arg(dirty),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    for (QWidget *w : pages) {
        const int i = m_tabs->indexOf(w);
        if (i >= 0)
            removeTab(i);
    }
    afterPagesChanged();
}

void EditorWindow::renameTab(int index)
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
    if (hub.nameTaken(ProjectHub::projectOf(page), ProjectHub::Scheme, text, page)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Схема с таким именем уже есть в этом проекте."));
        return;
    }
    ProjectHub::setNameOf(page, text);
    refreshTabLabels();
    hub.notify();
}

void EditorWindow::duplicateTab(int index)
{
    auto *src = qobject_cast<CircuitView *>(m_tabs->widget(index));
    if (!src)
        return;

    ProjectHub &hub = ProjectHub::instance();
    const int project = ProjectHub::projectOf(src);
    const QVector<ItemData> data = src->snapshot(false);
    const QString name = hub.uniqueName(project, ProjectHub::Scheme, ProjectHub::nameOf(src) + QStringLiteral(" (копия)"));

    CircuitView *dst = addScheme(project, name);          // новая вкладка становится текущей
    m_tabs->bar()->moveTab(m_tabs->indexOf(dst), index + 1);   // рядом с исходной
    dst->loadSnapshot(data, QStringLiteral("Копия схемы"));
    dst->fitToContent();
    hub.notify();
}

void EditorWindow::clearTab(int index)
{
    if (auto *view = qobject_cast<CircuitView *>(m_tabs->widget(index)))
        view->clearAll();                       // с отменой (Ctrl+Z)
}

void EditorWindow::showTabMenu(int index, const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *aNew = menu.addAction(QStringLiteral("Новая схема"));

    if (index < 0 || index >= m_tabs->count()) {            // пустое место на полосе вкладок
        if (menu.exec(globalPos) == aNew)
            newTab();
        return;
    }

    QAction *aDuplicate = menu.addAction(QStringLiteral("Дублировать"));
    QAction *aRename = menu.addAction(QStringLiteral("Переименовать…"));
    QAction *aClear = menu.addAction(QStringLiteral("Очистить схему"));
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

    if (chosen == aDuplicate) {
        duplicateTab(idx);
    } else if (chosen == aRename) {
        renameTab(idx);
    } else if (chosen == aClear) {
        clearTab(idx);
    } else if (chosen == aClose) {
        closeTab(idx);
    } else if (chosen == aOthers) {
        QVector<QWidget *> list;
        for (int i = 0; i < m_tabs->count(); ++i)
            if (i != idx)
                list.append(m_tabs->widget(i));
        closeTabsWhere(list);
    } else if (chosen == aRight) {
        QVector<QWidget *> list;
        for (int i = idx + 1; i < m_tabs->count(); ++i)
            list.append(m_tabs->widget(i));
        closeTabsWhere(list);
    } else if (chosen == aAll) {
        closeTabsWhere(allPages(m_tabs));
    }
}

bool EditorWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (m_tabs && obj == m_tabs->bar()) {
        switch (ev->type()) {
        case QEvent::ContextMenu: {                          // ПКМ по вкладке
            auto *ce = static_cast<QContextMenuEvent *>(ev);
            const int index = m_tabs->bar()->tabAt(ce->pos());
            const QPoint global = ce->globalPos();
            QTimer::singleShot(0, this, [this, index, global] { showTabMenu(index, global); });
            return true;
        }
        case QEvent::MouseButtonDblClick: {                  // двойной клик: переименовать / новая
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() != Qt::LeftButton)
                break;
            const int index = m_tabs->bar()->tabAt(me->position().toPoint());
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
            const int index = m_tabs->bar()->tabAt(me->position().toPoint());
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
    return QMainWindow::eventFilter(obj, ev);
}

// ============================================================ синхронизация

QString EditorWindow::placeHint(ElementType type) const
{
    const QString name = QString::fromUtf8(elementInfo(type).name);
    switch (type) {
    case ElementType::Wire:
        if (CircuitView *v = currentView(); v && v->routedWire())
            return QStringLiteral("Трассированный провод: протяните ЛКМ — провод ляжет «уголком» "
                                  "(сначала по горизонтали, с Shift — по вертикали) · Esc — выход");
        return QStringLiteral("%1: зажмите ЛКМ и протяните (Shift — строго по оси) · Esc — выход").arg(name);
    default:
        if (SymbolPainter::hasSinglePin(type))
            return QStringLiteral("%1: клик — поставить, протяжка — длина и направление · Esc — выход").arg(name);
        return QStringLiteral("%1: клик — поставить, протяжка ЛКМ — длина и направление · "
                              "Ctrl+R — повернуть · Esc — выход").arg(name);
    }
}

void EditorWindow::syncToolActions()
{
    CircuitView *v = currentView();
    if (!v)
        return;
    if (v->isSelectMode()) {
        m_selectAction->setChecked(true);
        m_hintLabel->setText(kSelectHint);
        return;
    }
    if (v->routedWire() && m_routedWireAction)
        m_routedWireAction->setChecked(true);
    else if (QAction *a = m_elementActions.value(int(v->placeType()), nullptr))
        a->setChecked(true);
    m_hintLabel->setText(placeHint(v->placeType()));
}

void EditorWindow::syncEditActions()
{
    CircuitView *v = currentView();
    const bool has = v && v->hasSelection();
    for (QAction *a : { m_cutAction, m_copyAction, m_deleteAction,
                        m_rotateCwAction, m_rotateCcwAction, m_flipAction })
        a->setEnabled(has);

    CircuitItem *single = v ? v->singleSelected() : nullptr;
    m_propsAction->setEnabled(single && single->hasProperties());
    m_toggleAction->setEnabled(v && v->selectionHasToggle());
}

void EditorWindow::updateCount()
{
    CircuitView *v = currentView();
    int n = 0;
    if (v) {
        for (CircuitItem *it : v->circuitScene()->circuitItems())
            if (!it->isPreview())
                ++n;
    }
    m_countLabel->setText(QStringLiteral("Элементов: %1").arg(n));
}

// ============================================================ меню и диалоги

namespace {

// Небольшие значки для меню действий над элементом (рисуются кодом, чтобы подстраиваться под тему).
enum class MenuGlyph { Props, RotateCw, RotateCcw, Flip, Copy, Cut, Trash, Toggle };

QIcon menuGlyph(MenuGlyph g, const QColor &color)
{
    const int S = 20;
    QPixmap pm(S * 2, S * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (g) {
    case MenuGlyph::Props:                                   // три движка
        for (int i = 0; i < 3; ++i) {
            const qreal y = 5.0 + i * 5.0;
            p.drawLine(QPointF(3, y), QPointF(17, y));
            p.setBrush(color);
            p.drawEllipse(QPointF(6.0 + ((i * 5) % 8) * 1.2, y), 1.9, 1.9);
            p.setBrush(Qt::NoBrush);
        }
        break;
    case MenuGlyph::RotateCw:
    case MenuGlyph::RotateCcw: {
        p.save();
        if (g == MenuGlyph::RotateCcw) {
            p.translate(S, 0);
            p.scale(-1, 1);
        }
        QPainterPath arc;
        arc.arcMoveTo(QRectF(4, 4, 12, 12), 60);
        arc.arcTo(QRectF(4, 4, 12, 12), 60, -270);
        p.drawPath(arc);
        QPolygonF head;
        head << QPointF(13.5, 1.8) << QPointF(16.5, 6.0) << QPointF(11.5, 6.2);
        p.setBrush(color);
        p.drawPolygon(head);
        p.restore();
        break;
    }
    case MenuGlyph::Flip: {
        p.setPen(QPen(color, 1.2, Qt::DashLine, Qt::RoundCap));
        p.drawLine(QPointF(10, 2.5), QPointF(10, 17.5));
        p.setPen(pen);
        QPolygonF l, r;
        l << QPointF(8.2, 5) << QPointF(8.2, 15) << QPointF(2.5, 15);
        r << QPointF(11.8, 5) << QPointF(11.8, 15) << QPointF(17.5, 15);
        p.drawPolygon(l);
        p.setBrush(color);
        p.drawPolygon(r);
        break;
    }
    case MenuGlyph::Copy:
        p.drawRoundedRect(QRectF(7.5, 3, 9, 11), 1.8, 1.8);
        p.drawRoundedRect(QRectF(3.5, 6.5, 9, 11), 1.8, 1.8);
        break;
    case MenuGlyph::Cut:
        p.drawEllipse(QPointF(5.5, 14.5), 2.4, 2.4);
        p.drawEllipse(QPointF(14.5, 14.5), 2.4, 2.4);
        p.drawLine(QPointF(7.0, 12.6), QPointF(14.0, 3.0));
        p.drawLine(QPointF(13.0, 12.6), QPointF(6.0, 3.0));
        break;
    case MenuGlyph::Trash:
        p.drawLine(QPointF(3.5, 5.5), QPointF(16.5, 5.5));
        p.drawLine(QPointF(8, 5.5), QPointF(8, 3.5));
        p.drawLine(QPointF(8, 3.5), QPointF(12, 3.5));
        p.drawLine(QPointF(12, 3.5), QPointF(12, 5.5));
        p.drawPolyline(QPolygonF({ QPointF(5, 5.5), QPointF(5.8, 17), QPointF(14.2, 17), QPointF(15, 5.5) }));
        p.drawLine(QPointF(8.3, 8.5), QPointF(8.3, 14));
        p.drawLine(QPointF(11.7, 8.5), QPointF(11.7, 14));
        break;
    case MenuGlyph::Toggle:
        p.drawRoundedRect(QRectF(2.5, 5.5, 15, 9), 4.5, 4.5);
        p.setBrush(color);
        p.drawEllipse(QPointF(13.0, 10), 2.6, 2.6);
        break;
    }
    p.end();
    return QIcon(pm);
}

} // namespace

void EditorWindow::showContextMenu(const QPoint &globalPos)
{
    CircuitView *v = currentView();
    if (!v)
        return;
    syncEditActions();

    // ПКМ по элементу — меню действий над ним (а не список всех элементов).
    if (CircuitItem *target = v->contextItem()) {
        const QColor col = Theme::instance().palette().text;

        QMenu menu(this);
        menu.setMinimumWidth(210);

        m_propsAction->setIcon(menuGlyph(MenuGlyph::Props, col));
        m_toggleAction->setIcon(menuGlyph(MenuGlyph::Toggle, col));
        m_rotateCwAction->setIcon(menuGlyph(MenuGlyph::RotateCw, col));
        m_rotateCcwAction->setIcon(menuGlyph(MenuGlyph::RotateCcw, col));
        m_flipAction->setIcon(menuGlyph(MenuGlyph::Flip, col));
        m_copyAction->setIcon(menuGlyph(MenuGlyph::Copy, col));
        m_cutAction->setIcon(menuGlyph(MenuGlyph::Cut, col));
        m_deleteAction->setIcon(menuGlyph(MenuGlyph::Trash, QColor(QStringLiteral("#e5675d"))));

        if (m_propsAction->isEnabled())
            menu.addAction(m_propsAction);
        if (v->selectionHasToggle())
            menu.addAction(m_toggleAction);

        // Осциллограмма элемента: на схему отдельным окошком или в нижнюю панель.
        if (!target->isDecoration()) {
            menu.addSeparator();
            QMenu *scopeMenu = menu.addMenu(QStringLiteral("Осциллограмма"));
            QAction *onScheme = scopeMenu->addAction(QStringLiteral("Прикрепить к схеме (новый элемент)"));
            QAction *inDock = scopeMenu->addAction(QStringLiteral("Показать в нижней панели"));
            connect(onScheme, &QAction::triggered, this, [v, target] { v->addScope(target); });
            connect(inDock, &QAction::triggered, this, [this, target] { showScopeInDock(target); });
        }
        menu.addSeparator();
        menu.addAction(m_rotateCwAction);
        menu.addAction(m_rotateCcwAction);
        menu.addAction(m_flipAction);
        menu.addSeparator();
        menu.addAction(m_cutAction);
        menu.addAction(m_copyAction);
        menu.addSeparator();
        menu.addAction(m_deleteAction);
        menu.exec(globalPos);
        return;
    }

    // Как у Falstad: сверху — инструмент выбора и самое частое, ниже — ВСЕ элементы
    // по категориям, в конце — действия над выделенным.
    QMenu menu(this);
    menu.addAction(m_selectAction);
    menu.addSeparator();
    for (ElementType t : { ElementType::Wire, ElementType::Resistor, ElementType::Capacitor,
                           ElementType::Inductor, ElementType::VoltageSourceDC, ElementType::Ground }) {
        if (QAction *a = m_elementActions.value(int(t), nullptr))
            menu.addAction(a);
        if (t == ElementType::Wire && m_routedWireAction)
            menu.addAction(m_routedWireAction);
    }
    menu.addSeparator();
    fillElementMenu(&menu);
    menu.addSeparator();

    if (v->hasSelection()) {
        menu.addAction(m_cutAction);
        menu.addAction(m_copyAction);
    }
    menu.addAction(m_pasteAction);
    if (v->hasSelection()) {
        menu.addAction(m_deleteAction);
        menu.addSeparator();
        menu.addAction(m_rotateCwAction);
        menu.addAction(m_rotateCcwAction);
        menu.addAction(m_flipAction);
        if (v->selectionHasToggle())
            menu.addAction(m_toggleAction);
        if (m_propsAction->isEnabled())
            menu.addAction(m_propsAction);
    }
    menu.addSeparator();
    menu.addAction(m_selectAllAction);
    menu.exec(globalPos);
}

void EditorWindow::openElementDialog(CircuitItem *item)
{
    CircuitView *v = currentView();
    if (!v || !item)
        return;
    // элемент мог исчезнуть, пока ждали отложенного вызова
    if (!v->circuitScene()->circuitItems().contains(item))
        return;
    if (!item->hasProperties())
        return;                                   // у провода, выключателей и кнопок окна свойств нет

    ElementDialog dlg(item, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QMap<QString, double> values = dlg.values();
    const bool textChanged = dlg.hasText() && dlg.text() != item->text();
    bool paramsChanged = false;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        paramsChanged = paramsChanged || item->param(it.key()) != it.value();
    if (!textChanged && !paramsChanged)
        return;
    v->undoStack()->beginMacro(QStringLiteral("Свойства: ") + item->displayName().toLower());
    if (textChanged)
        v->setItemText(item, dlg.text());
    if (paramsChanged)
        v->setItemParams(item, values);
    v->undoStack()->endMacro();
}

void EditorWindow::showSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void EditorWindow::showControls()
{
    QStringList keys;
    for (const KeyDef &k : kKeys)
        keys << QStringLiteral("<b>%1</b> — %2")
                    .arg(QString::fromLatin1(k.key),
                         QString::fromUtf8(elementInfo(k.type).name).toLower());

    const QString text = QStringLiteral(
        "<b>Перемещение</b><br>"
        "• тянуть элемент — переносятся выделенные элементы, провода остаются на месте;<br>"
        "• тянуть вывод (кружок на конце) — переносится узел, все концы в этой точке;<br>"
        "• <b>Shift</b> + тянуть — элемент переносится целиком, даже если схвачен за край или вывод;<br>"
        "• <b>Alt</b> + тянуть — как Shift, но подключённые провода растягиваются следом;<br>"
        "• земля, шины, антенна, метки — тянутся, как любой элемент: удлиняется ножка, знак остаётся на конце;<br>"
        "• <b>Shift+W</b> — трассированный провод: одним движением ложится «уголком»;<br>"
        "• «Надпись», «Прямоугольник» и «Линия» (категория «Провода, земля, метки, оформление») — "
        "только оформление схемы, ни с чем не соединяются; текст надписи — двойной клик;<br>"
        "• клик ЛКМ по выключателю, кнопке, переключателю или логическому входу — переключить "
        "(окна свойств у них нет);<br>"
        "• клавиатура: клавиша нажата, пока держите кнопку мыши; DIP-переключатель: клик по нужной позиции; "
        "двигать такой элемент — Shift + тянуть, свойства — двойной клик по краю или F2;<br>"
        "• <b>колесо мыши над элементом</b> — менять главную характеристику (сопротивление, ёмкость, "
        "индуктивность, напряжение, ток…); у переменных резисторов, конденсаторов и катушек — положение движка; "
        "<b>Shift</b> + колесо — номинал или частоту; <b>Ctrl</b> + колесо — масштаб над любым элементом;<br>"
        "• стрелки — сдвиг на клетку, Shift + стрелки — на 5 клеток.<br><br>"
        "<b>Правка</b><br>"
        "• <b>Ctrl+R</b> — повернуть, <b>Ctrl+Shift+R</b> — против часовой, <b>Ctrl+F</b> — отразить;<br>"
        "• <b>Пробел</b> — переключить выбранные ключи и кнопки;<br>"
        "• двойной клик или <b>F2</b> — окно со всеми характеристиками элемента;<br>"
        "• ПКМ по элементу — меню действий над ним; ПКМ по пустому месту — все элементы по категориям; "
        "ПКМ по вкладке — действия со схемой;<br>"
        "• <b>Esc</b> — отмена, затем режим выбора, затем снять выделение.<br><br>"
        "<b>Красный конец</b> — вывод стоит вплотную к чужому узлу или лежит на чужом проводе, "
        "но не соединён с ним (отключается в «Настройках»).<br><br>"
        "<b>Быстрый выбор элемента</b><br>"
        "<b>S</b> — выбор; %1.")
        .arg(keys.join(QStringLiteral("; ")));

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Управление"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void EditorWindow::showAbout()
{
    QMessageBox::about(this, QStringLiteral("О программе"),
        QStringLiteral("<b>Симулятор электрических цепей</b><br>"
                       "Клон идеи Falstad Circuit Simulator на C++ / Qt.<br>"
                       "Условные обозначения — по ГОСТ 2.721-74 и смежным стандартам ЕСКД.<br><br>"
                       "Готова визуальная оболочка (gui). Расчёт цепей будет подключён из ядра (core)."));
}

// ================================================================== проекты

namespace {

QVector<EditorWindow *> projectEditors()
{
    QVector<EditorWindow *> out;                        // основное окно первым, затем оторванные
    for (EditorWindow *w : std::as_const(allEditors()))
        if (!w->isStandalone())
            out.append(w);
    for (EditorWindow *w : std::as_const(allEditors()))
        if (w->isStandalone())
            out.append(w);
    return out;
}

const QString kProjectFilter = QStringLiteral("Проект GOVNO (*.govno)");

} // namespace

void EditorWindow::buildProjectDock()
{
    m_projectPanel = new ProjectPanel(this);
    m_projectDock = new QDockWidget(QStringLiteral("Проекты"), this);
    m_projectDock->setObjectName(QStringLiteral("projectDock"));
    m_projectDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                               | QDockWidget::DockWidgetClosable);
    m_projectDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_projectDock->setWidget(m_projectPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);
    if (m_navDock)
        splitDockWidget(m_navDock, m_projectDock, Qt::Horizontal);   // рядом с узкой панелью разделов

    QAction *toggle = m_projectDock->toggleViewAction();
    toggle->setText(QStringLiteral("Панель «Проекты»"));
    m_viewMenu->addAction(toggle);

    connect(&ProjectHub::instance(), &ProjectHub::changed, this, &EditorWindow::refreshProjectPanel);
    connect(m_projectPanel, &ProjectPanel::pageActivated, this, [](QWidget *page) { activatePage(page); });
    connect(m_projectPanel, &ProjectPanel::pageRenameRequested, this, [](QWidget *page) { renamePage(page); });
    connect(m_projectPanel, &ProjectPanel::pageCloseRequested, this, [](QWidget *page) { closePage(page); });
    connect(m_projectPanel, &ProjectPanel::newSchemeRequested, this, [this](int id) { addSchemeTo(id); });
    connect(m_projectPanel, &ProjectPanel::newMapRequested, this, [this](int id) { addMapTo(id); });
    connect(m_projectPanel, &ProjectPanel::projectActivated, this, [this](int id) { activateProject(id); });
    connect(m_projectPanel, &ProjectPanel::projectRenameRequested, this, [this](int id) { renameProjectDialog(id); });
    connect(m_projectPanel, &ProjectPanel::projectSaveRequested, this, [this](int id) { saveProject(id, false); });
    connect(m_projectPanel, &ProjectPanel::projectSaveAsRequested, this, [this](int id) { saveProject(id, true); });
    connect(m_projectPanel, &ProjectPanel::projectExplorerRequested, this, [this](int id) { openProjectInExplorer(id); });
    connect(m_projectPanel, &ProjectPanel::projectHideToggled, this, [this](int id) {
        const ProjectInfo *p = ProjectHub::instance().find(id);
        if (p)
            setProjectHidden(id, !p->hidden);
    });
    connect(m_projectPanel, &ProjectPanel::projectCloseRequested, this, [this](int id) { closeProject(id); });
    connect(m_projectPanel, &ProjectPanel::projectDeleteRequested, this, [this](int id) { deleteProject(id); });
    connect(m_projectPanel, &ProjectPanel::newProjectRequested, this, [this] { newProject(); });
    connect(m_projectPanel, &ProjectPanel::openProjectRequested, this, [this] { openProject(); });
}

void EditorWindow::refreshTabLabels()
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
    m_karnaugh->refreshLabels();
}

void EditorWindow::refreshProjectPanel()
{
    if (!m_projectPanel)
        return;
    const ProjectHub &hub = ProjectHub::instance();
    QVector<ProjectPanel::ProjectEntry> list;
    for (const ProjectInfo &p : hub.projects()) {
        ProjectPanel::ProjectEntry e;
        e.id = p.id;
        e.name = p.name;
        e.path = p.path;
        e.hidden = p.hidden;
        e.active = p.id == hub.activeId();
        list.append(e);
    }
    auto entryFor = [&list](int id) -> ProjectPanel::ProjectEntry * {
        for (ProjectPanel::ProjectEntry &e : list)
            if (e.id == id)
                return &e;
        return nullptr;
    };
    for (EditorWindow *w : projectEditors()) {
        const QString note = w->isStandalone() ? QStringLiteral("в отдельном окне") : QString();
        for (int i = 0; i < w->m_tabs->count(); ++i) {
            QWidget *page = w->m_tabs->widget(i);
            ProjectPanel::ProjectEntry *e = entryFor(ProjectHub::projectOf(page));
            if (!e)
                continue;
            e->schemes.append({ ProjectHub::nameOf(page), page, note });
            if (auto *v = qobject_cast<CircuitView *>(page); v && !v->undoStack()->isClean())
                e->dirty = true;
        }
        for (int i = 0; i < w->m_karnaugh->tabCount(); ++i) {
            QWidget *page = w->m_karnaugh->page(i);
            if (ProjectPanel::ProjectEntry *e = entryFor(ProjectHub::projectOf(page)))
                e->maps.append({ ProjectHub::nameOf(page), page, note });
        }
    }
    m_projectPanel->setContent(list);
    m_projectPanel->setCurrentPage(m_section == 1 ? m_karnaugh->currentPage() : m_tabs->currentWidget());
}

EditorWindow *EditorWindow::ownerOf(QWidget *page)
{
    for (EditorWindow *w : projectEditors())
        if (w->m_tabs->indexOf(page) >= 0 || w->m_karnaugh->contains(page))
            return w;
    return nullptr;
}

void EditorWindow::applyProjectVisibility(int projectId)
{
    const ProjectInfo *info = ProjectHub::instance().find(projectId);
    const bool visible = info && !info->hidden;
    for (QWidget *page : ProjectHub::instance().pagesOf(projectId)) {
        EditorWindow *w = ownerOf(page);
        if (!w)
            continue;
        const int i = w->m_tabs->indexOf(page);
        if (i < 0) {
            w->m_karnaugh->setPageVisible(page, visible);
            continue;
        }
        w->m_tabs->setTabVisible(i, visible);
        if (!visible && w->m_tabs->currentIndex() == i)
            for (int k = 0; k < w->m_tabs->count(); ++k)
                if (w->m_tabs->isTabVisible(k)) {
                    w->m_tabs->setCurrentIndex(k);
                    break;
                }
    }
}

void EditorWindow::setProjectHidden(int projectId, bool hidden)
{
    ProjectHub::instance().setHidden(projectId, hidden);
    applyProjectVisibility(projectId);
}

bool EditorWindow::activatePage(QWidget *page)
{
    EditorWindow *w = ownerOf(page);
    if (!w)
        return false;
    const int project = ProjectHub::projectOf(page);
    const ProjectInfo *info = ProjectHub::instance().find(project);
    if (info && info->hidden)
        setProjectHidden(project, false);
    const int i = w->m_tabs->indexOf(page);
    if (i >= 0) {
        w->showSection(0, false);
        w->m_tabs->setCurrentIndex(i);
    } else {
        w->showSection(1, false);
        w->m_karnaugh->select(page);
    }
    w->window()->raise();
    w->window()->activateWindow();
    return true;
}

void EditorWindow::activateProject(int projectId)
{
    ProjectHub &hub = ProjectHub::instance();
    const ProjectInfo *info = hub.find(projectId);
    if (!info)
        return;
    if (info->hidden)
        setProjectHidden(projectId, false);
    hub.setActive(projectId);
    const QVector<QWidget *> pages = hub.pagesOf(projectId);
    if (!pages.isEmpty())
        activatePage(pages.first());
}

void EditorWindow::addSchemeTo(int projectId)
{
    const ProjectInfo *info = ProjectHub::instance().find(projectId);
    if (!info)
        return;
    if (info->hidden)
        setProjectHidden(projectId, false);
    addScheme(projectId)->setFocus();
}

void EditorWindow::addMapTo(int projectId)
{
    const ProjectInfo *info = ProjectHub::instance().find(projectId);
    if (!info)
        return;
    if (info->hidden)
        setProjectHidden(projectId, false);
    m_karnaugh->addMap(projectId);
    showSection(1, false);
}

void EditorWindow::renamePage(QWidget *page)
{
    EditorWindow *w = ownerOf(page);
    if (!w)
        return;
    const int i = w->m_tabs->indexOf(page);
    if (i >= 0)
        w->renameTab(i);
    else
        w->m_karnaugh->renamePage(page);
}

void EditorWindow::closePage(QWidget *page)
{
    EditorWindow *w = ownerOf(page);
    if (!w)
        return;
    QPointer<QWidget> guard(page);
    QPointer<EditorWindow> window(w);
    if (w->m_tabs->indexOf(page) >= 0) {
        QTimer::singleShot(0, w, [window, guard] {            // окно вопроса — вне обработчика меню
            if (window && guard)
                window->closeTab(window->m_tabs->indexOf(guard));
        });
    } else {
        w->m_karnaugh->closePage(page);
    }
}

ProjectDoc EditorWindow::collectProject(int projectId) const
{
    ProjectDoc doc;
    if (const ProjectInfo *info = ProjectHub::instance().find(projectId))
        doc.name = info->name;
    for (EditorWindow *w : projectEditors()) {
        for (int i = 0; i < w->m_tabs->count(); ++i) {
            QWidget *page = w->m_tabs->widget(i);
            if (ProjectHub::projectOf(page) != projectId)
                continue;
            if (auto *view = qobject_cast<CircuitView *>(page))
                doc.schemes.append({ ProjectHub::nameOf(page), view->snapshot(false) });
        }
        for (int i = 0; i < w->m_karnaugh->tabCount(); ++i) {
            QWidget *page = w->m_karnaugh->page(i);
            if (ProjectHub::projectOf(page) != projectId)
                continue;
            if (auto *map = qobject_cast<KarnaughWidget *>(page)) {
                const KarnaughWidget::State st = map->state();
                MapDoc m;
                m.name = ProjectHub::nameOf(page);
                m.width = st.width;
                m.height = st.height;
                m.cells = st.cells;
                m.names = st.names;
                m.namesEdited = st.namesEdited;
                doc.maps.append(m);
            }
        }
    }
    return doc;
}

bool EditorWindow::projectDirty(int projectId) const
{
    for (QWidget *page : ProjectHub::instance().pagesOf(projectId))
        if (auto *v = qobject_cast<CircuitView *>(page); v && !v->undoStack()->isClean())
            return true;
    return false;
}

int EditorWindow::createProjectFromDoc(const ProjectDoc &doc, const QString &path)
{
    ProjectHub &hub = ProjectHub::instance();
    const QString base = !doc.name.isEmpty() ? doc.name
                       : !path.isEmpty() ? QFileInfo(path).completeBaseName() : QStringLiteral("Новый проект");
    const int id = hub.create(hub.uniqueProjectName(base), path);

    QWidget *firstScheme = nullptr;
    for (const SchemeDoc &s : doc.schemes) {
        CircuitView *view = addScheme(id, s.name);
        view->loadSnapshot(s.items, QString());
        view->fitToContent();
        view->undoStack()->clear();
        view->undoStack()->setClean();
        if (!firstScheme)
            firstScheme = view;
    }
    for (const MapDoc &m : doc.maps) {
        if (KarnaughWidget *map = m_karnaugh->addMap(id, m.name)) {
            KarnaughWidget::State st;
            st.width = m.width;
            st.height = m.height;
            st.cells = m.cells;
            st.names = m.names;
            st.namesEdited = m.namesEdited;
            map->setState(st);
        }
    }
    if (doc.schemes.isEmpty() && doc.maps.isEmpty())
        firstScheme = addScheme(id);

    hub.setActive(id);
    if (firstScheme) {
        showSection(0, false);
        m_tabs->setCurrentWidget(firstScheme);
    } else {
        showSection(1, false);
        m_karnaugh->select(m_karnaugh->page(m_karnaugh->tabCount() - 1));
    }
    return id;
}

void EditorWindow::newProject()
{
    ProjectHub &hub = ProjectHub::instance();
    const int id = hub.create(hub.uniqueProjectName(QStringLiteral("Новый проект")));
    addScheme(id);
    refreshProjectPanel();
}

void EditorWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Открыть проект"), QString(), kProjectFilter);
    if (!path.isEmpty())
        openProjectFile(path);
}

bool EditorWindow::openProjectFile(const QString &path)
{
    ProjectHub &hub = ProjectHub::instance();
    if (const int already = hub.findByPath(path)) {          // проект уже открыт — просто переходим к нему
        activateProject(already);
        return true;
    }
    ProjectDoc doc;
    QString error;
    int skipped = 0;
    if (!ProjectFile::load(path, &doc, &error, &skipped)) {
        QMessageBox::warning(this, QStringLiteral("Открыть проект"),
                             QStringLiteral("Не удалось открыть «%1»: %2.").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    createProjectFromDoc(doc, path);
    refreshProjectPanel();
    if (skipped > 0)
        QMessageBox::information(this, QStringLiteral("Открыть проект"),
            QStringLiteral("Элементов неизвестного типа пропущено: %1. Файл создан другой версией программы.").arg(skipped));
    statusBar()->showMessage(QStringLiteral("Проект открыт: %1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

bool EditorWindow::saveProject(int projectId, bool saveAs)
{
    ProjectHub &hub = ProjectHub::instance();
    if (projectId == 0)
        projectId = hub.ensureActive();
    const ProjectInfo *info = hub.find(projectId);
    if (!info)
        return false;

    QString path = info->path;
    if (saveAs || path.isEmpty()) {
        const QString suggestion = info->name + QLatin1Char('.') + ProjectFile::kExtension;
        path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить проект «%1»").arg(info->name),
                                            path.isEmpty() ? suggestion : path, kProjectFilter);
        if (path.isEmpty())
            return false;
        if (QFileInfo(path).suffix().isEmpty())
            path += QLatin1Char('.') + ProjectFile::kExtension;
        const int other = hub.findByPath(path);
        if (other && other != projectId) {
            QMessageBox::warning(this, QStringLiteral("Сохранить проект"),
                                 QStringLiteral("Этот файл уже занят другим открытым проектом."));
            return false;
        }
        if (info->name.startsWith(QStringLiteral("Новый проект")))
            hub.setName(projectId, hub.uniqueProjectName(QFileInfo(path).completeBaseName()));   // первое сохранение: имя — по файлу
    }

    QString error;
    if (!ProjectFile::save(path, collectProject(projectId), &error)) {
        QMessageBox::warning(this, QStringLiteral("Сохранить проект"),
                             QStringLiteral("Не удалось сохранить «%1»: %2.").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    hub.setPath(projectId, path);
    for (QWidget *page : hub.pagesOf(projectId))
        if (auto *v = qobject_cast<CircuitView *>(page))
            v->undoStack()->setClean();
    statusBar()->showMessage(QStringLiteral("Проект сохранён: %1").arg(QDir::toNativeSeparators(path)), 5000);
    hub.notify();
    return true;
}

void EditorWindow::closeProject(int projectId, bool askToSave)
{
    ProjectHub &hub = ProjectHub::instance();
    if (projectId == 0)
        projectId = hub.activeId();
    const ProjectInfo *info = hub.find(projectId);
    if (!info)
        return;
    if (askToSave && projectDirty(projectId)) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Закрыть проект"),
            QStringLiteral("В проекте «%1» есть несохранённые изменения. Сохранить их?").arg(info->name),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (answer == QMessageBox::Cancel)
            return;
        if (answer == QMessageBox::Save && !saveProject(projectId, false))
            return;
    }

    QVector<QPointer<EditorWindow>> owners;
    for (QWidget *page : hub.pagesOf(projectId)) {
        EditorWindow *w = ownerOf(page);
        if (!w)
            continue;
        const int i = w->m_tabs->indexOf(page);
        if (i >= 0)
            w->removeTab(i);
        else
            w->m_karnaugh->closePage(page);
        owners.append(w);
    }
    hub.remove(projectId);
    for (const QPointer<EditorWindow> &w : std::as_const(owners))
        if (w)
            w->afterPagesChanged();
}

void EditorWindow::deleteProject(int projectId)
{
    ProjectHub &hub = ProjectHub::instance();
    const ProjectInfo *info = hub.find(projectId);
    if (!info)
        return;
    const QString name = info->name;
    const QString path = info->path;
    const bool onDisk = !path.isEmpty() && QFile::exists(path);
    const QString text = onDisk
        ? QStringLiteral("Удалить проект «%1»?\nФайл «%2» будет удалён с диска без возможности восстановления, "
                         "вкладки проекта закроются.").arg(name, QDir::toNativeSeparators(path))
        : QStringLiteral("Удалить проект «%1»?\nУ него нет файла на диске: вкладки закроются, несохранённые "
                         "изменения пропадут.").arg(name);
    if (QMessageBox::question(this, QStringLiteral("Удалить проект"), text,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (onDisk && !QFile::remove(path)) {
        QMessageBox::warning(this, QStringLiteral("Удалить проект"),
                             QStringLiteral("Не удалось удалить файл «%1».").arg(QDir::toNativeSeparators(path)));
        return;
    }
    closeProject(projectId, false);
}

void EditorWindow::openProjectInExplorer(int projectId)
{
    const ProjectInfo *info = ProjectHub::instance().find(projectId);
    if (!info)
        return;
    if (info->path.isEmpty() || !QFile::exists(info->path)) {
        QMessageBox::information(this, QStringLiteral("Открыть в проводнике"),
                                 QStringLiteral("Проект «%1» ещё не сохранён: у него нет файла на диске.").arg(info->name));
        return;
    }
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            { QStringLiteral("/select,"), QDir::toNativeSeparators(info->path) });   // папка с выделенным файлом
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(info->path).absolutePath()));
#endif
}

void EditorWindow::renameProjectDialog(int projectId)
{
    ProjectHub &hub = ProjectHub::instance();
    if (projectId == 0)
        projectId = hub.ensureActive();
    const ProjectInfo *info = hub.find(projectId);
    if (!info)
        return;
    bool ok = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("Переименовать проект"),
                                               QStringLiteral("Название проекта:"), QLineEdit::Normal,
                                               info->name, &ok).trimmed();
    if (ok && !text.isEmpty())
        hub.setName(projectId, text);
}

void EditorWindow::showNotYet()
{
    QMessageBox::information(this, QStringLiteral("Файлы схем"),
        QStringLiteral("Открытие и сохранение схем появится вместе с ядром симулятора "
                       "(формат файла описывает ядро)."));
}
