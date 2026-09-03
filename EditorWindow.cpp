//#pragma once
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
    qDebug() << "EditorWindow::EditorWindow() - создание окна редактора";
    setupUI();
    qDebug() << "EditorWindow::EditorWindow() - завершено";
}

void EditorWindow::setupUI() {
    qDebug() << "EditorWindow::setupUI() - начало настройки интерфейса";
    //applyTheme(QColor(30, 30, 46));   // изначально темно везде

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---------- Строка меню ----------
    QMenuBar *menuBar = new QMenuBar(this);
    // ----- Меню "Файл" -----
    QMenu *fileMenu = menuBar->addMenu("Файл");
    qDebug() << "  Создано меню 'Файл'";

    QAction *newAction = fileMenu->addAction("Новый проект");
    connect(newAction, &QAction::triggered, this, &EditorWindow::onNewProject);
    qDebug() << "  Добавлено действие 'Новый проект'";

    QAction *openAction = fileMenu->addAction("Открыть...");
    connect(openAction, &QAction::triggered, this, &EditorWindow::onOpenProject);

    QAction *saveAction = fileMenu->addAction("Сохранить");
    fileMenu->addSeparator();

    QAction *exportAction = fileMenu->addAction("Экспортировать как...");
    QAction *exitAction = fileMenu->addAction("Выйти");
    connect(exitAction, &QAction::triggered, this, &EditorWindow::backToMainMenu);

    // ----- Меню "Правка" -----
    QMenu *PravkaMenu = menuBar->addMenu("Правка");

    // ----- Меню "Настройки" -----
    QMenu *settingsMenu = menuBar->addMenu("Настройки");
    QAction *paramsAction = settingsMenu->addAction("Параметры (скоро)");
    paramsAction->setEnabled(false);   // заглушка — позже сюда добавишь свои параметры
    settingsMenu->addSeparator();
    QAction *advSettingsAction = settingsMenu->addAction("Доп. настройки");
    connect(advSettingsAction, &QAction::triggered, this, &EditorWindow::createSettingsTab);

    undoAction = new QAction("Отменить", this);
    undoAction->setShortcut(QKeySequence::Undo);   // Ctrl+Z на всех платформах
    undoAction->setEnabled(false);
    connect(undoAction, &QAction::triggered, this, &EditorWindow::onUndo);

    redoAction = new QAction("Повторить", this);
    redoAction->setShortcut(QKeySequence::Redo);   // Ctrl+Y на Windows
    redoAction->setEnabled(false);
    connect(redoAction, &QAction::triggered, this, &EditorWindow::onRedo);

    PravkaMenu->addAction(undoAction);
    PravkaMenu->addAction(redoAction);
    PravkaMenu->addSeparator();
    PravkaMenu->addAction("Копировать");
    PravkaMenu->addAction("Вставить");

    connect(PravkaMenu, &QMenu::aboutToShow, this, &EditorWindow::updateUndoRedoActions);

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

    // ----- Меню "Edit" -----
    m_editMenu = menuBar->addMenu("Edit");
    buildPersistentEditMenu(m_editMenu);   // строится один раз — см. комментарий в .h
    connect(m_editMenu, &QMenu::aboutToShow, this, [this]() {
        updatePersistentEditMenuState();
        updateUndoRedoActions();
    });

    mainLayout->addWidget(menuBar);

    // ---------- Область вкладок ----------
    tabWidget = new QTabWidget(this);
    qDebug() << " Создан QTabWidget";
    tabWidget->setTabsClosable(true);
    tabWidget->setDocumentMode(true);
    tabWidget->setMovable(true);

    // ДОБАВИТЬ ЭТУ СТРОКУ:
    tabWidget->tabBar()->installEventFilter(this);

    mainLayout->addWidget(tabWidget, 1);

    QString initialName = generateUniqueTabName("Проект");
    qDebug() << "  Создаём начальную вкладку с именем:" << initialName;
    createNewTab(initialName);
    applyTheme(m_backgroundColor);

    connect(tabWidget, &QTabWidget::tabCloseRequested,
            this, &EditorWindow::closeTab);

    qDebug() << "EditorWindow::setupUI() - завершено";
}

