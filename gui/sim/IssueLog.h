#pragma once

#include <QWidget>

#include "sim/SimIssue.h"

class QListWidget;
class QListWidgetItem;

// Журнал симуляции: что запускали, что остановили и почему. Строка с проблемой элемента кликается —
// элемент выделяется на схеме. Журнал живёт всё время работы окна, его можно очистить.
class IssueLog : public QWidget
{
    Q_OBJECT
public:
    explicit IssueLog(QWidget *parent = nullptr);

    void addMessage(SimIssue::Severity severity, const QString &text, const QString &hint = QString(),
                    quint64 itemUid = 0, const QVector<int> &pins = {});
    void addIssue(const SimIssue &issue);
    void clear();
    int count() const;

signals:
    void issueActivated(quint64 itemUid, const QVector<int> &pins);   // клик по строке с элементом

private:
    void refreshColors();

    QListWidget *m_list = nullptr;
};
