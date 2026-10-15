#include "InfiniteGraphicsView.h"
#include "InfiniteGraphicsScene.h"
#include "EditorWindow.h"
#include <QLabel>
#include <QAction>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QApplication>
#include <QContextMenuEvent>
#include <QTabWidget>
#include <QInputDialog>
#include <QMenu>
#include <QTabBar>
#include <QMouseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QWheelEvent>
#include <QScrollBar>
#include <QTimer>
#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QGridLayout>   // для сетки палитры 8×8 в createSettingsTab
#include <cmath>
#include <functional>
#include <QLineF>

namespace {
// Масштаб вида по умолчанию (используется и при создании вкладки, и при
// "Сбросить вид", и при "По размеру", когда сцена пуста) — раньше это
// число 2.0 было продублировано в трёх местах по отдельности.
constexpr qreal kInitialViewScale = 2.0;
constexpr qreal kZoomStep = 1.25;

// Метка на служебных объектах сцены (оси, деления, подписи "0,0"), которые
// createNewTab() рисует для ориентации. onFitToSize() должен их игнорировать
// при расчёте bounding rect пользовательских элементов — раньше это делалось
// хрупкой эвристикой "длина линии > 300", которая ошибочно исключала бы и
// длинные провода, если бы пользователь такой нарисовал.
constexpr int kAuxItemRole = Qt::UserRole + 1;

void markAsAuxItem(QGraphicsItem *item) {
    if (item) item->setData(kAuxItemRole, true);
}
} // namespace

QString buildAppStyleSheet(const QColor &bg)
{
    const bool dark = bg.lightness() < 128;
    const QString bgS = bg.name();
    const QString txt = dark ? "#f1f1f1" : "#1e1e1e";

    // Аддитивная Shade-функция: прибавляем яркость к каждой компоненте.
    // В отличие от QColor::lighter(), работает даже на чисто чёрном фоне
    // (lighter() умножает компоненты: 0 * 1.5 = 0 — чёрный так и остаётся чёрным).
    auto shade = [&](int d) -> QString {
        const int s = dark ? d : -d;   // в светлой теме — в обратную сторону
        return QColor(qBound(0, bg.red()   + s, 255),
                      qBound(0, bg.green() + s, 255),
                      qBound(0, bg.blue()  + s, 255)).name();
    };

    const QString panel = shade(10);   // фон меню / выпадающих списков
    const QString ctrl  = shade(18);   // фон полей, кнопок, комбобоксов
    const QString brd   = shade(55);   // РАМКИ: серые, видны даже на чёрном
    const QString hov   = shade(32);   // ховер
    const QString deep  = shade(-10);  // невыбранные вкладки (на чёрном → чёрный)
    const QString acc   = "#0078d7";

    return QString(
               "QWidget { background-color:%1; color:%2; }"
               "QDialog { background:transparent; }"
               "QMenuBar { background:%1; }"
               "QMenuBar::item { background:transparent; }"
               "QMenuBar::item:selected { background:%5; }"
               "QMenu { background:%3; color:%2; border:1px solid %4; }"
               "QMenu::item { padding:5px 24px; }"
               "QMenu::item:selected { background:%5; }"
               "QMenu::separator { background:%4; height:1px; }"
               "QTabWidget::pane { border:none; background:%1; }"
               "QTabBar::tab { background:%6; color:%2; padding:6px 14px; border:1px solid %4; border-bottom:none; }"
               "QTabBar::tab:selected { background:%1; }"
               "QComboBox { background:%7; color:%2; border:1px solid %4; border-radius:3px; padding:4px 8px; min-height:22px; }"
               "QComboBox:hover { border-color:%5; }"
               "QComboBox::drop-down { border:none; width:22px; }"
               "QComboBox::down-arrow { border-left:4px solid transparent; border-right:4px solid transparent; border-top:6px solid %2; margin-right:8px; }"
               "QComboBox QAbstractItemView { background:%3; color:%2; border:1px solid %4; selection-background-color:%5; }"
               "QLabel, QCheckBox { background:transparent; }"
               "QCheckBox::indicator { width:16px; height:16px; border:1px solid %4; border-radius:3px; background:%7; }"
               "QCheckBox::indicator:checked { background:%8; border-color:%8; }"
               "QPushButton { background:%7; color:%2; border:1px solid %4; border-radius:3px; padding:5px 16px; }"
               "QPushButton:hover { background:%5; }"
               "QLineEdit, QSpinBox, QDoubleSpinBox { background:%7; color:%2; border:1px solid %4; border-radius:3px; padding:4px 6px; }"
               "QScrollBar:vertical { background:%1; width:12px; }"
               "QScrollBar::handle:vertical { background:%4; border-radius:6px; min-height:30px; }"
               "QScrollBar:horizontal { background:%1; height:12px; }"
               "QScrollBar::handle:horizontal { background:%4; border-radius:6px; min-width:30px; }"
               "QScrollBar::add-line, QScrollBar::sub-line { height:0; width:0; }"
               ).arg(bgS, txt, panel, brd, hov, deep, ctrl, acc);
}