void EditorWindow::contextMenuEvent(QContextMenuEvent *event) {
    qDebug() << "EditorWindow::contextMenuEvent() - позиция:" << event->pos();
    //updateUndoRedoActions();          // ← добавить
    QMenu menu(this);
    Draw_Edit_Menu(&menu);
    menu.exec(event->globalPos());
}

void EditorWindow::Draw_Edit_Menu(QMenu *menu) {
    qDebug() << "EditorWindow::Draw_Edit_Menu() - заполнение меню элементами";
    InfiniteGraphicsView *view = getCurrentView();

    if (!view) {
        qDebug() << "  ОШИБКА: getCurrentView вернул nullptr!";
        return;  // <-- Добавьте эту проверку
    }

    QAction *wireAction = menu->addAction("Wire\tw");
    wireAction->setCheckable(true);
    wireAction->setChecked(view && view->isWireMode());
    connect(wireAction, &QAction::triggered, this, [this]() {
        InfiniteGraphicsView *view = getCurrentView();
        if (!view) return;
        view->setWireMode(!view->isWireMode());
    });

    QAction *resistorAction = menu->addAction("Resistor\tr");
    resistorAction->setCheckable(true);
    resistorAction->setChecked(view && view->isComponentMode() &&
                               view->currentComponentType() == ComponentType::Resistor);
    connect(resistorAction, &QAction::triggered, this, [this]() {
        InfiniteGraphicsView *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Resistor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Resistor);
    });

    QAction *capAction = menu->addAction("Non-polar capacitor\tc");
    capAction->setCheckable(true);
    capAction->setChecked(view && view->isComponentMode() &&
                          view->currentComponentType() == ComponentType::Non_Polar_Capacitor);
    connect(capAction, &QAction::triggered, this, [this]() {
        InfiniteGraphicsView *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Non_Polar_Capacitor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Non_Polar_Capacitor);
    });
    menu->addAction("Polar capacitor");
    menu->addSeparator();

    QMenu *MPassive_elements = menu->addMenu("Passive elements");
    MPassive_elements->addAction("Inductor coil");
    MPassive_elements->addAction("Switch");

    QMenu *MActive_elements = menu->addMenu("Active elements");
    MActive_elements->addAction("Diode");

    QMenu *MActive_blocks = menu->addMenu("Active blocks");
    MActive_blocks->addAction("Operational amplifier ( - top)");

    QMenu *MLogic_elements_Input_Output = menu->addMenu("Logic elements, inputs and outputs");
    QMenu *MDigital_elements = menu->addMenu("Digital elements");
    QMenu *MAnalog_and_Hubrid_elements = menu->addMenu("Analog and hybrid elements");

    menu->addSeparator();
    menu->addAction(undoAction);
    menu->addAction(redoAction);
    menu->addSeparator();

    // === НОВОЕ: буфер обмена в меню ===
    QAction *copyAction = menu->addAction("Копировать\tCtrl+C");
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (InfiniteGraphicsView *v = getCurrentView()) v->copySelected();
    });
    QAction *cutAction = menu->addAction("Вырезать\tCtrl+X");
    connect(cutAction, &QAction::triggered, this, [this]() {
        if (InfiniteGraphicsView *v = getCurrentView()) v->cutSelected();
    });
    QAction *pasteAction = menu->addAction("Вставить\tCtrl+V");
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (InfiniteGraphicsView *v = getCurrentView()) v->pasteClipboard();
    });
    menu->addSeparator();

    // ---------- Выделить (чекбокс, режим selection) ----------
    QAction *selectAction = menu->addAction("Выделить");
    selectAction->setCheckable(true);
    selectAction->setChecked(view && !view->isWireMode() && !view->isComponentMode());
    connect(selectAction, &QAction::triggered, this, [this]() {
        InfiniteGraphicsView *v = getCurrentView();
        if (v) {
            v->setWireMode(false);
            v->disableComponentMode();   // <-- добавить
        }
    });

    QAction *deleteAction = menu->addAction("Удалить\t Del");
    bool hasSelection = view && view->scene() && !view->scene()->selectedItems().isEmpty();
    deleteAction->setEnabled(hasSelection);
    connect(deleteAction, &QAction::triggered, this, [this]() {
        InfiniteGraphicsView *view = getCurrentView();
        if (view) view->deleteSelectedItems();
    });
}

