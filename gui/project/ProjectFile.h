#pragma once

#include <QObject>
#include <QPointer>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

#include "canvas/CircuitView.h"

// Проект — это набор схем и карт Карно под общим именем. Хранится одним файлом .govno (JSON):
// схемы — как слепки элементов (тип по стабильному id из реестра, геометрия, параметры, клетки карты Карно у
// элемента «Карта Карно»), карты Карно — размер, клетки и имена переменных.
// Осциллограммы и результаты расчёта в файл не попадают: их порождает симуляция.
struct SchemeDoc {
    QString name;
    QVector<ItemData> items;
};

struct MapDoc {
    QString name;
    int width = 4;
    int height = 4;
    QVector<quint8> cells;       // height × width по строкам: 0, 1, 2 (X)
    QString names;               // строка из поля «Имена переменных»
    bool namesEdited = false;    // имена правил вручную (иначе — A, B, …)
};

struct ProjectDoc {
    QString name;
    QVector<SchemeDoc> schemes;
    QVector<MapDoc> maps;
};

namespace ProjectFile {

inline const QString kExtension = QStringLiteral("govno");

bool save(const QString &path, const ProjectDoc &doc, QString *error);
// Неизвестные типы элементов пропускаются; их число попадает в skipped (может быть nullptr).
bool load(const QString &path, ProjectDoc *doc, QString *error, int *skipped = nullptr);

} // namespace ProjectFile

// Проект в списке открытых: имя, файл (пусто — ещё не сохранён) и признак «скрыт» (вкладки проекта убраны с полос).
struct ProjectInfo {
    int id = 0;
    QString name;
    QString path;
    bool hidden = false;
};

// Реестр проектов, общий для всех окон. В приложении одновременно открыто несколько проектов; у каждой страницы
// (схема, карта Карно) в свойствах записан проект, вид и имя. Имена страниц уникальны внутри проекта и вида.
// Любое изменение состава вкладок сообщается через notify() (сигнал changed() приходит один раз за цикл событий) —
// панель «Проект», подписи вкладок и заголовок окна обновляются сами.
class ProjectHub : public QObject
{
    Q_OBJECT
public:
    enum PageKind { Scheme = 0, Map = 1 };

    static ProjectHub &instance();

    // --- проекты -------------------------------------------------------------------------------------------
    const QVector<ProjectInfo> &projects() const { return m_projects; }
    const ProjectInfo *find(int id) const;
    int findByPath(const QString &path) const;
    int create(const QString &name, const QString &path = QString());      // новый проект становится активным
    void remove(int id);
    int activeId() const { return m_active; }
    void setActive(int id);
    int ensureActive();                                   // активный проект; если проектов нет — создаёт «Новый проект»
    void setName(int id, const QString &name);
    void setPath(int id, const QString &path);
    void setHidden(int id, bool hidden);
    QString uniqueProjectName(const QString &base) const;

    // --- страницы ------------------------------------------------------------------------------------------
    static void attach(QWidget *page, int projectId, PageKind kind, const QString &name);
    static int projectOf(const QWidget *page);
    static PageKind kindOf(const QWidget *page);
    static QString nameOf(const QWidget *page);
    static void setNameOf(QWidget *page, const QString &name);
    QString tabLabel(const QWidget *page) const;          // имя (и название проекта, если проектов несколько)
    bool nameTaken(int projectId, PageKind kind, const QString &name, const QWidget *except = nullptr) const;
    QString uniqueName(int projectId, PageKind kind, const QString &base, const QWidget *except = nullptr) const;
    QString nextName(int projectId, PageKind kind) const;  // «Схема N» / «Карта N»
    QVector<QWidget *> pagesOf(int projectId) const;

    void notify();                                        // вкладки изменились (добавлены, закрыты, переименованы, переставлены)

signals:
    void changed();

private:
    ProjectHub() = default;
    QVector<QWidget *> livePages() const;

    QVector<ProjectInfo> m_projects;
    int m_active = 0;
    int m_nextId = 0;
    mutable QVector<QPointer<QWidget>> m_pages;
    bool m_pending = false;
};