EditorWindow::EditorWindow(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void EditorWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---------- Строка меню ----------
    QMenuBar *menuBar = new QMenuBar(this);

    // ----- Меню "Файл" -----
    QMenu *fileMenu = menuBar->addMenu("Файл");

    QAction *newAction = fileMenu->addAction("Новый проект");
    connect(newAction, &QAction::triggered, this, &EditorWindow::onNewProject);

    QAction *openAction = fileMenu->addAction("Открыть...");
    connect(openAction, &QAction::triggered, this, &EditorWindow::onOpenProject);

    // TODO: у проекта пока нет формата сериализации схемы — эти два пункта
    // намеренно ничего не делают, пока не появится save/load (см. onOpenProject).
    fileMenu->addAction("Сохранить")->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction("Экспортировать как...")->setEnabled(false);

    QAction *exitAction = fileMenu->addAction("Выйти");
    connect(exitAction, &QAction::triggered, this, &EditorWindow::backToMainMenu);

    // ---------- Общие действия редактирования ----------
    // Создаются ОДИН раз и переиспользуются в трёх местах ниже: меню
    // "Правка", меню "Edit" и временном контекстном меню по ПКМ
    // (см. contextMenuEvent). Один и тот же QAction можно добавлять сразу
    // в несколько QMenu — Qt сам следит за синхронизацией checked/enabled
    // и текста, поэтому не нужно строить три параллельных набора пунктов
    // с одинаковой логикой и вручную держать их в согласованном состоянии.
    createEditActions();

    // ----- Меню "Правка" (быстрый доступ: undo/redo + буфер обмена) -----
    QMenu *quickEditMenu = menuBar->addMenu("Правка");
    quickEditMenu->addAction(undoAction);
    quickEditMenu->addAction(redoAction);
    quickEditMenu->addSeparator();
    quickEditMenu->addAction(m_copyAction);
    quickEditMenu->addAction(m_cutAction);
    quickEditMenu->addAction(m_pasteAction);
    connect(quickEditMenu, &QMenu::aboutToShow, this, &EditorWindow::updateEditActionsState);

    // ----- Меню "Настройки" -----
    QMenu *settingsMenu = menuBar->addMenu("Настройки");
    QAction *paramsAction = settingsMenu->addAction("Параметры (скоро)");
    paramsAction->setEnabled(false);   // заглушка — позже сюда добавишь свои параметры
    settingsMenu->addSeparator();
    QAction *advSettingsAction = settingsMenu->addAction("Доп. настройки");
    connect(advSettingsAction, &QAction::triggered, this, &EditorWindow::createSettingsTab);

    // ----- Меню "Вид" -----
    QMenu *viewMenu = menuBar->addMenu("Вид");

    QAction *zoomInAction = viewMenu->addAction("Увеличить");
    connect(zoomInAction, &QAction::triggered, this, &EditorWindow::onZoomIn);

    QAction *zoomOutAction = viewMenu->addAction("Уменьшить");
    connect(zoomOutAction, &QAction::triggered, this, &EditorWindow::onZoomOut);

    QAction *fitToSizeAction = viewMenu->addAction("По размеру");
    connect(fitToSizeAction, &QAction::triggered, this, &EditorWindow::onFitToSize);

    viewMenu->addSeparator();

    QAction *resetViewAction = viewMenu->addAction("Сбросить вид");
    connect(resetViewAction, &QAction::triggered, this, &EditorWindow::onResetView);

    // ----- Меню "Edit" (полный набор: типы элементов + правка) -----
    QMenu *editMenu = menuBar->addMenu("Edit");
    populateEditMenu(editMenu);
    connect(editMenu, &QMenu::aboutToShow, this, &EditorWindow::updateEditActionsState);

    mainLayout->addWidget(menuBar);

    // ---------- Область вкладок ----------
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->setDocumentMode(true);
    tabWidget->setMovable(true);
    tabWidget->tabBar()->installEventFilter(this);

    mainLayout->addWidget(tabWidget, 1);

    createNewTab(generateUniqueTabName("Проект"));
    applyTheme(m_backgroundColor);

    connect(tabWidget, &QTabWidget::tabCloseRequested,
            this, &EditorWindow::closeTab);
}

