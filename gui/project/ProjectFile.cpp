#include "project/ProjectFile.h"

#include <QCoreApplication>
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

#include "util/CoreTypes.h"

namespace {

constexpr int kFormatVersion = 1;
const char kFormatName[] = "govno-project";

// id из реестра -> тип (для чтения файла).
const QHash<QString, ElementType> &typeById()
{
    static const QHash<QString, ElementType> map = [] {
        QHash<QString, ElementType> m;
        for (const ElementInfo &info : allElements())
            m.insert(QString::fromUtf8(info.id), info.type);
        return m;
    }();
    return map;
}

QJsonArray pointToJson(const QPoint &p)
{
    return QJsonArray{ p.x(), p.y() };
}

QPoint pointFromJson(const QJsonValue &v)
{
    const QJsonArray a = v.toArray();
    return QPoint(a.at(0).toInt(), a.at(1).toInt());
}

QJsonArray cellsToJson(const QVector<quint8> &cells)
{
    QJsonArray a;
    for (quint8 c : cells)
        a.append(int(c));
    return a;
}

QVector<quint8> cellsFromJson(const QJsonValue &v)
{
    const QJsonArray a = v.toArray();
    QVector<quint8> out;
    out.reserve(a.size());
    for (const QJsonValue &x : a)
        out.append(quint8(std::clamp(x.toInt(), 0, 2)));
    return out;
}

QJsonObject itemToJson(const ItemData &d)
{
    QJsonObject o;
    o.insert(QStringLiteral("type"), QString::fromUtf8(elementInfo(d.type).id));
    o.insert(QStringLiteral("a"), pointToJson(d.a));
    o.insert(QStringLiteral("b"), pointToJson(d.b));
    if (d.flip)
        o.insert(QStringLiteral("flip"), true);
    QJsonObject params;
    for (auto it = d.params.constBegin(); it != d.params.constEnd(); ++it)
        params.insert(it.key(), it.value());
    if (!params.isEmpty())
        o.insert(QStringLiteral("params"), params);
    if (d.type == ElementType::TextLabel)
        o.insert(QStringLiteral("text"), d.text);
    if (!d.cells.isEmpty())
        o.insert(QStringLiteral("cells"), cellsToJson(d.cells));
    if (!d.link.isEmpty())
        o.insert(QStringLiteral("link"), d.link);
    if (d.type == ElementType::Wire && d.note != WireNote()) {
        QJsonObject n;
        n.insert(QStringLiteral("text"), d.note.text);
        if (d.note.shown)
            n.insert(QStringLiteral("shown"), true);
        n.insert(QStringLiteral("dx"), d.note.offset.x());
        n.insert(QStringLiteral("dy"), d.note.offset.y());
        if (d.note.autoText)
            n.insert(QStringLiteral("auto"), true);
        o.insert(QStringLiteral("note"), n);
    }
    return o;
}

} // namespace

