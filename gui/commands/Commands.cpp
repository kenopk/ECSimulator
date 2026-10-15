#include "commands/Commands.h"

#include "canvas/CircuitScene.h"
#include "items/CircuitItem.h"

namespace {

void addToScene(CircuitScene *scene, const QList<CircuitItem *> &items)
{
    for (CircuitItem *it : items) {
        if (it->scene() != scene)
            scene->addItem(it);
        it->setPreview(false);
    }
}

void removeFromScene(CircuitScene *scene, const QList<CircuitItem *> &items)
{
    for (CircuitItem *it : items) {
        it->setSelected(false);
        if (it->scene() == scene)
            scene->removeItem(it);
    }
}

} // namespace

// ------------------------------------------------------------------ Add

AddItemsCommand::AddItemsCommand(CircuitScene *scene, const QList<CircuitItem *> &items, const QString &text)
    : QUndoCommand(text), m_scene(scene), m_items(items)
{
}

AddItemsCommand::~AddItemsCommand()
{
    if (m_owns)
        qDeleteAll(m_items);
}

void AddItemsCommand::redo()
{
    addToScene(m_scene, m_items);
    m_owns = false;
}

void AddItemsCommand::undo()
{
    removeFromScene(m_scene, m_items);
    m_owns = true;
}

// --------------------------------------------------------------- Remove

RemoveItemsCommand::RemoveItemsCommand(CircuitScene *scene, const QList<CircuitItem *> &items, const QString &text)
    : QUndoCommand(text), m_scene(scene), m_items(items)
{
}

RemoveItemsCommand::~RemoveItemsCommand()
{
    if (m_owns)
        qDeleteAll(m_items);
}

void RemoveItemsCommand::redo()
{
    removeFromScene(m_scene, m_items);
    m_owns = true;
}

void RemoveItemsCommand::undo()
{
    addToScene(m_scene, m_items);
    m_owns = false;
}

// ------------------------------------------------------------- Geometry

GeometryCommand::GeometryCommand(const QVector<GeometryEntry> &entries, const QString &text, bool mergeable)
    : QUndoCommand(text), m_entries(entries), m_mergeable(mergeable)
{
}

void GeometryCommand::redo()
{
    for (const GeometryEntry &e : std::as_const(m_entries))
        e.item->setGeometry(e.newA, e.newB, e.newFlip);
}

void GeometryCommand::undo()
{
    for (const GeometryEntry &e : std::as_const(m_entries))
        e.item->setGeometry(e.oldA, e.oldB, e.oldFlip);
}

bool GeometryCommand::mergeWith(const QUndoCommand *other)
{
    if (!m_mergeable || other->id() != id())
        return false;
    const auto *o = static_cast<const GeometryCommand *>(other);
    if (o->m_entries.size() != m_entries.size())
        return false;
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].item != o->m_entries[i].item)
            return false;
    for (int i = 0; i < m_entries.size(); ++i) {
        m_entries[i].newA = o->m_entries[i].newA;
        m_entries[i].newB = o->m_entries[i].newB;
        m_entries[i].newFlip = o->m_entries[i].newFlip;
    }
    return true;
}

// ---------------------------------------------------------------- Param

ChangeTextCommand::ChangeTextCommand(CircuitItem *item, const QString &oldText, const QString &newText)
    : QUndoCommand(QStringLiteral("Изменение текста")), m_item(item), m_old(oldText), m_new(newText)
{
}

void ChangeTextCommand::redo() { m_item->setText(m_new); }
void ChangeTextCommand::undo() { m_item->setText(m_old); }

ChangeParamCommand::ChangeParamCommand(CircuitItem *item, const QString &key, double oldValue, double newValue)
    : QUndoCommand(QStringLiteral("Изменение параметра")),
      m_item(item), m_key(key), m_old(oldValue), m_new(newValue)
{
}

void ChangeParamCommand::redo() { m_item->setParam(m_key, m_new); }
void ChangeParamCommand::undo() { m_item->setParam(m_key, m_old); }

bool ChangeParamCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id())
        return false;
    const auto *o = static_cast<const ChangeParamCommand *>(other);
    if (o->m_item != m_item || o->m_key != m_key)
        return false;
    m_new = o->m_new;
    return true;
}
