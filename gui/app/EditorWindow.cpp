#include "app/EditorWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
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
#include "theme/Theme.h"
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

// Значок «все элементы»: четыре квадратика.
QIcon gridIcon(const QColor &color)
{
    QIcon icon;
    for (int size : { 24, 32, 48 }) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const qreal margin = size * 0.22;
        const qreal gap = size * 0.12;
        const qreal cell = (size - 2 * margin - gap) / 2.0;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col)
                p.drawRoundedRect(QRectF(margin + col * (cell + gap), margin + row * (cell + gap), cell, cell),
                                  cell * 0.22, cell * 0.22);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

bool tabNameTaken(const QTabWidget *tabs, const QString &name, int except = -1)
{
    for (int i = 0; i < tabs->count(); ++i)
        if (i != except && tabs->tabText(i) == name)
            return true;
    return false;
}

QVector<CircuitView *> allViews(const QTabWidget *tabs)
{
    QVector<CircuitView *> out;
    for (int i = 0; i < tabs->count(); ++i)
        if (auto *v = qobject_cast<CircuitView *>(tabs->widget(i)))
            out.append(v);
    return out;
}

constexpr qreal kBaseFlowSpeed = 40.0;      // пикселей/с при множителе 1×

} // namespace

// ================================================================ создание

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Widget);       // вложенное окно внутри MainWindow

    m_undoGroup = new QUndoGroup(this);

    m_tabs = new EditorTabs(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(false);          // свои кнопки закрытия — см. newTab()
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);
    setCentralWidget(m_tabs);

    buildActions();
    buildMenus();
    buildToolBars();
    buildDock();
    buildStatusBar();

    connect(m_tabs, &QTabWidget::currentChanged, this, &EditorWindow::onTabChanged);
    m_tabs->bar()->installEventFilter(this);

    connect(&Theme::instance(), &Theme::changed, this, &EditorWindow::refreshIcons);

    newTab();
}