// =============================================================
// Постоянное меню "Edit" (menu bar) — в отличие от Draw_Edit_Menu()
// выше (который создаёт временное меню под правым кликом заново
// при каждом вызове), эти два метода разделяют "построить один раз"
// и "обновить состояние" — чтобы не плодить QAction/QMenu объекты
// при каждом открытии меню в menu bar.
// =============================================================
void EditorWindow::buildPersistentEditMenu(QMenu *menu) {
    m_wireMenuAction = menu->addAction("Wire\tw");
    m_wireMenuAction->setCheckable(true);
    connect(m_wireMenuAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->setWireMode(!v->isWireMode());
    });

    m_resistorMenuAction = menu->addAction("Resistor\tr");
    m_resistorMenuAction->setCheckable(true);
    connect(m_resistorMenuAction, &QAction::triggered, this, [this]() {
        auto *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Resistor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Resistor);
    });

    m_capMenuAction = menu->addAction("Non-polar capacitor\tc");
    m_capMenuAction->setCheckable(true);
    connect(m_capMenuAction, &QAction::triggered, this, [this]() {
        auto *v = getCurrentView();
        if (!v) return;
        if (v->isComponentMode() && v->currentComponentType() == ComponentType::Non_Polar_Capacitor)
            v->disableComponentMode();
        else
            v->setComponentMode(ComponentType::Non_Polar_Capacitor);
    });
    menu->addAction("Polar capacitor");
    menu->addSeparator();

    QMenu *passive = menu->addMenu("Passive elements");
    passive->addAction("Inductor coil");
    passive->addAction("Switch");

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

    QAction *copyAction = menu->addAction("Копировать\tCtrl+C");
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->copySelected();
    });
    QAction *cutAction = menu->addAction("Вырезать\tCtrl+X");
    connect(cutAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->cutSelected();
    });
    QAction *pasteAction = menu->addAction("Вставить\tCtrl+V");
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->pasteClipboard();
    });
    menu->addSeparator();

    m_selectMenuAction = menu->addAction("Выделить");
    m_selectMenuAction->setCheckable(true);
    connect(m_selectMenuAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) {
            v->setWireMode(false);
            v->disableComponentMode();
        }
    });

    m_deleteMenuAction = menu->addAction("Удалить\t Del");
    connect(m_deleteMenuAction, &QAction::triggered, this, [this]() {
        if (auto *v = getCurrentView()) v->deleteSelectedItems();
    });

    updatePersistentEditMenuState();   // сразу выставить корректное начальное состояние
}

void EditorWindow::updatePersistentEditMenuState() {
    InfiniteGraphicsView *view = getCurrentView();
    m_wireMenuAction->setChecked(view && view->isWireMode());
    m_resistorMenuAction->setChecked(view && view->isComponentMode() &&
                                     view->currentComponentType() == ComponentType::Resistor);
    m_capMenuAction->setChecked(view && view->isComponentMode() &&
                                view->currentComponentType() == ComponentType::Non_Polar_Capacitor);
    m_selectMenuAction->setChecked(view && !view->isWireMode() && !view->isComponentMode());
    m_deleteMenuAction->setEnabled(view && view->scene() && !view->scene()->selectedItems().isEmpty());
}

// =============================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// =============================================================

InfiniteGraphicsView* EditorWindow::getCurrentView() const {
    if (!tabWidget) {
        qDebug() << "  getCurrentView: tabWidget ещё не создан";
        return nullptr;
    }
    QWidget *currentWidget = tabWidget->currentWidget();
    if (!currentWidget) {
        qDebug() << "  getCurrentView: currentWidget is null";
        return nullptr;
    }
    QList<QGraphicsView*> views = currentWidget->findChildren<QGraphicsView*>();
    if (views.isEmpty()) {
        qDebug() << "  getCurrentView: QGraphicsView не найден";
        return nullptr;
    }
    InfiniteGraphicsView *view = qobject_cast<InfiniteGraphicsView*>(views.first());
    if (!view) {
        qDebug() << "  getCurrentView: не удалось привести к InfiniteGraphicsView";
        return nullptr;
    }
    return view;
}
// =============================================================
// СЛОТЫ ДЛЯ ЗУМА И СБРОСА
// =============================================================

