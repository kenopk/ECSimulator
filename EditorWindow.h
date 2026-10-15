#ifndef EDITORWINDOW_H
#define EDITORWINDOW_H

#include <QWidget>
#include <QTabWidget>
#include <QMenu>

QString buildAppStyleSheet(const QColor &bg);
// Предварительное объявление кастомного класса
class InfiniteGraphicsView;

class EditorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWindow(QWidget *parent = nullptr);
    void applyTheme(const QColor &bg);
    void applyCursorTheme(Qt::CursorShape shape);

public slots:
    // === ПУБЛИЧНЫЕ СЛОТЫ ===
    // Эти методы могут вызываться из других классов (например, из MainWindow)
    void onNewProject();
    void onOpenProject();

    // Слоты для зума и сброса вида (тоже могут вызываться извне, если нужно)
    void onZoomIn();
    void onZoomOut();
    void onFitToSize();
    void onResetView();

private slots:
    // === ПРИВАТНЫЕ СЛОТЫ ===
    // Эти методы используются только внутри самого EditorWindow
    void closeTab(int index);
    void renameTab(int index);
    void showTabContextMenu(int index, const QPoint &globalPos);
    void onUndo();
    void onRedo();
    void updateEditActionsState();   // обновляет checked/enabled всех действий редактирования

signals:
    void backToMainMenu();

private:
    // Внутренние методы настройки и логики
    void setupUI();
    void createNewTab(const QString &title);

    // Создаёт ВСЕ QAction'ы редактирования (Wire/Resistor/Cap/Select/Delete/
    // Copy/Cut/Paste/Undo/Redo) один раз. Дальше эти же объекты добавляются
    // и в постоянное меню "Edit"/"Правка" в menu bar, и во временное
    // контекстное меню по ПКМ (contextMenuEvent) — Qt позволяет один QAction
    // показывать сразу в нескольких QMenu, поэтому не нужно строить два
    // параллельных набора пунктов меню с одинаковой логикой.
    void createEditActions();

    // Наполняет переданное меню общими действиями редактирования
    // (используется и для постоянного, и для временного меню).
    void populateEditMenu(QMenu *menu);

    QString generateUniqueTabName(const QString &baseName);

    // Вспомогательные функции
    InfiniteGraphicsView* getCurrentView() const;

    // Переопределение событий
    bool eventFilter(QObject *obj, QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    // Указатель на виджет вкладок
    QTabWidget *tabWidget = nullptr;

    // Общие действия редактирования — см. createEditActions()/populateEditMenu().
    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;
    QAction *m_wireAction = nullptr;
    QAction *m_resistorAction = nullptr;
    QAction *m_capAction = nullptr;
    QAction *m_selectAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_pasteAction = nullptr;

    void createSettingsTab();
    QColor m_backgroundColor = QColor(30, 30, 46);
    Qt::CursorShape m_cursorShape = Qt::ArrowCursor;

    QAction *m_polarCapAction = nullptr;
};

#endif // EDITORWINDOW_H