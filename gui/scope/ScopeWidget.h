#pragma once

#include <QPointer>
#include <QSplitter>
#include <QWidget>

#include "scope/ScopeSource.h"

class QTimer;

// Одна осциллограмма в нижней панели или в отдельном окне: график, значок «шестерёнка» (параметры),
// курсор со значениями, меню правой кнопки. Свойства берутся из ScopeSource, у каждого элемента свои.
class ScopeView : public QWidget
{
    Q_OBJECT
public:
    explicit ScopeView(ScopeSource *source, QWidget *parent = nullptr);

    ScopeSource *source() const { return m_source; }

    // Отдельное окно: в меню вместо «Вынести в окно» — «Вернуть в нижнюю панель», без расположения.
    void setDetached(bool detached) { m_detached = detached; }
    // Как сейчас расположены осциллограммы панели (чтобы выключить этот пункт в меню).
    void setArrangement(Qt::Orientation o) { m_arrangement = o; }

signals:
    void removeRequested();                      // «Убрать осциллограф»
    void moveToSchemeRequested();                // «Переместить осциллограф в схему»
    void windowRequested();                      // вынести в окно / вернуть в панель
    void arrangeRequested(Qt::Orientation o);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private:
    void showProperties();

    QPointer<ScopeSource> m_source;
    QTimer *m_timer = nullptr;
    bool m_dirty = true;
    bool m_detached = false;
    Qt::Orientation m_arrangement = Qt::Horizontal;
    double m_hover = -1;
    bool m_gearHover = false;
};

// Нижняя панель осциллограмм: несколько осциллограмм рядом или друг над другом (границы можно двигать).
class ScopeBoard : public QWidget
{
    Q_OBJECT
public:
    explicit ScopeBoard(QWidget *parent = nullptr);

    ScopeView *addScope(ScopeSource *source);        // уже показанную не дублирует
    bool contains(ScopeSource *source) const;
    void removeScope(ScopeSource *source);
    int count() const;
    Qt::Orientation arrangement() const { return m_split->orientation(); }
    void arrange(Qt::Orientation o);

signals:
    void emptied();                                   // убрали последнюю осциллограмму
    void windowRequested(ScopeSource *source);        // вынести в отдельное окно
    void moveToSchemeRequested(ScopeSource *source);  // перенести на схему

private:
    ScopeView *viewFor(ScopeSource *source) const;
    void remove(ScopeView *view);

    QSplitter *m_split = nullptr;
};

// Осциллограмма в отдельном окне приложения (как оторванная вкладка схемы).
class ScopeWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ScopeWindow(ScopeSource *source, QWidget *parent = nullptr);

    ScopeSource *source() const { return m_view->source(); }

signals:
    void dockRequested(ScopeSource *source);          // «Вернуть в нижнюю панель»
    void moveToSchemeRequested(ScopeSource *source);

private:
    ScopeView *m_view = nullptr;
};