void EditorWindow::onZoomIn() {
    qDebug() << "EditorWindow::onZoomIn() - увеличение масштаба";
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) return;

    qreal factor = 1.25;
    view->scale(factor, factor);
    qDebug() << "  масштаб увеличен в" << factor << "раз";
}

void EditorWindow::onZoomOut() {
    qDebug() << "EditorWindow::onZoomOut() - уменьшение масштаба";
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) return;

    qreal factor = 1.0 / 1.25;
    view->scale(factor, factor);
    qDebug() << "  масштаб уменьшен в" << 1.25 << "раз";
}

void EditorWindow::onFitToSize() {
    qDebug() << "EditorWindow::onFitToSize() - подгонка по размеру";
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) return;

    QGraphicsScene *scene = view->scene();
    if (!scene) return;

    // Получаем все элементы на сцене
    QList<QGraphicsItem*> items = scene->items();

    // Фильтруем только пользовательские элементы (не оси и не текстовые метки)
    QList<QGraphicsItem*> userItems;
    for (QGraphicsItem *item : items) {
        // !!! ИСПРАВЛЕНИЕ: используем qgraphicsitem_cast вместо qobject_cast !!!
        // QGraphicsItem не является QObject, поэтому qobject_cast для него запрещён.

        // Игнорируем текстовые метки (они созданы через addText)
        if (qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            continue;
        }

        // Игнорируем линии осей
        if (auto* line = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            // Пропускаем оси (длинные линии от -200 до 200, их длина = 400)
            if (line->line().length() > 300) {
                continue;
            }
        }

        // Если это не текст и не длинная линия-ось, считаем это пользовательским объектом
        userItems.append(item);
    }

    if (userItems.isEmpty()) {
        // Ничего нет на сцене - сбрасываем в центр с начальным масштабом
        qDebug() << "  на сцене нет пользовательских объектов, сброс в 0,0 с масштабом 2.0";
        view->resetTransform();
        view->scale(2.0, 2.0); // Начальный масштаб как в createNewTab

        // Центрируем на (0,0)
        view->centerOn(0, 0);
    } else {
        // ЕСТЬ ОБЪЕКТЫ - заглушка под будущую реализацию
        qDebug() << "  на сцене найдено" << userItems.size() << "пользовательских объектов(а)";
        qDebug() << "  TODO: реализовать точную подгонку масштаба под bounding rect этих объектов";

        // Временная реализация: просто центрируем вид на общем прямоугольнике всех объектов сцены
        QRectF sceneRect = scene->itemsBoundingRect();
        qDebug() << "  общий bounding rect сцены:" << sceneRect;

        view->centerOn(sceneRect.center());
    }
}

void EditorWindow::onResetView() {
    qDebug() << "EditorWindow::onResetView() - сброс вида";
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) return;

    // Сбрасываем трансформацию (масштаб)
    view->resetTransform();

    // Устанавливаем начальный масштаб
    view->scale(2.0, 2.0);

    // Центрируем на (0,0)
    view->centerOn(0, 0);

    qDebug() << "  вид сброшен: масштаб 2.0, позиция (0,0)";
}

// =============================================================
// СОЗДАНИЕ НОВОЙ ВКЛАДКИ
// =============================================================

