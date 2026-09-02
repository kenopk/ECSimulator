#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>      // Базовый класс главного окна, даёт тебе готовое окно с меню, статусной строкой и центральной областью
#include <QLabel>           // Текстовая надпись (логотип, подзаголовок)
#include <QResizeEvent>     // событие, которое возникает при изменении размера окна (нужно для переопределения)
#include <QImage>           // Работа с изображениями
#include <QPushButton>      // Работа с кнопками
#include <QStackedWidget>   // контейнер, который показывает только один виджет за раз (переключение между главным меню и редактором)
#include "editorwindow.h"
// ============================================================================
// ОСНОВНОЙ КЛАСС ГЛАВНОГО ОКНА
// ============================================================================
class MainWindow : public QMainWindow { // Наследование от QMainWinow
    Q_OBJECT  // Макрос для работы сигналов/слотов Qt

public:
    explicit MainWindow(QWidget *parent = nullptr); // explicit – запрещает неявное преобразование.
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override; // Переопределение события изменения размера окна

private:
    void setupUI();     // создаёт виджеты (кнопки, надписи, QStackedWidget), добавляет их в layout, настраивает подключение сигналов.
    void setupStyles(); //  задаёт цвета, градиенты, скругления через CSS-подобный синтаксис Qt.

    QLabel *titleLabel = nullptr;       // Указатель на заголовок приложения
    QPushButton *btnCreate = nullptr;   // Кнопка "Создать"
    QPushButton *btnOpen = nullptr;     // Кнопка "Открыть"
    QPushButton *btnSettings = nullptr; // Кнопка "Настройки"

    QStackedWidget *stackedWidget = nullptr;    // Контейнер, который переключает два состояния: главный экран (startScreen) и редактор (editorScreen).Позволяет реализовать переходы без создания отдельных окон.
    QWidget *startScreen = nullptr;             // Виджет, содержащий меню (кнопки, логотип).
    EditorWindow *editorScreen = nullptr;       // Твой кастомный класс редактора

// ========================================================================
// DEBUG: Указатель на оверлей отладки (красные линии центра) в CMake add_compile_definitions(DEBUG)
// ========================================================================
#ifdef DEBUG
    QWidget *m_debugOverlay = nullptr;  // Виджет рисует красные линии по центру, показывает координаты курсора
#endif
};

#endif // MAINWINDOW_H