namespace ProjectFile {

bool save(const QString &path, const ProjectDoc &doc, QString *error)
{
    QJsonArray schemes;
    for (const SchemeDoc &s : doc.schemes) {
        QJsonArray items;
        for (const ItemData &d : s.items)
            items.append(itemToJson(d));
        QJsonObject o;
        o.insert(QStringLiteral("name"), s.name);
        if (s.mmPerCell != 1.0)
            o.insert(QStringLiteral("mmPerCell"), s.mmPerCell);
        o.insert(QStringLiteral("items"), items);
        schemes.append(o);
    }
    QJsonArray maps;
    for (const MapDoc &m : doc.maps) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), m.name);
        o.insert(QStringLiteral("width"), m.width);
        o.insert(QStringLiteral("height"), m.height);
        o.insert(QStringLiteral("cells"), cellsToJson(m.cells));
        if (m.namesEdited)
            o.insert(QStringLiteral("names"), m.names);
        maps.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(kFormatName));
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("name"), doc.name);
    root.insert(QStringLiteral("schemes"), schemes);
    root.insert(QStringLiteral("maps"), maps);

    QSaveFile file(path);                              // запись во временный файл: старый не пропадёт при сбое
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool load(const QString &path, ProjectDoc *doc, QString *error, int *skipped)
{
    if (skipped)
        *skipped = 0;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (json.isNull() || !json.isObject()) {
        if (error)
            *error = QStringLiteral("файл повреждён (%1)").arg(parseError.errorString());
        return false;
    }
    const QJsonObject root = json.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormatName)) {
        if (error)
            *error = QStringLiteral("это не файл проекта GOVNO");
        return false;
    }
    if (root.value(QStringLiteral("version")).toInt() > kFormatVersion) {
        if (error)
            *error = QStringLiteral("файл создан более новой версией программы");
        return false;
    }

    ProjectDoc out;
    out.name = root.value(QStringLiteral("name")).toString();
    for (const QJsonValue &sv : root.value(QStringLiteral("schemes")).toArray()) {
        const QJsonObject so = sv.toObject();
        SchemeDoc s;
        s.name = so.value(QStringLiteral("name")).toString();
        s.mmPerCell = std::max(1e-6, so.value(QStringLiteral("mmPerCell")).toDouble(1.0));
        for (const QJsonValue &iv : so.value(QStringLiteral("items")).toArray()) {
            const QJsonObject io = iv.toObject();
            const auto type = typeById().constFind(io.value(QStringLiteral("type")).toString());
            if (type == typeById().constEnd()) {
                if (skipped)
                    ++*skipped;
                continue;
            }
            ItemData d;
            d.type = type.value();
            d.a = pointFromJson(io.value(QStringLiteral("a")));
            d.b = pointFromJson(io.value(QStringLiteral("b")));
            d.flip = io.value(QStringLiteral("flip")).toBool();
            const QJsonObject params = io.value(QStringLiteral("params")).toObject();
            for (auto it = params.constBegin(); it != params.constEnd(); ++it)
                d.params.insert(it.key(), it.value().toDouble());
            d.text = io.value(QStringLiteral("text")).toString();
            d.cells = cellsFromJson(io.value(QStringLiteral("cells")));
            d.link = io.value(QStringLiteral("link")).toString();
            if (io.contains(QStringLiteral("note"))) {
                const QJsonObject n = io.value(QStringLiteral("note")).toObject();
                d.note.text = n.value(QStringLiteral("text")).toString();
                d.note.shown = n.value(QStringLiteral("shown")).toBool();
                d.note.offset = QPoint(n.value(QStringLiteral("dx")).toInt(32), n.value(QStringLiteral("dy")).toInt(-64));
                d.note.autoText = n.value(QStringLiteral("auto")).toBool();
            }
            s.items.append(d);
        }
        out.schemes.append(s);
    }
    for (const QJsonValue &mv : root.value(QStringLiteral("maps")).toArray()) {
        const QJsonObject mo = mv.toObject();
        MapDoc m;
        m.name = mo.value(QStringLiteral("name")).toString();
        m.width = std::max(1, mo.value(QStringLiteral("width")).toInt(4));
        m.height = std::max(1, mo.value(QStringLiteral("height")).toInt(4));
        m.cells = cellsFromJson(mo.value(QStringLiteral("cells")));
        m.names = mo.value(QStringLiteral("names")).toString();
        m.namesEdited = !m.names.isEmpty();
        out.maps.append(m);
    }
    *doc = out;
    return true;
}

} // namespace ProjectFile

// ================================================================ ProjectHub

ProjectHub &ProjectHub::instance()
{
    static ProjectHub hub;
    return hub;
}

const ProjectInfo *ProjectHub::find(int id) const
{
    for (const ProjectInfo &p : m_projects)
        if (p.id == id)
            return &p;
    return nullptr;
}

int ProjectHub::findByPath(const QString &path) const
{
    if (path.isEmpty())
        return 0;
    const QString key = QFileInfo(path).absoluteFilePath();
    for (const ProjectInfo &p : m_projects)
        if (!p.path.isEmpty() && QFileInfo(p.path).absoluteFilePath().compare(key, Qt::CaseInsensitive) == 0)
            return p.id;
    return 0;
}

int ProjectHub::create(const QString &name, const QString &path)
{
    ProjectInfo p;
    p.id = ++m_nextId;
    p.name = name;
    p.path = path;
    m_projects.append(p);
    m_active = p.id;
    emit changed();
    return p.id;
}

void ProjectHub::remove(int id)
{
    for (int i = 0; i < m_projects.size(); ++i)
        if (m_projects[i].id == id) {
            m_projects.removeAt(i);
            break;
        }
    if (m_active == id)
        m_active = m_projects.isEmpty() ? 0 : m_projects.last().id;
    emit changed();
}

void ProjectHub::setActive(int id)
{
    if (id == m_active || !find(id))
        return;
    m_active = id;
    emit changed();
}

