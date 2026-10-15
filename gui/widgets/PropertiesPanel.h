#pragma once

#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

class CircuitItem;
class CircuitView;
class ParamForm;
class QVBoxLayout;

// Панель «Элемент»: параметры выбранного элемента (применяются сразу, с отменой).
// Ввод понимает инженерные суффиксы (4.7k, 100n, 2.2u, 5V ...).
class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setView(CircuitView *view);
    void focusFirstField();

signals:
    void detailsRequested(CircuitItem *item);    // кнопка «Все характеристики…»

private:
    void scheduleRebuild();
    void rebuild();
    void syncValues();
    void showHint(const QString &text);

    QPointer<CircuitView> m_view;
    QVBoxLayout *m_root = nullptr;
    QWidget *m_body = nullptr;
    ParamForm *m_form = nullptr;
    CircuitItem *m_item = nullptr;
    bool m_rebuildPending = false;
    QVector<QMetaObject::Connection> m_connections;
};
