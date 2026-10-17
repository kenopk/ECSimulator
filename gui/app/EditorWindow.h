#pragma once

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMainWindow>
#include <QPointer>
#include <QTabWidget>
#include <QVector>

#include "sim/SimIssue.h"
#include "util/CoreTypes.h"
#include "widgets/DetachableTabBar.h"

class CircuitItem;
class CircuitView;
class KarnaughSection;
class ProjectPanel;
struct ProjectDoc;
class IssueLog;
class ScopeBoard;
class ScopeItem;
class ScopeSource;
class PropertiesPanel;
class SimulationPanel;
class QAction;
class QActionGroup;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QMenu;
class QTabBar;
class QStackedWidget;
class QToolButton;
class QUndoGroup;

// QTabWidget с полосой вкладок DetachableTabBar: вкладки можно переносить между окнами и отрывать в новое окно.
class EditorTabs : public QTabWidget
{
public:
    explicit EditorTabs(QWidget *parent = nullptr)
        : QTabWidget(parent), m_bar(new DetachableTabBar(this))
    {
        setTabBar(m_bar);
        setAcceptDrops(true);          // вкладку можно бросить в любое место области вкладок
    }
    DetachableTabBar *bar() const { return m_bar; }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (DetachableTabBar::canAccept(e->mimeData(), 0))
            e->acceptProposedAction();
        else
            e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *e) override
    {
        if (DetachableTabBar::canAccept(e->mimeData(), 0))
            e->acceptProposedAction();
        else
            e->ignore();
    }
    void dropEvent(QDropEvent *e) override
    {
        if (!DetachableTabBar::canAccept(e->mimeData(), 0)) {
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

// Окно редактора: вкладки со схемами, меню элементов, правая панель
// (симуляция + свойства), статус-строка. Каждая вкладка — отдельный
// CircuitView со своим стеком отмены.
class EditorWindow : public QMainWindow
{
    Q_OBJECT
public:
    // standalone — отдельное окно верхнего уровня (вкладка, оторванная от основного окна).
    explicit EditorWindow(QWidget *parent = nullptr, bool standalone = false);
    ~EditorWindow() override;

    void newTab();
    void newKarnaughTab();

    // Проекты: в приложении открыто несколько, у каждого свои схемы и карты Карно; проект сохраняется одним файлом .govno.
    void newProject();
    void openProject();                           // спрашивает файл
    bool openProjectFile(const QString &path);
    bool saveProject(int projectId, bool saveAs);          // 0 — активный проект
    void closeProject(int projectId, bool askToSave = true);   // 0 — активный проект
    void showSection(int section, bool autoCreate = true);   // autoCreate: пустой раздел карт сам создаёт первую карту
    void onSimulationToggled(bool on);
    void recheckIssues();                         // схема изменилась: перепроверить правила
    void clearIssues();
    void showScopeInDock(CircuitItem *item);   // осциллограмма элемента в нижней панели
    void showScopeInDock(ScopeSource *source);
    void detachScope(ScopeSource *source);      // осциллограмма в отдельное окно
    void scopeToScheme(ScopeSource *source);    // осциллограмма на схему
    void showScopeMenu(ScopeItem *item, const QPoint &globalPos);       // 0 — симулятор цепей, 1 — карты Карно
    int tabCount() const { return m_tabs ? m_tabs->count() : 0; }
    bool isStandalone() const { return m_standalone; }

    // Панель управления симуляцией: сюда подключится ядро
    // (runningChanged, simulationSpeedChanged, resetClicked ...).
    SimulationPanel *simulationPanel() const { return m_sim; }

    // Резкая остановка симуляции из-за проблем: кнопка «Старт», виновники красным, плашка с причиной на схеме
    // и записи в журнале симуляции. Ядро вызывает это, когда находит проблему (нет земли, вырожденная
    // матрица, не сходится…); правило «резистор перед индикатором» интерфейс проверяет сам (sim/Preflight).
    void stopSimulationWithIssues(const QVector<SimIssue> &issues);
    IssueLog *simulationLog() const { return m_log; }

signals:
    void backToMainMenu();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void showEvent(QShowEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolBars();
    void buildDock();
    void buildProjectDock();
    void buildStatusBar();
    void refreshIcons();

    // Меню «все элементы по категориям» — одно и то же в меню, на панели и по ПКМ.
    void fillElementMenu(QMenu *menu);

    CircuitView *currentView() const;
    void attachView(CircuitView *view);
    void onTabChanged(int index);

    // вкладки
    void closeTab(int index);                          // с подтверждением
    void closeTabsWhere(const QVector<QWidget *> &pages);
    void addPageTab(QWidget *page, int at = -1);        // вкладка + кнопка «×»; имя и проект — в свойствах страницы
    // Перенос вкладок между окнами (см. DetachableTabBar).
    QWidget *takeTab(int index);                                // вынуть вкладку, не удаляя страницу
    void adoptTab(QWidget *page, int at = -1, bool reveal = true);
    void detachTab(int index, const QPoint &globalPos);         // вкладка уходит в новое окно
    void adoptMap(QWidget *page, int at = -1, bool reveal = true);
    void detachMap(int index, const QPoint &globalPos);         // то же для карты Карно
    void onMapDropped(DetachableTabBar *source, int sourceIndex, int targetIndex);
    void checkEmptyAfterMove();                                 // у окна унесли последнюю страницу: закрыть его
    void afterPagesChanged();                                   // страницы убавились: пустое окно закрывается, раздел переключается
    void onTabDropped(DetachableTabBar *source, int sourceIndex, int targetIndex);
    void removeTab(int index);                         // без вопросов
    void renameTab(int index);
    void duplicateTab(int index);
    void clearTab(int index);
    void showTabMenu(int index, const QPoint &globalPos);

    // проекты
    ProjectDoc collectProject(int projectId) const;
    int createProjectFromDoc(const ProjectDoc &doc, const QString &path);
    bool projectDirty(int projectId) const;
    void refreshProjectPanel();
    void refreshTabLabels();
    void renameProjectDialog(int projectId);
    void deleteProject(int projectId);
    void openProjectInExplorer(int projectId);
    static void setProjectHidden(int projectId, bool hidden);
    void activateProject(int projectId);
    void addSchemeTo(int projectId);
    void addMapTo(int projectId);
    static EditorWindow *ownerOf(QWidget *page);
    static bool activatePage(QWidget *page);
    static void renamePage(QWidget *page);
    static void closePage(QWidget *page);
    static void applyProjectVisibility(int projectId);
    CircuitView *addScheme(int projectId, const QString &name = QString());

    void syncToolActions();
    void syncEditActions();
    void updateCount();
    void showContextMenu(const QPoint &globalPos);
    void openElementDialog(CircuitItem *item);
    void showSettings();
    void showAbout();
    void showControls();
    void showNotYet();
    QString placeHint(ElementType type) const;

    EditorTabs *m_tabs = nullptr;
    QUndoGroup *m_undoGroup = nullptr;
    QPointer<CircuitView> m_view;
    PropertiesPanel *m_panel = nullptr;
    SimulationPanel *m_sim = nullptr;
    QDockWidget *m_simDock = nullptr;        // панели можно двигать и закреплять по краям окна
    QDockWidget *m_propDock = nullptr;
    QDockWidget *m_scopeDock = nullptr;      // нижняя панель осциллограмм (вкладка на каждый элемент)
    ScopeBoard *m_scopeBoard = nullptr;
    IssueLog *m_log = nullptr;                // журнал симуляции
    QDockWidget *m_logDock = nullptr;
    bool m_logDockWas = false;
    QVector<SimIssue> m_issues;               // текущие проблемы после остановки (пусто — всё в порядке)
    bool m_scopeDockWas = false;
    bool m_simDockWas = true;                // были ли панели видны до перехода в раздел карт Карно
    bool m_propDockWas = true;
    bool m_standalone = false;
    bool m_docksSized = false;               // начальные размеры правых панелей уже заданы
    int m_section = 0;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_elementsMenu = nullptr;
    QStackedWidget *m_stack = nullptr;       // разделы: вкладки схем / вкладки карт Карно
    KarnaughSection *m_karnaugh = nullptr;
    ProjectPanel *m_projectPanel = nullptr;  // только в основном окне
    QDockWidget *m_projectDock = nullptr;
    QDockWidget *m_navDock = nullptr;
    QActionGroup *m_navGroup = nullptr;      // переключатель разделов на левой панели
    QAction *m_navSimAction = nullptr;
    QAction *m_navKarnaughAction = nullptr;

    QActionGroup *m_toolGroup = nullptr;
    QAction *m_selectAction = nullptr;
    QAction *m_routedWireAction = nullptr;
    QVector<QAction *> m_elementActions;     // индекс — ElementType

    QAction *m_escAction = nullptr;
    QAction *m_newTabAction = nullptr;
    QAction *m_karnaughAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_newProjectAction = nullptr;
    QAction *m_renameProjectAction = nullptr;
    QAction *m_closeProjectAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_menuAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_controlsAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_selectAllAction = nullptr;
    QAction *m_propsAction = nullptr;
    QAction *m_rotateCwAction = nullptr;
    QAction *m_rotateCcwAction = nullptr;
    QAction *m_flipAction = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_resetViewAction = nullptr;

    QLabel *m_hintLabel = nullptr;
    QLabel *m_posLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_countLabel = nullptr;

    QVector<QMetaObject::Connection> m_viewConnections;
    bool m_closeAfterDrag = false;           // унесли последнюю вкладку: окно закроется, когда закончится перенос
};
