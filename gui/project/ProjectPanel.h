#pragma once

#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

// Панель «Проект»: дерево всех открытых проектов. У каждого проекта две группы — «Схемы» и «Карты Карно».
// Двойной клик по странице открывает её; двойной клик по пустой группе создаёт в ней первую страницу;
// ПКМ по проекту — открыть в проводнике, скрыть, сохранить, переименовать, закрыть, удалить.
// Саму работу выполняет окно редактора (сигналы ниже).
class ProjectPanel : public QWidget
{
    Q_OBJECT
public:
    struct Entry {
        QString name;
        QPointer<QWidget> page;
        QString note;               // например, «в отдельном окне»
    };

    struct ProjectEntry {
        int id = 0;
        QString name;
        QString path;
        bool hidden = false;
        bool active = false;
        bool dirty = false;
        QVector<Entry> schemes;
        QVector<Entry> maps;
    };

    explicit ProjectPanel(QWidget *parent = nullptr);

    void setContent(const QVector<ProjectEntry> &projects);
    void setCurrentPage(QWidget *page);          // подсветить открытую страницу

signals:
    void pageActivated(QWidget *page);
    void pageRenameRequested(QWidget *page);
    void pageCloseRequested(QWidget *page);
    void newSchemeRequested(int projectId);
    void newMapRequested(int projectId);
    void projectActivated(int projectId);
    void projectRenameRequested(int projectId);
    void projectSaveRequested(int projectId);
    void projectSaveAsRequested(int projectId);
    void projectExplorerRequested(int projectId);
    void projectHideToggled(int projectId);
    void projectCloseRequested(int projectId);
    void projectDeleteRequested(int projectId);
    void newProjectRequested();
    void openProjectRequested();

private:
    enum Role { KindRole = Qt::UserRole, ProjectRole = Qt::UserRole + 1 };
    enum Kind { ProjectItem = 0, SchemeGroup = 1, MapGroup = 2, PageItem = 3 };

    void showMenu(const QPoint &pos);
    QWidget *pageOf(QTreeWidgetItem *item) const;
    static QString keyOf(QTreeWidgetItem *item);

    QTreeWidget *m_tree = nullptr;
    QVector<QPair<QTreeWidgetItem *, QPointer<QWidget>>> m_pages;
    QSet<QString> m_collapsed;                   // свёрнутые узлы переживают перестроение дерева
    bool m_building = false;
};