int ProjectHub::ensureActive()
{
    if (!find(m_active)) {
        if (m_projects.isEmpty())
            create(uniqueProjectName(QStringLiteral("Новый проект")));
        else
            m_active = m_projects.last().id;
    }
    return m_active;
}

void ProjectHub::setName(int id, const QString &name)
{
    for (ProjectInfo &p : m_projects)
        if (p.id == id && p.name != name) {
            p.name = name;
            emit changed();
        }
}

void ProjectHub::setPath(int id, const QString &path)
{
    for (ProjectInfo &p : m_projects)
        if (p.id == id && p.path != path) {
            p.path = path;
            emit changed();
        }
}

void ProjectHub::setHidden(int id, bool hidden)
{
    for (ProjectInfo &p : m_projects)
        if (p.id == id && p.hidden != hidden) {
            p.hidden = hidden;
            emit changed();
        }
}

QString ProjectHub::uniqueProjectName(const QString &base) const
{
    auto taken = [this](const QString &n) {
        for (const ProjectInfo &p : m_projects)
            if (p.name == n)
                return true;
        return false;
    };
    if (!taken(base))
        return base;
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 %2").arg(base).arg(n);
        if (!taken(candidate))
            return candidate;
    }
}

// ---------------------------------------------------------------- страницы

void ProjectHub::attach(QWidget *page, int projectId, PageKind kind, const QString &name)
{
    page->setProperty("govnoProject", projectId);
    page->setProperty("govnoKind", int(kind));
    page->setProperty("govnoName", name);
    ProjectHub &hub = instance();
    for (const QPointer<QWidget> &p : std::as_const(hub.m_pages))
        if (p == page)
            return;
    hub.m_pages.append(page);
}

int ProjectHub::projectOf(const QWidget *page) { return page ? page->property("govnoProject").toInt() : 0; }
ProjectHub::PageKind ProjectHub::kindOf(const QWidget *page) { return page && page->property("govnoKind").toInt() == Map ? Map : Scheme; }
QString ProjectHub::nameOf(const QWidget *page) { return page ? page->property("govnoName").toString() : QString(); }
void ProjectHub::setNameOf(QWidget *page, const QString &name)
{
    if (!page)
        return;
    const QString old = page->property("govnoName").toString();
    page->setProperty("govnoName", name);
    if (old != name && kindOf(page) == Map)
        emit instance().pageRenamed(page, old, name);
}

QVector<QWidget *> ProjectHub::livePages() const
{
    QVector<QWidget *> out;
    for (int i = m_pages.size() - 1; i >= 0; --i)
        if (m_pages[i].isNull())
            m_pages.removeAt(i);
    for (const QPointer<QWidget> &p : m_pages)
        out.append(p.data());
    return out;
}

QString ProjectHub::tabLabel(const QWidget *page) const
{
    const QString name = nameOf(page);
    if (m_projects.size() < 2)
        return name;
    const ProjectInfo *p = find(projectOf(page));
    return p ? name + QStringLiteral("  ·  ") + p->name : name;
}

bool ProjectHub::nameTaken(int projectId, PageKind kind, const QString &name, const QWidget *except) const
{
    for (QWidget *w : livePages())
        if (w != except && projectOf(w) == projectId && kindOf(w) == kind && nameOf(w) == name)
            return true;
    return false;
}

QString ProjectHub::uniqueName(int projectId, PageKind kind, const QString &base, const QWidget *except) const
{
    if (!nameTaken(projectId, kind, base, except))
        return base;
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 %2").arg(base).arg(n);
        if (!nameTaken(projectId, kind, candidate, except))
            return candidate;
    }
}

QString ProjectHub::nextName(int projectId, PageKind kind) const
{
    const QString prefix = kind == Map ? QStringLiteral("Карта %1") : QStringLiteral("Схема %1");
    for (int n = 1;; ++n) {
        const QString name = prefix.arg(n);
        if (!nameTaken(projectId, kind, name))
            return name;
    }
}

QVector<QWidget *> ProjectHub::pagesOf(int projectId) const
{
    QVector<QWidget *> out;
    for (QWidget *w : livePages())
        if (projectOf(w) == projectId)
            out.append(w);
    return out;
}

void ProjectHub::notify()
{
    if (m_pending || QCoreApplication::closingDown())
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
        m_pending = false;
        emit changed();
    });
}
