#pragma once

#include <QFrame>

class QLabel;

// Красная плашка поверх схемы: «Симуляция остановлена» и причина. «Показать» — выделить виновника,
// «×» — убрать плашку (подсветка на схеме остаётся, пока проблема не исправлена).
class IssueBanner : public QFrame
{
    Q_OBJECT
public:
    explicit IssueBanner(QWidget *parent = nullptr);

    void setContent(const QString &title, const QString &message);

signals:
    void showClicked();
    void closeClicked();

private:
    void applyColors();

    QLabel *m_title = nullptr;
    QLabel *m_message = nullptr;
};