void EditorWindow::contextMenuEvent(QContextMenuEvent *event) {
    if (!getCurrentView()) return;

    updateEditActionsState();   // актуализировать checked/enabled перед показом
    QMenu menu(this);
    populateEditMenu(&menu);
    menu.exec(event->globalPos());
}

// =============================================================
// Общие действия редактирования — см. комментарий в setupUI() и в .h
// =============================================================
void EditorWindow::createEditActions() {
    undoAction = new QAction("Отменить", this);
    undoAction->setShortcut(QKeySequence::Undo);   // Ctrl+Z на всех платформах
    undoAction->setEnabled(false);
    connect(undoAction, &QAction::triggered, this, &EditorWindow::onUndo);

    redoAction = new QAction("Повторить", this);
    redoAction->setShortcut(QKeySequence::Redo);   // Ctrl+Y на Windows
    redoAction->setEnabled(false);
    connect(redoAction, &QAction::triggered, this, &EditorWindow::onRedo);

    m_wireAction = new QAction("Wire\tw", this);
    m_wireAction->setCheckable(true);
    connect(m_wireAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->setWireMode(!v->isWireMode());
    });

    m_resistorAction = new QAction("Resistor\tr", this);
    m_resistorAction->setCheckable(true);
    connect(m_resistorAction, &QAction::triggered, this, [this]() {
        auto *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Resistor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Resistor);
    });

    m_capAction = new QAction("Non-polar capacitor\tc", this);
    m_capAction->setCheckable(true);
    connect(m_capAction, &QAction::triggered, this, [this]() {
        auto *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Non_Polar_Capacitor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Non_Polar_Capacitor);
    });

    m_polarCapAction = new QAction("Polar capacitor\te", this);  // шорткат 'e' от Electrolytic
    m_polarCapAction->setCheckable(true);
    connect(m_polarCapAction, &QAction::triggered, this, [this]() {
        auto *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Polar_Capacitor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Polar_Capacitor);
    });


    m_selectAction = new QAction("Выделить", this);
    m_selectAction->setCheckable(true);
    connect(m_selectAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) {
            v->setWireMode(false);
            v->disableComponentMode();
        }
    });

    m_deleteAction = new QAction("Удалить\t Del", this);
    connect(m_deleteAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->deleteSelectedItems();
    });

    m_copyAction = new QAction("Копировать\tCtrl+C", this);
    connect(m_copyAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->copySelected();
    });

    m_cutAction = new QAction("Вырезать\tCtrl+X", this);
    connect(m_cutAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->cutSelected();
    });

    m_pasteAction = new QAction("Вставить\tCtrl+V", this);
    connect(m_pasteAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->pasteClipboard();
    });
}

