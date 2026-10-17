#include "project/ProjectPanel.h"

#include <QFont>
#include <QHeaderView>
#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "theme/Theme.h"

ProjectPanel::ProjectPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("projectPanel"));
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("projectTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setFrameShape(QFrame::NoFrame);
    // Выделение — мягкая подложка акцентного цвета на всю строку, включая отступ (стиль задаём у самого дерева).
    auto applyStyle = [this] {
        const Palette &pal = Theme::instance().palette();
        const QString soft = QStringLiteral("rgba(%1,%2,%3,70)").arg(pal.accent.red()).arg(pal.accent.green()).arg(pal.accent.blue());
        m_tree->setStyleSheet(QStringLiteral(
            "QTreeWidget { background: %1; border: none; outline: 0; padding: 4px 2px; show-decoration-selected: 1;"
            " selection-background-color: transparent; selection-color: %2; }"
            "QTreeWidget::item { padding: 3px 2px; }"
            "QTreeWidget::item:hover { background: %3; }"
            "QTreeWidget::item:selected { background: %4; color: %2; }")
            .arg(pal.panel.name(), pal.text.name(), pal.panelAlt.name(), soft));
    };
    applyStyle();
    connect(&Theme::instance(), &Theme::changed, this, applyStyle);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        const int kind = item->data(0, KindRole).toInt();
        const int project = item->data(0, ProjectRole).toInt();
        if (kind == PageItem) {
            if (QWidget *page = pageOf(item))
                emit pageActivated(page);
        } else if (kind == ProjectItem) {
            emit projectActivated(project);
        } else if (item->childCount() == 0) {                  // пустая группа: первый двойной клик создаёт страницу
            if (kind == SchemeGroup)
                emit newSchemeRequested(project);
            else
                emit newMapRequested(project);
        }
    });
    connect(m_tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) {
        if (!m_building)
            m_collapsed.insert(keyOf(item));
    });
    connect(m_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
        if (!m_building)
            m_collapsed.remove(keyOf(item));
    });
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ProjectPanel::showMenu);
}

QString ProjectPanel::keyOf(QTreeWidgetItem *item)
{
    return QStringLiteral("%1/%2").arg(item->data(0, ProjectRole).toInt()).arg(item->data(0, KindRole).toInt());
}

QWidget *ProjectPanel::pageOf(QTreeWidgetItem *item) const
{
    for (const auto &pair : m_pages)
        if (pair.first == item)
            return pair.second.data();
    return nullptr;
}

void ProjectPanel::setContent(const QVector<ProjectEntry> &projects)
{
    m_building = true;
    m_tree->clear();
    m_pages.clear();
    const Palette &pal = Theme::instance().palette();

    for (const ProjectEntry &pr : projects) {
        QString title = pr.name;
        if (pr.dirty)
            title += QStringLiteral("  •");
        if (pr.hidden)
            title += QStringLiteral("  (скрыт)");
        auto *root = new QTreeWidgetItem(m_tree, { title });
        root->setData(0, KindRole, ProjectItem);
        root->setData(0, ProjectRole, pr.id);
        root->setData(0, Qt::UserRole + 2, pr.hidden);
        QFont f = root->font(0);
        f.setBold(pr.active);
        root->setFont(0, f);
        if (pr.hidden)
            root->setForeground(0, pal.textDim);
        root->setToolTip(0, pr.path.isEmpty() ? QStringLiteral("Проект ещё не сохранён") : pr.path);

        auto addGroup = [&](const QString &caption, const QVector<Entry> &entries, Kind kind) {
            auto *group = new QTreeWidgetItem(root, { QStringLiteral("%1 (%2)").arg(caption).arg(entries.size()) });
            group->setData(0, KindRole, kind);
            group->setData(0, ProjectRole, pr.id);
            group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
            if (pr.hidden)
                group->setForeground(0, pal.textDim);
            for (const Entry &e : entries) {
                auto *item = new QTreeWidgetItem(group, { e.note.isEmpty() ? e.name : e.name + QStringLiteral("  ·  ") + e.note });
                item->setData(0, KindRole, PageItem);
                item->setData(0, ProjectRole, pr.id);
                item->setToolTip(0, e.name);
                if (pr.hidden)
                    item->setForeground(0, pal.textDim);
                m_pages.append({ item, e.page });
            }
            group->setExpanded(!m_collapsed.contains(keyOf(group)));
        };
        addGroup(QStringLiteral("Схемы"), pr.schemes, SchemeGroup);
        addGroup(QStringLiteral("Карты Карно"), pr.maps, MapGroup);
        root->setExpanded(!m_collapsed.contains(keyOf(root)));
    }
    m_building = false;
}

