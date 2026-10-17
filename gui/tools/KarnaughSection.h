#pragma once

#include <QList>
#include <QWidget>

class QStackedLayout;
class QTabBar;
class DetachableTabBar;
class KarnaughTabs;
class KarnaughWidget;

// Раздел «Карты Карно»: свои вкладки (как у схем) с калькуляторами KarnaughWidget. У каждой карты есть проект
// и имя (см. ProjectHub), вкладки можно переименовывать (двойной клик, ПКМ), закрывать (кнопка «×», СКМ, ПКМ),
// переносить между окнами и отрывать в новое окно (DetachableTabBar вида 1).
// Если закрыли все карты, показывается кнопка «Новая карта Карно».
class KarnaughSection : public QWidget
{
    Q_OBJECT
public:
    explicit KarnaughSection(QWidget *parent = nullptr);

    int tabCount() const;
    QWidget *page(int index) const;
    QString pageName(int index) const;
    QWidget *currentPage() const;
    DetachableTabBar *bar() const;

    KarnaughWidget *addMap(int projectId, const QString &name = QString());   // новая карта в проекте, становится текущей
    void newTab();                                        // карта в активном проекте
    void closeCurrent();
    void closeAll();
    bool contains(QWidget *page) const;
    bool select(QWidget *page);
    void renamePage(QWidget *page);                       // спрашивает новое имя
    void closePage(QWidget *page);
    QWidget *takePage(int index);                         // вынуть вкладку, не удаляя карту (её заберёт другое окно)
    void adoptPage(QWidget *page, int at = -1);
    void setPageVisible(QWidget *page, bool visible);     // скрытые проекты убирают свои вкладки с полосы
    void refreshLabels();                                 // подписи и подсказки вкладок по реестру проектов

signals:
    void becameEmpty();                                   // закрыта последняя карта

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void addPageTab(QWidget *page, int at);
    void closeTab(int index);
    void closeWhere(const QList<QWidget *> &pages);
    void renameTab(int index);
    void showTabMenu(int index, const QPoint &globalPos);
    void syncEmptyState();

    KarnaughTabs *m_tabs = nullptr;
    QStackedLayout *m_layout = nullptr;
};
