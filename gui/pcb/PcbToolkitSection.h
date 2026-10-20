#pragma once

#include <QWidget>
#include <QVector>

#include "pcb/PcbTrace.h"

class PcbOptions;
class PcbPage;
class PcbTabs;
class QCheckBox;
class QLabel;
class QPushButton;
class QTimer;
class QScrollArea;
class QStackedWidget;

// Раздел «Печатные платы»: 19 калькуляторов на подвкладках (в три ряда, как в исходной программе),
// справа общие настройки (медь, материал, температуры, единицы) и панель «Информация».
class PcbToolkitSection : public QWidget
{
    Q_OBJECT
public:
    explicit PcbToolkitSection(QWidget *parent = nullptr);

    int pageCount() const { return m_pages.size(); }
    void showPage(int index);

    // Связь со схемой: трасса провода, с которой работают калькуляторы (длина, ширина). Невалидная — снять связь.
    void setTrace(const PcbTrace &trace);
    const PcbTrace &trace() const { return m_trace; }

signals:
    // Результат расчёта текущей вкладки — в комментарий связанного провода. automatic: обновление «само»
    // (только если комментарий пуст или был создан расчётом).
    void traceCommentRequested(const QString &text, bool automatic);
    void traceShowRequested();                 // «Показать на схеме»
    void traceUnlinked();                      // связь сняли кнопкой «×»

private:
    void selectPage(int index);
    void applyContext();
    void refreshInfo();
    void applyStyle();
    void printCurrent();
    void updateLinkBar();
    void scheduleAutoComment();

    QVector<PcbPage *> m_pages;
    PcbTabs *m_tabs = nullptr;
    QStackedWidget *m_stack = nullptr;
    PcbOptions *m_options = nullptr;
    QLabel *m_info = nullptr;
    QWidget *m_body = nullptr;
    int m_current = -1;
    PcbTrace m_trace;
    QWidget *m_linkBar = nullptr;
    QLabel *m_linkLabel = nullptr;
    QCheckBox *m_autoComment = nullptr;
    QPushButton *m_linkWrite = nullptr;
    QTimer *m_autoTimer = nullptr;
};