void EditorWindow::createNewTab(const QString &title) {
    qDebug() << "EditorWindow::createNewTab() - создание вкладки с заголовком:" << title;

    QWidget *tabContent = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tabContent);
    layout->setContentsMargins(0, 0, 0, 0);

    InfiniteGraphicsView *view = new InfiniteGraphicsView;
    view->setPreferredCursor(m_cursorShape);
    InfiniteGraphicsScene *scene = new InfiniteGraphicsScene(view);
    scene->setBackgroundColor(m_backgroundColor);   // фон рисует сцена (drawBackground), не вьюха

    scene->setSceneRect(-10000, -10000, 20000, 20000);
    qDebug() << "  sceneRect установлена: (-10000, -10000, 20000, 20000)";

    // ====== РИСУЕМ ОСИ И МЕТКИ ======
    QPen axisPen(Qt::white, 1.5);
    QPen tickPen(Qt::gray, 1);

    scene->addLine(-200, 0, 200, 0, axisPen);
    scene->addLine(0, -200, 0, 200, axisPen);

    for (int x = -200; x <= 200; x += 50) {
        if (x == 0) continue;
        scene->addLine(x, -5, x, 5, tickPen);
        QGraphicsTextItem *text = scene->addText(QString::number(x));
        text->setPos(x - 10, 5);
        text->setDefaultTextColor(Qt::lightGray);
        text->setScale(0.5);
    }

    for (int y = -200; y <= 200; y += 50) {
        if (y == 0) continue;
        scene->addLine(-5, y, 5, y, tickPen);
        QGraphicsTextItem *text = scene->addText(QString::number(y));
        text->setPos(-25, y - 8);
        text->setDefaultTextColor(Qt::lightGray);
        text->setScale(0.5);
    }

    QGraphicsTextItem *labelX = scene->addText("X");
    labelX->setPos(205, -10);
    labelX->setDefaultTextColor(Qt::white);
    labelX->setScale(1.2);

    QGraphicsTextItem *labelY = scene->addText("Y");
    labelY->setPos(-25, -215);
    labelY->setDefaultTextColor(Qt::white);
    labelY->setScale(1.2);

    scene->addEllipse(-10, -10, 20, 20, QPen(Qt::red, 2), QBrush(Qt::yellow));

    QGraphicsTextItem *centerLabel = scene->addText("0,0");
    centerLabel->setPos(15, -10);
    centerLabel->setDefaultTextColor(Qt::green);
    centerLabel->setScale(0.8);
    // =================================================

    view->setScene(scene);
    view->scale(2.0, 2.0);
    view->centerOn(0, 0);
    qDebug() << "  начальный масштаб 2.0 применён, вид центрирован на (0,0)";

    layout->addWidget(view);

    int index = tabWidget->addTab(tabContent, title);
    qDebug() << "  вкладка добавлена, индекс =" << index;
    tabWidget->setCurrentIndex(index);
    qDebug() << "  текущая вкладка установлена на" << index;

    connect(view->undoStack(), &QUndoStack::canUndoChanged, this, [this]() {
        updateUndoRedoActions();
    });
    connect(view->undoStack(), &QUndoStack::canRedoChanged, this, [this]() {
        updateUndoRedoActions();
    });
}

// =============================================================
// ОСТАЛЬНЫЕ СЛОТЫ И ФУНКЦИИ
// =============================================================

void EditorWindow::onNewProject() {
    qDebug() << "EditorWindow::onNewProject() - создание нового проекта";
    QString newName = generateUniqueTabName("Проект");
    qDebug() << "  сгенерировано имя:" << newName;
    createNewTab(newName);
}

void EditorWindow::closeTab(int index) {
    qDebug() << "EditorWindow::closeTab() - закрытие вкладки с индексом" << index;
    QWidget *widget = tabWidget->widget(index);
    if (widget) {
        tabWidget->removeTab(index);
        delete widget;
        qDebug() << "  вкладка удалена";
    } else {
        qDebug() << "  ошибка: виджет не найден";
    }
}