// Наполняет любое переданное меню полным набором действий редактирования
// схемы. Используется и для постоянного меню "Edit" (создаётся один раз),
// и для временного контекстного меню по ПКМ (создаётся заново при каждом
// вызове contextMenuEvent — сами QAction при этом не пересоздаются,
// пересоздаются только пункты-заглушки без логики, вроде "Passive elements").
void EditorWindow::populateEditMenu(QMenu *menu) {
    // --- Верхний уровень: базовые инструменты ---
    menu->addAction(m_wireAction);
    menu->addAction(m_resistorAction);
    menu->addAction(m_capAction);          // неполярный конденсатор — остаётся здесь
    menu->addSeparator();

    // --- Пассивные элементы ---
    QMenu *passive = menu->addMenu("Passive elements");
    passive->addAction(m_polarCapAction);             // конденсатор (полярный) — уже реализован
    passive->addSeparator();
    passive->addAction("Inductor coil");              // катушка индуктивности (L)
    passive->addAction("Switch");                     // выключатель (s)
    passive->addAction("Push button");                // кнопка
    passive->addAction("SPDT switch");                // SPDT переключатель (S)
    passive->addAction("DPDT switch");                // DPDT переключатель
    passive->addAction("Make-before-break switch");   // переключатель с перекрывающими контактами
    passive->addAction("Potentiometer");              // потенциометр
    passive->addAction("Transformer");                // трансформатор (T)
    passive->addAction("Tapped transformer");         // трансформатор с отводом
    passive->addAction("Custom transformer");         // пользовательский трансформатор
    passive->addAction("Transmission line");          // линия электропередачи
    passive->addAction("Relay");                      // реле (R)
    passive->addAction("Relay coil");                 // катушка реле
    passive->addAction("Relay contact");              // контакт реле
    passive->addAction("Photoresistor");              // фоторезистор
    passive->addAction("Thermistor");                 // термистор
    passive->addAction("Memristor");                  // мемристор
    passive->addAction("Spark gap");                  // искровой промежуток
    passive->addAction("Fuse");                       // плавкий предохранитель
    passive->addAction("Quartz resonator");           // кварцевый резонатор
    passive->addAction("Cross switch");               // перекрёстный переключатель
    passive->addAction("Gyrator");                    // гиратор

    // --- Остальные секции без изменений ---
    QMenu *active = menu->addMenu("Active elements");
    active->addAction("Diode");
    QMenu *activeBlocks = menu->addMenu("Active blocks");
    activeBlocks->addAction("Operational amplifier ( - top)");
    menu->addMenu("Logic elements, inputs and outputs");
    menu->addMenu("Digital elements");
    menu->addMenu("Analog and hybrid elements");
    menu->addSeparator();
    menu->addAction(undoAction);
    menu->addAction(redoAction);
    menu->addSeparator();
    menu->addAction(m_copyAction);
    menu->addAction(m_cutAction);
    menu->addAction(m_pasteAction);
    menu->addSeparator();
    menu->addAction(m_selectAction);
    menu->addAction(m_deleteAction);
}

// Обновляет checked/enabled/текст для ВСЕХ общих действий редактирования
// разом — вызывается перед показом любого из трёх мест, где они видны
// (Правка/Edit через aboutToShow, контекстное меню — явно в contextMenuEvent),
// а также после undo()/redo(), чтобы шорткаты Ctrl+Z/Ctrl+Y сразу обновляли
// состояние, даже если ни одно меню не открывалось.
void EditorWindow::updateEditActionsState() {
    InfiniteGraphicsView *view = getCurrentView();

    const bool canUndo = view && view->canUndo();
    const bool canRedo = view && view->canRedo();
    undoAction->setEnabled(canUndo);
    redoAction->setEnabled(canRedo);
    undoAction->setText(canUndo ? "Отменить: " + view->undoText() : "Отменить");
    redoAction->setText(canRedo ? "Повторить: " + view->redoText() : "Повторить");

    const bool wireMode = view && view->isWireMode();
    const bool isResistorMode = view && view->isComponentMode() &&
                                view->currentComponentType() == ComponentType::Resistor;
    const bool isCapMode = view && view->isComponentMode() &&
                           view->currentComponentType() == ComponentType::Non_Polar_Capacitor;

    m_wireAction->setChecked(wireMode);
    m_resistorAction->setChecked(isResistorMode);
    m_capAction->setChecked(isCapMode);
    m_selectAction->setChecked(view && !wireMode && !view->isComponentMode());
    m_deleteAction->setEnabled(view && view->scene() && !view->scene()->selectedItems().isEmpty());

    const bool isPolarCapMode = view && view->isComponentMode() &&
                                view->currentComponentType() == ComponentType::Polar_Capacitor;

    // ... в конце метода:
    m_capAction->setChecked(isCapMode);
    m_polarCapAction->setChecked(isPolarCapMode);  // <-- ДОБАВИТЬ
}

// =============================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// =============================================================

InfiniteGraphicsView* EditorWindow::getCurrentView() const {
    if (!tabWidget) return nullptr;
    QWidget *currentWidget = tabWidget->currentWidget();
    if (!currentWidget) return nullptr;
    const QList<QGraphicsView*> views = currentWidget->findChildren<QGraphicsView*>();
    if (views.isEmpty()) return nullptr;
    return qobject_cast<InfiniteGraphicsView*>(views.first());
}

// =============================================================
// СЛОТЫ ДЛЯ ЗУМА И СБРОСА
// =============================================================

void EditorWindow::onZoomIn() {
    if (InfiniteGraphicsView *view = getCurrentView())
        view->scale(kZoomStep, kZoomStep);
}

