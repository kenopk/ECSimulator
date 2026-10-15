#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QTabWidget>
#include <QVector>

#include "util/CoreTypes.h"

class CircuitItem;
class CircuitView;
class PropertiesPanel;
class SimulationPanel;
class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QMenu;
class QTabBar;
class QToolButton;
class QUndoGroup;

// QTabWidget с открытым доступом к полосе вкладок (в Qt tabBar() защищён).
class EditorTabs : public QTabWidget
{
public:
    using QTabWidget::QTabWidget;
    QTabBar *bar() const { return tabBar(); }
};

// Окно редактора: вкладки со схемами, меню элементов, правая панель
// (симуляция + свойства), статус-строка. Каждая вкладка — отдельный
// CircuitView со своим стеком отмены.
class EditorWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EditorWindow(QWidget *parent = nullptr);
    ~EditorWindow() override;

    void newTab();
    int tabCount() const { return m_tabs ? m_tabs->count() : 0; }

    // Панель управления симуляцией: сюда подключится ядро
    // (runningChanged, simulationSpeedChanged, resetClicked ...).
    SimulationPanel *simulationPanel() const { return m_sim; }

signals:
    void backToMainMenu();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolBars();
    void buildDock();
    void buildStatusBar();
    void refreshIcons();

    // Меню «все элементы по категориям» — одно и то же в меню, на панели и по ПКМ.
    void fillElementMenu(QMenu *menu);

    CircuitView *currentView() const;
    void attachView(CircuitView *view);
    void onTabChanged(int index);

    // вкладки
    QString uniqueTabName(const QString &base);
    QString nextTabName();
    void closeTab(int index);                          // с подтверждением
    void closeTabsWhere(const QVector<CircuitView *> &views);
    void removeTab(int index);                         // без вопросов
    void renameTab(int index);
    void duplicateTab(int index);
    void clearTab(int index);
    void showTabMenu(int index, const QPoint &globalPos);

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
    QDockWidget *m_dock = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_elementsMenu = nullptr;
    QToolButton *m_allButton = nullptr;      // «Все элементы» на левой панели

    QActionGroup *m_toolGroup = nullptr;
    QAction *m_selectAction = nullptr;
    QAction *m_routedWireAction = nullptr;
    QVector<QAction *> m_elementActions;     // индекс — ElementType

    QAction *m_escAction = nullptr;
    QAction *m_newTabAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
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
    int m_tabCounter = 0;
};
