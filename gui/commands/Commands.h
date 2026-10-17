#pragma once

#include <QList>
#include <QPoint>
#include <QString>
#include <QUndoCommand>
#include <QVector>

class CircuitItem;
class CircuitScene;

// Добавление элементов. Элементы могут уже находиться на сцене (создавались
// «вживую» при рисовании) — тогда первый redo ничего не делает.
class AddItemsCommand : public QUndoCommand
{
public:
    AddItemsCommand(CircuitScene *scene, const QList<CircuitItem *> &items, const QString &text);
    ~AddItemsCommand() override;
    void undo() override;
    void redo() override;

private:
    CircuitScene *m_scene;
    QList<CircuitItem *> m_items;
    bool m_owns = false;      // true, пока элементы сняты со сцены и принадлежат команде
};

// Удаление элементов.
class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(CircuitScene *scene, const QList<CircuitItem *> &items, const QString &text);
    ~RemoveItemsCommand() override;
    void undo() override;
    void redo() override;

private:
    CircuitScene *m_scene;
    QList<CircuitItem *> m_items;
    bool m_owns = false;
};

// Изменение геометрии (перенос, перетаскивание вывода/узла, поворот, отражение).
struct GeometryEntry {
    CircuitItem *item = nullptr;
    QPoint oldA, oldB;
    QPoint newA, newB;
    bool oldFlip = false;
    bool newFlip = false;
};

class GeometryCommand : public QUndoCommand
{
public:
    GeometryCommand(const QVector<GeometryEntry> &entries, const QString &text, bool mergeable = false);
    void undo() override;
    void redo() override;
    int id() const override { return m_mergeable ? 0x6401 : -1; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    QVector<GeometryEntry> m_entries;
    bool m_mergeable;
};

// Изменение текста надписи.
class ChangeTextCommand : public QUndoCommand
{
public:
    ChangeTextCommand(CircuitItem *item, const QString &oldText, const QString &newText);
    void undo() override;
    void redo() override;

private:
    CircuitItem *m_item;
    QString m_old;
    QString m_new;
};

// Правка клеток карты Карно.
class ChangeCellsCommand : public QUndoCommand
{
public:
    ChangeCellsCommand(CircuitItem *item, const QVector<quint8> &oldCells, const QVector<quint8> &newCells);
    void undo() override;
    void redo() override;

private:
    CircuitItem *m_item;
    QVector<quint8> m_old;
    QVector<quint8> m_new;
};

// Изменение одного параметра (склеивается при последовательном вводе).
class ChangeParamCommand : public QUndoCommand
{
public:
    ChangeParamCommand(CircuitItem *item, const QString &key, double oldValue, double newValue);
    void undo() override;
    void redo() override;
    int id() const override { return 0x6402; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    CircuitItem *m_item;
    QString m_key;
    double m_old;
    double m_new;
};