EditorWindow::~EditorWindow()
{
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

    m_openAction = new QAction(QStringLiteral("Открыть…"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, [this] { showNotYet(); });

    m_saveAction = new QAction(QStringLiteral("Сохранить"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, [this] { showNotYet(); });

    m_closeTabAction = new QAction(QStringLiteral("Закрыть вкладку"), this);
    m_closeTabAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    connect(m_closeTabAction, &QAction::triggered, this, [this] { closeTab(m_tabs->currentIndex()); });

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
    if (m_allButton)
        m_allButton->setIcon(gridIcon(color));
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
    file->addAction(m_openAction);
    file->addAction(m_saveAction);
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
    // Только левая панель инструментов: остальные действия (файл, правка, вид) —
    // в меню сверху, чтобы не дублировать их кнопками.
    // --- левая: выбор и самые частые элементы -----------------------------
    auto *tools = new QToolBar(QStringLiteral("Элементы"), this);
    tools->setObjectName(QStringLiteral("toolsBar"));
    tools->setOrientation(Qt::Vertical);
    tools->setMovable(false);
    tools->setFloatable(false);
    tools->setIconSize(QSize(28, 28));
    tools->setToolButtonStyle(Qt::ToolButtonIconOnly);

    tools->addAction(m_selectAction);
    tools->addSeparator();
    for (ElementType t : { ElementType::Wire, ElementType::Resistor, ElementType::Capacitor,
                           ElementType::Inductor, ElementType::VoltageSourceDC, ElementType::Ground }) {
        if (QAction *a = m_elementActions.value(int(t), nullptr))
            tools->addAction(a);
    }
    tools->addSeparator();

    // «Все элементы» — выпадающий список по категориям (тот же, что по ПКМ).
    auto *popup = new QMenu(this);
    popup->addAction(m_selectAction);
    popup->addSeparator();
    if (m_routedWireAction)
        popup->addAction(m_routedWireAction);
    fillElementMenu(popup);

    m_allButton = new QToolButton(tools);
    m_allButton->setObjectName(QStringLiteral("allElementsButton"));
    m_allButton->setToolTip(QStringLiteral("Все элементы"));
    m_allButton->setPopupMode(QToolButton::InstantPopup);
    m_allButton->setMenu(popup);
    m_allButton->setAutoRaise(true);
    m_allButton->setIconSize(QSize(28, 28));
    m_allButton->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; }"));
    tools->addWidget(m_allButton);
    addToolBar(Qt::LeftToolBarArea, tools);
    refreshIcons();
}

void EditorWindow::buildDock()
{
    m_sim = new SimulationPanel(this);
    m_panel = new PropertiesPanel(this);

    // Правая панель как у Falstad: сверху симуляция, под ней свойства выбранного элемента.
    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("dockRoot"));
    root->setAttribute(Qt::WA_StyledBackground, true);
    root->setMinimumWidth(280);
    auto *lay = new QVBoxLayout(root);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_sim);
    auto *sep = new QFrame(root);
    sep->setObjectName(QStringLiteral("separator"));
    sep->setFrameShape(QFrame::HLine);
    lay->addWidget(sep);
    lay->addWidget(m_panel, 1);

    m_dock = new QDockWidget(QStringLiteral("Панель"), this);
    m_dock->setObjectName(QStringLiteral("sideDock"));
    m_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_dock->setTitleBarWidget(new QWidget(m_dock));        // без заголовка — минимализм
    m_dock->setWidget(root);
    addDockWidget(Qt::RightDockWidgetArea, m_dock);
    resizeDocks({ m_dock }, { 300 }, Qt::Horizontal);

    QAction *toggle = m_dock->toggleViewAction();
    toggle->setText(QStringLiteral("Правая панель"));
    m_viewMenu->addAction(toggle);

    // двойной клик / кнопка «Все характеристики…» -> окно свойств
    connect(m_panel, &PropertiesPanel::detailsRequested, this, [this](CircuitItem *item) {
        QTimer::singleShot(0, this, [this, item] { openElementDialog(item); });
    });

    // --- симуляция --------------------------------------------------------
    CircuitItem::setFlowSpeed(kBaseFlowSpeed * m_sim->currentSpeed());
    connect(m_sim, &SimulationPanel::currentSpeedChanged, this, [](double factor) {
        CircuitItem::setFlowSpeed(kBaseFlowSpeed * factor);
    });
    connect(m_sim, &SimulationPanel::runningChanged, this, [this](bool on) {
        for (CircuitView *v : allViews(m_tabs))
            v->setFlowAnimation(on);
    });
    connect(m_sim, &SimulationPanel::resetClicked, this, [this] {
        for (CircuitView *v : allViews(m_tabs))
            v->resetSimulationState();
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

QString EditorWindow::nextTabName()
{
    for (;;) {
        const QString name = QStringLiteral("Схема %1").arg(++m_tabCounter);
        if (!tabNameTaken(m_tabs, name))
            return name;
    }
}

QString EditorWindow::uniqueTabName(const QString &base)
{
    if (!tabNameTaken(m_tabs, base))
        return base;
    for (int n = 2;; ++n) {
        const QString name = QStringLiteral("%1 %2").arg(base).arg(n);
        if (!tabNameTaken(m_tabs, name))
            return name;
    }
}

void EditorWindow::newTab()
{
    auto *view = new CircuitView;
    view->setFlowAnimation(m_sim && m_sim->isRunning());
    m_undoGroup->addStack(view->undoStack());
    const int index = m_tabs->addTab(view, nextTabName());

    // Плоская кнопка «×» вместо стандартной иконки стиля. Закрытие — отложенно:
    // окно подтверждения не должно жить внутри обработчика клика по вкладке.
    auto *close = new QToolButton(m_tabs->bar());
    close->setObjectName(QStringLiteral("tabClose"));
    close->setText(QStringLiteral("×"));
    close->setToolTip(QStringLiteral("Закрыть вкладку"));
    close->setAutoRaise(true);
    close->setCursor(Qt::ArrowCursor);
    m_tabs->bar()->setTabButton(index, QTabBar::RightSide, close);
    QPointer<QWidget> page = view;
    connect(close, &QToolButton::clicked, this, [this, page] {
        QTimer::singleShot(0, this, [this, page] {
            if (page)
                closeTab(m_tabs->indexOf(page));
        });
    });

    m_tabs->setCurrentIndex(index);
    view->setFocus();
}

void EditorWindow::onTabChanged(int index)
{
    attachView(qobject_cast<CircuitView *>(m_tabs->widget(index)));
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
    m_viewConnections << connect(view->undoStack(), &QUndoStack::indexChanged,
                                 this, [this] { updateCount(); });

    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(view->zoom() * 100)));
    syncToolActions();
    syncEditActions();
    updateCount();
}

void EditorWindow::removeTab(int index)
{
    auto *view = qobject_cast<CircuitView *>(m_tabs->widget(index));
    if (!view)
        return;

    if (view == m_view)
        attachView(nullptr);
    m_undoGroup->removeStack(view->undoStack());
    m_tabs->removeTab(index);       // currentChanged подключит соседнюю вкладку
    view->deleteLater();
}