bool EditorWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == tabWidget->tabBar()) {
#ifdef QT_DEBUG
        qDebug() << "eventFilter: событие на панели вкладок, тип =" << event->type();
#endif
        if (event->type() == QEvent::MouseButtonDblClick) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            int index = tabWidget->tabBar()->tabAt(mouseEvent->pos());
            if (index != -1) {
                // Вкладку настроек переименовывать НЕЛЬЗЯ
                QWidget *w = tabWidget->widget(index);
                if (w && w->property("isSettings").toBool()) {
                    return true;   // глотаем событие — диалог не откроется
                }
                QTimer::singleShot(0, this, [this, index]() {
                    renameTab(index);
                });
                return true;
            }
        }
        else if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent*>(event);
            int index = tabWidget->tabBar()->tabAt(contextEvent->pos());
            if (index != -1) {
                qDebug() << "  правый клик по вкладке" << index << "- показ контекстного меню (отложенный вызов)";
                QPoint globalPos = contextEvent->globalPos();
                // По той же причине откладываем menu.exec()
                QTimer::singleShot(0, this, [this, index, globalPos]() {
                    showTabContextMenu(index, globalPos);
                });
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EditorWindow::renameTab(int index) {
    qDebug() << "EditorWindow::renameTab() - переименование вкладки" << index;
    QString currentTitle = tabWidget->tabText(index);
    bool ok;
    QString newTitle = QInputDialog::getText(this,
                                             "Переименование",
                                             "Введите новое имя вкладки:",
                                             QLineEdit::Normal,
                                             currentTitle,
                                             &ok);
    if (!ok || newTitle.isEmpty()) {
        qDebug() << "  переименование отменено или пустое имя";
        return;
    }

    for (int i = 0; i < tabWidget->count(); ++i) {
        if (i != index && tabWidget->tabText(i) == newTitle) {
            QMessageBox::warning(this, "Ошибка", "Вкладка с таким именем уже существует.");
            qDebug() << "  ошибка: имя" << newTitle << "уже занято";
            return;
        }
    }
    tabWidget->setTabText(index, newTitle);
    qDebug() << "  вкладка переименована в" << newTitle;
}

void EditorWindow::showTabContextMenu(int index, const QPoint &globalPos) {
    qDebug() << "EditorWindow::showTabContextMenu() - для вкладки" << index;
    QMenu menu(this);
    QAction *New_tab = menu.addAction("Create");
    connect(New_tab, &QAction::triggered, this, &EditorWindow::onNewProject);

    bool isSettings = tabWidget->widget(index) &&
                      tabWidget->widget(index)->property("isSettings").toBool();

    QAction *renameAction = menu.addAction("Переименовать");
    renameAction->setEnabled(!isSettings);   // ← для настроек недоступно
    connect(renameAction, &QAction::triggered, this, [this, index]() {
        renameTab(index);
    });

    menu.addSeparator();

    QAction *closeAction = menu.addAction("Закрыть");
    connect(closeAction, &QAction::triggered, this, [this, index]() {
        closeTab(index);
    });

    QAction *closeAllAction = menu.addAction("Закрыть все");
    connect(closeAllAction, &QAction::triggered, this, [this]() {
        qDebug() << "  закрыть все вкладки";
        while (tabWidget->count() > 0) {
            closeTab(0);
        }
    });

    QAction *closeOthersAction = menu.addAction("Закрыть другие");
    connect(closeOthersAction, &QAction::triggered, this, [this, index]() {
        qDebug() << "  закрыть другие вкладки, кроме" << index;
        for (int i = tabWidget->count() - 1; i >= 0; --i) {
            if (i != index) {
                closeTab(i);
            }
        }
    });

    menu.exec(globalPos);
}

QString EditorWindow::generateUniqueTabName(const QString &baseName) {
    qDebug() << "EditorWindow::generateUniqueTabName() - базовое имя:" << baseName;
    QStringList existingNames;
    for (int i = 0; i < tabWidget->count(); ++i) {
        existingNames << tabWidget->tabText(i);
    }
    if (!existingNames.contains(baseName)) {
        qDebug() << "  имя свободно, возвращаем" << baseName;
        return baseName;
    }
    int suffix = 2;
    while (true) {
        QString candidate = baseName + " (" + QString::number(suffix) + ")";
        if (!existingNames.contains(candidate)) {
            qDebug() << "  найдено свободное имя:" << candidate;
            return candidate;
        }
        ++suffix;
    }
}

void EditorWindow::onOpenProject() {
    qDebug() << "EditorWindow::onOpenProject() - открытие проекта";
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Открыть проект",
                                                    QDir::homePath(),
                                                    "Файлы проектов (*.prj);;Все файлы (*)");
    if (fileName.isEmpty()) {
        qDebug() << "  выбор файла отменён";
        return;
    }
    qDebug() << "  выбран файл:" << fileName;

    for (int i = 0; i < tabWidget->count(); ++i) {
        QWidget *widget = tabWidget->widget(i);
        if (widget->property("filePath").toString() == fileName) {
            tabWidget->setCurrentIndex(i);
            qDebug() << "  файл уже открыт во вкладке" << i << ", переключаемся";
            return;
        }
    }

    QFileInfo fileInfo(fileName);
    QString tabTitle = fileInfo.baseName();
    qDebug() << "  создаём вкладку с именем" << tabTitle;
    createNewTab(tabTitle);

    QWidget *currentWidget = tabWidget->currentWidget();
    if (currentWidget) {
        currentWidget->setProperty("filePath", fileName);
        qDebug() << "  путь сохранён в свойстве виджета";
    }
}

