#pragma once

#include <QMainWindow>

class EditorWindow;
class QStackedWidget;

// Главное окно: стартовый экран + редактор (переключаются в QStackedWidget).
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *buildStartScreen();
    void showEditor(bool newDocument);
    void showStart();

    QStackedWidget *m_stack = nullptr;
    QWidget *m_start = nullptr;
    EditorWindow *m_editor = nullptr;
};