void EditorWindow::onZoomOut() {
    if (InfiniteGraphicsView *view = getCurrentView())
        view->scale(1.0 / kZoomStep, 1.0 / kZoomStep);
}

void EditorWindow::onFitToSize() {
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) return;
    QGraphicsScene *scene = view->scene();
    if (!scene) return;

    // Объединяем bounding rect всех НЕ служебных объектов сцены (провода,
    // компоненты) — служебные (оси, деления, подписи) помечены в
    // createNewTab() через markAsAuxItem() и пропускаются по метке, а не
    // по эвристике вроде "длина линии больше 300".
    QRectF userBounds;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(kAuxItemRole).toBool()) continue;
        const QRectF r = item->sceneBoundingRect();
        userBounds = userBounds.isNull() ? r : userBounds.united(r);
    }

    if (userBounds.isNull()) {
        // На сцене нет пользовательских объектов — сбрасываем в центр.
        view->resetTransform();
        view->scale(kInitialViewScale, kInitialViewScale);
        view->centerOn(0, 0);
    } else {
        // TODO: сейчас только центрируем; точную подгонку масштаба под
        // bounding rect (fitInView с учётом отступов) можно добавить позже.
        view->centerOn(userBounds.center());
    }
}

void EditorWindow::onResetView() {
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) return;

    view->resetTransform();
    view->scale(kInitialViewScale, kInitialViewScale);
    view->centerOn(0, 0);
}

// =============================================================
// СОЗДАНИЕ НОВОЙ ВКЛАДКИ
// =============================================================

void EditorWindow::createNewTab(const QString &title) {
    QWidget *tabContent = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tabContent);
    layout->setContentsMargins(0, 0, 0, 0);

    InfiniteGraphicsView *view = new InfiniteGraphicsView;
    view->setPreferredCursor(m_cursorShape);
    InfiniteGraphicsScene *scene = new InfiniteGraphicsScene(view);
    scene->setBackgroundColor(m_backgroundColor);   // фон рисует сцена (drawBackground), не вьюха
    scene->setSceneRect(-10000, -10000, 20000, 20000);

    // ====== РИСУЕМ ОСИ И МЕТКИ (служебные объекты — см. markAsAuxItem) ======
    QPen axisPen(Qt::white, 1.5);
    QPen tickPen(Qt::gray, 1);

    markAsAuxItem(scene->addLine(-200, 0, 200, 0, axisPen));
    markAsAuxItem(scene->addLine(0, -200, 0, 200, axisPen));

    for (int x = -200; x <= 200; x += 50) {
        if (x == 0) continue;
        markAsAuxItem(scene->addLine(x, -5, x, 5, tickPen));
        QGraphicsTextItem *text = scene->addText(QString::number(x));
        text->setPos(x - 10, 5);
        text->setDefaultTextColor(Qt::lightGray);
        text->setScale(0.5);
        markAsAuxItem(text);
    }

    for (int y = -200; y <= 200; y += 50) {
        if (y == 0) continue;
        markAsAuxItem(scene->addLine(-5, y, 5, y, tickPen));
        QGraphicsTextItem *text = scene->addText(QString::number(y));
        text->setPos(-25, y - 8);
        text->setDefaultTextColor(Qt::lightGray);
        text->setScale(0.5);
        markAsAuxItem(text);
    }

    QGraphicsTextItem *labelX = scene->addText("X");
    labelX->setPos(205, -10);
    labelX->setDefaultTextColor(Qt::white);
    labelX->setScale(1.2);
    markAsAuxItem(labelX);

    QGraphicsTextItem *labelY = scene->addText("Y");
    labelY->setPos(-25, -215);
    labelY->setDefaultTextColor(Qt::white);
    labelY->setScale(1.2);
    markAsAuxItem(labelY);

    markAsAuxItem(scene->addEllipse(-10, -10, 20, 20, QPen(Qt::red, 2), QBrush(Qt::yellow)));

    QGraphicsTextItem *centerLabel = scene->addText("0,0");
    centerLabel->setPos(15, -10);
    centerLabel->setDefaultTextColor(Qt::green);
    centerLabel->setScale(0.8);
    markAsAuxItem(centerLabel);
    // =================================================

    view->setScene(scene);
    view->scale(kInitialViewScale, kInitialViewScale);
    view->centerOn(0, 0);

    layout->addWidget(view);

    int index = tabWidget->addTab(tabContent, title);
    tabWidget->setCurrentIndex(index);

    connect(view->undoStack(), &QUndoStack::canUndoChanged, this, &EditorWindow::updateEditActionsState);
    connect(view->undoStack(), &QUndoStack::canRedoChanged, this, &EditorWindow::updateEditActionsState);
}