void EditorWindow::closeTab(int index)
{
    auto *view = qobject_cast<CircuitView *>(m_tabs->widget(index));
    if (!view)
        return;

    if (view->undoStack()->count() > 0) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Закрыть вкладку"),
            QStringLiteral("Вкладка «%1» содержит изменения, а сохранение схем появится вместе с ядром.\n"
                           "Закрыть без сохранения?").arg(m_tabs->tabText(index)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    removeTab(index);
    if (m_tabs->count() == 0)
        emit backToMainMenu();          // закрыли последнюю — возвращаемся в главное меню
}

void EditorWindow::closeTabsWhere(const QVector<CircuitView *> &views)
{
    if (views.isEmpty())
        return;

    int dirty = 0;
    for (CircuitView *v : views)
        if (v->undoStack()->count() > 0)
            ++dirty;
    if (dirty > 0) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Закрыть вкладки"),
            QStringLiteral("В закрываемых вкладках есть изменения (%1), а сохранение схем появится "
                           "вместе с ядром.\nЗакрыть без сохранения?").arg(dirty),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    for (CircuitView *v : views) {
        const int i = m_tabs->indexOf(v);
        if (i >= 0)
            removeTab(i);
    }
    if (m_tabs->count() == 0)
        emit backToMainMenu();
}

void EditorWindow::renameTab(int index)
{
    if (index < 0 || index >= m_tabs->count())
        return;
    bool ok = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("Переименовать вкладку"),
                                               QStringLiteral("Название:"), QLineEdit::Normal,
                                               m_tabs->tabText(index), &ok).trimmed();
    if (!ok || text.isEmpty())
        return;
    if (tabNameTaken(m_tabs, text, index)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Вкладка с таким именем уже существует."));
        return;
    }
    m_tabs->setTabText(index, text);
}

void EditorWindow::duplicateTab(int index)
{
    auto *src = qobject_cast<CircuitView *>(m_tabs->widget(index));
    if (!src)
        return;

    const QVector<ItemData> data = src->snapshot(false);
    const QString name = uniqueTabName(m_tabs->tabText(index) + QStringLiteral(" (копия)"));

    newTab();                                   // новая вкладка становится текущей
    const int last = m_tabs->count() - 1;
    m_tabs->setTabText(last, name);
    m_tabs->bar()->moveTab(last, index + 1);    // рядом с исходной

    if (CircuitView *dst = currentView()) {
        dst->loadSnapshot(data, QStringLiteral("Копия схемы"));
        dst->fitToContent();
    }
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
        QVector<CircuitView *> list;
        for (int i = 0; i < m_tabs->count(); ++i)
            if (i != idx)
                if (auto *v = qobject_cast<CircuitView *>(m_tabs->widget(i)))
                    list.append(v);
        closeTabsWhere(list);
    } else if (chosen == aRight) {
        QVector<CircuitView *> list;
        for (int i = idx + 1; i < m_tabs->count(); ++i)
            if (auto *v = qobject_cast<CircuitView *>(m_tabs->widget(i)))
                list.append(v);
        closeTabsWhere(list);
    } else if (chosen == aAll) {
        closeTabsWhere(allViews(m_tabs));
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
        const QColor dim = Theme::instance().palette().textDim;
        const QList<CircuitItem *> sel = v->selectedCircuitItems();
        const int count = std::max<int>(1, int(sel.size()));

        QMenu menu(this);
        menu.setMinimumWidth(250);

        auto *header = new QWidgetAction(&menu);
        auto *box = new QWidget(&menu);
        auto *row = new QHBoxLayout(box);
        row->setContentsMargins(10, 8, 12, 8);
        row->setSpacing(10);
        auto *pic = new QLabel(box);
        pic->setFixedSize(40, 40);
        pic->setAlignment(Qt::AlignCenter);
        if (count == 1)
            pic->setPixmap(SymbolPainter::icon(target->elementType(), 36, col));
        else
            pic->setText(QString::number(count));
        auto *col2 = new QVBoxLayout;
        col2->setSpacing(1);
        auto *title = new QLabel(count == 1 ? target->displayName()
                                            : QStringLiteral("Выбрано элементов: %1").arg(count), box);
        QFont tf = title->font();
        tf.setBold(true);
        tf.setPointSizeF(tf.pointSizeF() + 1.0);
        title->setFont(tf);
        QString subText;
        if (count == 1) {
            const ElementInfo &info = elementInfo(target->elementType());
            subText = QString::fromUtf8(categoryName(info.category));
            if (target->pinCount() > 0)
                subText += QStringLiteral(" · выводов: %1").arg(target->pinCount());
        } else {
            subText = QStringLiteral("действия применятся ко всем");
        }
        auto *sub = new QLabel(subText, box);
        sub->setStyleSheet(QStringLiteral("color: %1;").arg(dim.name()));
        col2->addWidget(title);
        col2->addWidget(sub);
        row->addWidget(pic);
        row->addLayout(col2, 1);
        header->setDefaultWidget(box);
        menu.addAction(header);
        menu.addSeparator();

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

void EditorWindow::showNotYet()
{
    QMessageBox::information(this, QStringLiteral("Файлы схем"),
        QStringLiteral("Открытие и сохранение схем появится вместе с ядром симулятора "
                       "(формат файла описывает ядро)."));
}
