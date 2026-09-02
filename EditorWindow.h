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
    void onOpenProject();       // <-- Теперь этот слот публичный, ошибка исчезнет

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
    void updateUndoRedoActions();

signals:
    void backToMainMenu();

private:
    // Внутренние методы настройки и логики
    void setupUI();
    void createNewTab(const QString &title);
    void Draw_Edit_Menu(QMenu *menu);
    QString generateUniqueTabName(const QString &baseName);

    // Вспомогательные функции
    InfiniteGraphicsView* getCurrentView() const;

    // Переопределение событий
    bool eventFilter(QObject *obj, QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    // Указатель на виджет вкладок
    //QTabWidget *tabWidget;
    QTabWidget *tabWidget = nullptr;  // ← добавили = nullptr

    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;

    void createSettingsTab();
    QColor m_backgroundColor = QColor(30, 30, 46);
    Qt::CursorShape m_cursorShape = Qt::ArrowCursor;   // было OpenHandCursor;
};

#endif // EDITORWINDOW_H