// =============================================================
// ОСТАЛЬНЫЕ СЛОТЫ И ФУНКЦИИ
// =============================================================

void EditorWindow::onNewProject() {
    createNewTab(generateUniqueTabName("Проект"));
}

void EditorWindow::closeTab(int index) {
    QWidget *widget = tabWidget->widget(index);
    if (!widget) return;
    tabWidget->removeTab(index);
    delete widget;
}

bool EditorWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj != tabWidget->tabBar()) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        const int index = tabWidget->tabBar()->tabAt(mouseEvent->pos());
        if (index == -1) return QWidget::eventFilter(obj, event);

        // Вкладку настроек переименовывать нельзя.
        QWidget *w = tabWidget->widget(index);
        if (w && w->property("isSettings").toBool()) return true;   // глотаем событие

        QTimer::singleShot(0, this, [this, index]() { renameTab(index); });
        return true;
    }

    if (event->type() == QEvent::ContextMenu) {
        auto *contextEvent = static_cast<QContextMenuEvent*>(event);
        const int index = tabWidget->tabBar()->tabAt(contextEvent->pos());
        if (index == -1) return QWidget::eventFilter(obj, event);

        const QPoint globalPos = contextEvent->globalPos();
        // Откладываем показ меню тем же способом, что и переименование выше —
        // чтобы не показывать меню внутри обработки события таббара.
        QTimer::singleShot(0, this, [this, index, globalPos]() {
            showTabContextMenu(index, globalPos);
        });
        return true;
    }

    return QWidget::eventFilter(obj, event);
}

void EditorWindow::renameTab(int index) {
    const QString currentTitle = tabWidget->tabText(index);
    bool ok = false;
    const QString newTitle = QInputDialog::getText(this,
                                                   "Переименование",
                                                   "Введите новое имя вкладки:",
                                                   QLineEdit::Normal,
                                                   currentTitle,
                                                   &ok);
    if (!ok || newTitle.isEmpty()) return;

    for (int i = 0; i < tabWidget->count(); ++i) {
        if (i != index && tabWidget->tabText(i) == newTitle) {
            QMessageBox::warning(this, "Ошибка", "Вкладка с таким именем уже существует.");
            return;
        }
    }
    tabWidget->setTabText(index, newTitle);
}

void EditorWindow::showTabContextMenu(int index, const QPoint &globalPos) {
    QMenu menu(this);
    QAction *newTabAction = menu.addAction("Create");
    connect(newTabAction, &QAction::triggered, this, &EditorWindow::onNewProject);

    const bool isSettings = tabWidget->widget(index) &&
                            tabWidget->widget(index)->property("isSettings").toBool();

    QAction *renameAction = menu.addAction("Переименовать");
    renameAction->setEnabled(!isSettings);   // для вкладки настроек недоступно
    connect(renameAction, &QAction::triggered, this, [this, index]() { renameTab(index); });

    menu.addSeparator();

    QAction *closeAction = menu.addAction("Закрыть");
    connect(closeAction, &QAction::triggered, this, [this, index]() { closeTab(index); });

    QAction *closeAllAction = menu.addAction("Закрыть все");
    connect(closeAllAction, &QAction::triggered, this, [this]() {
        while (tabWidget->count() > 0) closeTab(0);
    });

    QAction *closeOthersAction = menu.addAction("Закрыть другие");
    connect(closeOthersAction, &QAction::triggered, this, [this, index]() {
        for (int i = tabWidget->count() - 1; i >= 0; --i)
            if (i != index) closeTab(i);
    });

    menu.exec(globalPos);
}

QString EditorWindow::generateUniqueTabName(const QString &baseName) {
    QStringList existingNames;
    for (int i = 0; i < tabWidget->count(); ++i)
        existingNames << tabWidget->tabText(i);

    if (!existingNames.contains(baseName)) return baseName;

    int suffix = 2;
    QString candidate;
    do {
        candidate = baseName + " (" + QString::number(suffix) + ")";
        ++suffix;
    } while (existingNames.contains(candidate));
    return candidate;
}