void EditorWindow::onUndo() {
    qDebug() << "EditorWindow::onUndo()";
    InfiniteGraphicsView *view = getCurrentView();
    if (view) view->undo();
    updateUndoRedoActions();
}

void EditorWindow::onRedo() {
    qDebug() << "EditorWindow::onRedo()";
    InfiniteGraphicsView *view = getCurrentView();
    if (view) view->redo();
    updateUndoRedoActions();
}

void EditorWindow::updateUndoRedoActions() {
    qDebug() << "updateUndoRedoActions called";
    InfiniteGraphicsView *view = getCurrentView();
    if (!view) {
        qDebug() << "  view is null, disabling actions";
        undoAction->setEnabled(false);
        redoAction->setEnabled(false);
        undoAction->setText("Отменить");
        redoAction->setText("Повторить");
        return;
    }
    bool canUndo = view->canUndo();
    bool canRedo = view->canRedo();
    qDebug() << "  canUndo =" << canUndo << "canRedo =" << canRedo;
    undoAction->setEnabled(canUndo);
    redoAction->setEnabled(canRedo);
    undoAction->setText(canUndo ? "Отменить: " + view->undoText() : "Отменить");
    redoAction->setText(canRedo ? "Повторить: " + view->redoText() : "Повторить");
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

    // ---------- ТЕМА: выпадающий список ВМЕСТО кнопки ----------
    auto *themeCombo = new QComboBox;
    themeCombo->addItems({"Тёмный (по умолчанию)", "Белый", "Черный"});  // ← добавили "Черный"
    themeCombo->setFixedWidth(220);
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                QColor c;
                switch(idx) {
                case 1:  c = QColor(255, 255, 255); break;   // Белый
                case 2:  c = QColor(0, 0, 0); break;         // Черный
                default: c = QColor(30, 30, 46); break;      // Тёмный (по умолчанию)
                }
                applyTheme(c);   // ← сразу во всём приложении
            });

    auto *themeRow = new QHBoxLayout;
    themeRow->setSpacing(12);
    themeRow->addWidget(new QLabel("Тема:"));
    themeRow->addWidget(themeCombo);
    themeRow->addStretch();           // всё лишнее место — вправо, список слева
    lay->addLayout(themeRow);

    // ---------- ПАЛИТРА: прижата ВЛЕВО (было по центру) ----------
    auto *hexPalette = new HexColorPalette(content);
    hexPalette->onColorPicked = [this](const QColor &c) {
        applyTheme(c);                // свой оттенок из сот — сразу во всём приложении
    };
    lay->addWidget(hexPalette, 0, Qt::AlignLeft);   // ← было Qt::AlignHCenter

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

    if (!tabWidget) return;   // ← вкладки ещё не созданы — красить нечего
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
    for (int i = 0; i < tabWidget->count(); ++i) {   // сразу во всех вкладках
        QWidget *w = tabWidget->widget(i);
        if (!w) continue;
        for (InfiniteGraphicsView *v : w->findChildren<InfiniteGraphicsView*>())
            v->setPreferredCursor(shape);
    }
}