void ProjectPanel::setCurrentPage(QWidget *page)
{
    for (const auto &pair : m_pages)
        if (pair.second.data() == page) {
            m_tree->setCurrentItem(pair.first);
            return;
        }
}

void ProjectPanel::showMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    const int kind = item ? item->data(0, KindRole).toInt() : -1;
    const int project = item ? item->data(0, ProjectRole).toInt() : 0;
    QWidget *page = item && kind == PageItem ? pageOf(item) : nullptr;

    QMenu menu(this);
    QAction *aOpen = nullptr, *aRename = nullptr, *aClose = nullptr;
    QAction *aExplorer = nullptr, *aHide = nullptr, *aSave = nullptr, *aSaveAs = nullptr;
    QAction *aProjRename = nullptr, *aProjClose = nullptr, *aDelete = nullptr;
    QAction *aNewScheme = nullptr, *aNewMap = nullptr;

    if (page) {
        aOpen = menu.addAction(QStringLiteral("Открыть"));
        aRename = menu.addAction(QStringLiteral("Переименовать…"));
        aClose = menu.addAction(QStringLiteral("Закрыть"));
        menu.addSeparator();
    }
    if (project) {
        if (kind == ProjectItem) {
            aExplorer = menu.addAction(QStringLiteral("Открыть в проводнике"));
            aHide = menu.addAction(item->data(0, Qt::UserRole + 2).toBool() ? QStringLiteral("Показать")
                                                                             : QStringLiteral("Скрыть"));
            menu.addSeparator();
            aSave = menu.addAction(QStringLiteral("Сохранить"));
            aSaveAs = menu.addAction(QStringLiteral("Сохранить как…"));
            aProjRename = menu.addAction(QStringLiteral("Переименовать…"));
            menu.addSeparator();
        }
        aNewScheme = menu.addAction(QStringLiteral("Новая схема"));
        aNewMap = menu.addAction(QStringLiteral("Новая карта Карно"));
        if (kind == ProjectItem) {
            menu.addSeparator();
            aProjClose = menu.addAction(QStringLiteral("Закрыть проект"));
            aDelete = menu.addAction(QStringLiteral("Удалить проект…"));
        }
        menu.addSeparator();
    }
    QAction *aNewProject = menu.addAction(QStringLiteral("Новый проект"));
    QAction *aOpenProject = menu.addAction(QStringLiteral("Открыть проект…"));

    QPointer<QWidget> guard = page;
    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == aNewProject)
        emit newProjectRequested();
    else if (chosen == aOpenProject)
        emit openProjectRequested();
    else if (chosen == aNewScheme)
        emit newSchemeRequested(project);
    else if (chosen == aNewMap)
        emit newMapRequested(project);
    else if (chosen == aExplorer)
        emit projectExplorerRequested(project);
    else if (chosen == aHide)
        emit projectHideToggled(project);
    else if (chosen == aSave)
        emit projectSaveRequested(project);
    else if (chosen == aSaveAs)
        emit projectSaveAsRequested(project);
    else if (chosen == aProjRename)
        emit projectRenameRequested(project);
    else if (chosen == aProjClose)
        emit projectCloseRequested(project);
    else if (chosen == aDelete)
        emit projectDeleteRequested(project);
    else if (guard && chosen == aOpen)
        emit pageActivated(guard);
    else if (guard && chosen == aRename)
        emit pageRenameRequested(guard);
    else if (guard && chosen == aClose)
        emit pageCloseRequested(guard);
}