void EditorWindow::onOpenProject() {
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Открыть проект",
                                                          QDir::homePath(),
                                                          "Файлы проектов (*.prj);;Все файлы (*)");
    if (fileName.isEmpty()) return;

    for (int i = 0; i < tabWidget->count(); ++i) {
        QWidget *widget = tabWidget->widget(i);
        if (widget->property("filePath").toString() == fileName) {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }

    // TODO: тут только заводится вкладка с именем файла — сама схема из
    // fileName пока никак не десериализуется и не строится на сцене.
    // Нужен формат сохранения (например, JSON: список проводов/компонентов
    // с их Pin'ами и m_params) и парная функция saveProject().
    const QFileInfo fileInfo(fileName);
    createNewTab(fileInfo.baseName());

    if (QWidget *currentWidget = tabWidget->currentWidget())
        currentWidget->setProperty("filePath", fileName);
}

void EditorWindow::onUndo() {
    if (InfiniteGraphicsView *view = getCurrentView()) view->undo();
    updateEditActionsState();
}

void EditorWindow::onRedo() {
    if (InfiniteGraphicsView *view = getCurrentView()) view->redo();
    updateEditActionsState();
}

// =============================================================
// Шестиугольная палитра как на референсе:
// тон — по углу, насыщенность — от центра, белый центр, тёмный обод
// =============================================================
class HexColorPalette : public QWidget {
public:
    std::function<void(const QColor &)> onColorPicked;

    explicit HexColorPalette(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(260, 240);
    }
    QSize sizeHint() const override { return QSize(300, 270); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const qreal s = hexSize();
        const QPointF c = QPointF(width() / 2.0, height() / 2.0);
        const QList<Cell> cells = buildCells(s, c);
        for (const Cell &cell : cells) {
            p.setBrush(cell.color);
            p.setPen(QPen(QColor(0, 0, 0, 90), 1));
            p.drawPolygon(hexPolygon(cell.center, s * 0.98));
        }
    }

    void mousePressEvent(QMouseEvent *e) override {
        const qreal s = hexSize();
        const QPointF c = QPointF(width() / 2.0, height() / 2.0);
        QColor picked; qreal bestD = 1e9;
        const QList<Cell> cells = buildCells(s, c);
        for (const Cell &cell : cells) {
            const qreal d = QLineF(e->position(), cell.center).length();
            if (d < bestD) { bestD = d; picked = cell.color; }
        }
        if (bestD <= s && onColorPicked) onColorPicked(picked);
    }

private:
    struct Cell { QPointF center; QColor color; };
    static constexpr int R = 4;   // число колец сот (всего 61 ячейка)

    qreal hexSize() const {
        const qreal byW = (width()  - 20) / (std::sqrt(3.0) * (2 * R + 1));
        const qreal byH = (height() - 20) / (1.5 * (2 * R) + 2);
        return qMax(4.0, qMin(byW, byH));
    }

    static QPolygonF hexPolygon(const QPointF &c, qreal s) {
        QPolygonF poly;
        for (int i = 0; i < 6; ++i) {
            const qreal a = M_PI / 180.0 * (60 * i - 30);   // соты «остриём вверх»
            poly << c + s * QPointF(std::cos(a), std::sin(a));
        }
        return poly;
    }

    QList<Cell> buildCells(qreal s, const QPointF &c) const {
        QList<Cell> cells;
        const qreal maxDist = std::sqrt(3.0) * s * R;
        for (int q = -R; q <= R; ++q) {
            for (int r = qMax(-R, -q - R); r <= qMin(R, -q + R); ++r) {
                QPointF pos = c + QPointF(s * std::sqrt(3.0) * (q + r / 2.0),
                                          s * 1.5 * r);
                const qreal dx = pos.x() - c.x();
                const qreal dy = pos.y() - c.y();
                const qreal t  = qBound(0.0, std::hypot(dx, dy) / maxDist, 1.0);
                qreal hue = std::atan2(dy, dx) * 180.0 / M_PI - 60.0;  // как на референсе: справа малиновый, сверху синий, слева зелёный
                hue = std::fmod(hue + 360.0, 360.0);
                const int sat = int(255 * t);                            // центр белый → края насыщенные
                const int val = t < 0.7 ? 255 : int(255 - (t - 0.7) / 0.3 * 110); // тёмный обод
                cells.append({pos, QColor::fromHsv(int(hue), sat, val)});
            }
        }
        return cells;
    }
};

void EditorWindow::createSettingsTab() {
    // Не создаём дубликаты
    for (int i = 0; i < tabWidget->count(); ++i) {
        QWidget *w = tabWidget->widget(i);
        if (w && w->property("isSettings").toBool()) { tabWidget->setCurrentIndex(i); return; }
    }

    QWidget *content = new QWidget;
    content->setProperty("isSettings", true);
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(24, 16, 24, 16);
    lay->setSpacing(16);

    QLabel *head = new QLabel("Настройки");
    head->setStyleSheet("font-size:18px; font-weight:bold; background:transparent;");
    lay->addWidget(head);

    // ---------- ТЕМА: выпадающий список ----------
    auto *themeCombo = new QComboBox;
    themeCombo->addItems({"Тёмный (по умолчанию)", "Белый", "Черный"});
    themeCombo->setFixedWidth(220);
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                QColor c;
                switch(idx) {
                case 1:  c = QColor(255, 255, 255); break;   // Белый
                case 2:  c = QColor(0, 0, 0); break;         // Черный
                default: c = QColor(30, 30, 46); break;      // Тёмный (по умолчанию)
                }
                applyTheme(c);   // сразу во всём приложении
            });

    auto *themeRow = new QHBoxLayout;
    themeRow->setSpacing(12);
    themeRow->addWidget(new QLabel("Тема:"));
    themeRow->addWidget(themeCombo);
    themeRow->addStretch();
    lay->addLayout(themeRow);

    // ---------- ПАЛИТРА ----------
    auto *hexPalette = new HexColorPalette(content);
    hexPalette->onColorPicked = [this](const QColor &c) {
        applyTheme(c);   // свой оттенок из сот — сразу во всём приложении
    };
    lay->addWidget(hexPalette, 0, Qt::AlignLeft);

    // ---------- Форма курсора ----------
    auto *curCombo = new QComboBox;
    curCombo->addItems({"Стрелка (по умолчанию)", "Перекрестие", "Рука"});
    curCombo->setFixedWidth(220);
    connect(curCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                Qt::CursorShape s = Qt::ArrowCursor;
                if (idx == 1) s = Qt::CrossCursor;
                if (idx == 2) s = Qt::OpenHandCursor;
                m_cursorShape = s;
                for (int i = 0; i < tabWidget->count(); ++i) {
                    QWidget *w = tabWidget->widget(i);
                    if (w && !w->property("isSettings").toBool())
                        for (InfiniteGraphicsView *v : w->findChildren<InfiniteGraphicsView*>())
                            v->setCursor(s);
                }
            });

    auto *curRow = new QHBoxLayout;
    curRow->setSpacing(12);
    curRow->addWidget(new QLabel("Форма курсора:"));
    curRow->addWidget(curCombo);
    curRow->addStretch();
    lay->addLayout(curRow);

    lay->addStretch();

    int idx = tabWidget->addTab(content, "Настройки");
    tabWidget->setCurrentIndex(idx);
}

void EditorWindow::applyTheme(const QColor &bg)
{
    qApp->setStyleSheet(buildAppStyleSheet(bg));   // всё приложение
    m_backgroundColor = bg;   // запоминаем — используется при создании следующих вкладок

    if (!tabWidget) return;   // вкладки ещё не созданы — красить нечего
    for (int i = 0; i < tabWidget->count(); ++i) {
        QWidget *w = tabWidget->widget(i);
        if (!w) continue;
        const QList<InfiniteGraphicsView*> views = w->findChildren<InfiniteGraphicsView*>();
        for (InfiniteGraphicsView *v : views) {
            // InfiniteGraphicsScene не имеет Q_OBJECT (не проходит через moc),
            // поэтому qobject_cast здесь не сработает — нужен dynamic_cast (RTTI).
            if (auto *s = dynamic_cast<InfiniteGraphicsScene*>(v->scene()))
                s->setBackgroundColor(bg);
        }
    }
}

void EditorWindow::applyCursorTheme(Qt::CursorShape shape)
{
    m_cursorShape = shape;
    if (!tabWidget) return;
    for (int i = 0; i < tabWidget->count(); ++i) {   // сразу во всех вкладках
        QWidget *w = tabWidget->widget(i);
        if (!w) continue;
        for (InfiniteGraphicsView *v : w->findChildren<InfiniteGraphicsView*>())
            v->setPreferredCursor(shape);
    }
}