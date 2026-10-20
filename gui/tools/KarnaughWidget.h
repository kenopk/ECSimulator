#pragma once

#include <QPoint>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <atomic>
#include <memory>

#include "tools/KarnaughSolver.h"

class QFrame;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTextEdit;
class QTimer;

// Сетка карты Карно: клетки 0 / 1 / X, подписи строк и столбцов кодом Грея.
// ЛКМ — следующее значение (0 → 1 → X → 0), ПКМ — предыдущее; протяжка закрашивает клетки тем же значением.
// Оформление повторяет sublime.tools: тёмные подписи, красная подсветка искомых клеток, диагональ в углу.
class KarnaughCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit KarnaughCanvas(QWidget *parent = nullptr);

    int columns() const { return m_w; }
    int rows() const { return m_h; }
    const QVector<quint8> &cells() const { return m_cells; }

    void resizeMap(int width, int height);     // старые значения сохраняются, новые клетки = 0
    void fill(quint8 value);
    void setVarNames(const QStringList &names);   // имена для подписи в углу (строки, затем столбцы)
    void setCells(const QVector<quint8> &cells);   // значения по строкам, размер должен совпадать с текущим
    void setTarget(quint8 value);                 // какие клетки подсвечивать (1 или 0; X подсвечивается всегда)

signals:
    void changed();
    void geometryChanged();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    static constexpr int kCellW = 48;
    static constexpr int kCellH = 46;

    void updateGeometryHints();
    QPoint cellAt(const QPoint &pos) const;     // (-1, -1) — мимо клеток
    void setCell(const QPoint &c, quint8 value);

    int m_w = 4;
    int m_h = 4;
    QVector<quint8> m_cells;                    // m_h * m_w, по строкам
    QStringList m_names;
    quint8 m_target = Karnaugh::One;
    int m_left = 0;                             // ширина колонки подписей строк
    int m_top = 0;                              // высота строки подписей столбцов
    bool m_painting = false;
    quint8 m_paintValue = 0;
};

// Вкладка «Карта Карно»: имена переменных, размер карты (ширина × высота), карта и четыре результата —
// СДНФ, минимальная ДНФ, СКНФ, минимальная КНФ (отрицания — чертой сверху). Оформление карты взято с
// sublime.tools. Расчёт идёт в фоне, чтобы большая карта не вешала окно.
class KarnaughWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KarnaughWidget(QWidget *parent = nullptr);
    ~KarnaughWidget() override;

    // Всё, что нужно сохранить в проекте: размер, клетки и имена переменных.
    struct State {
        int width = 4;
        int height = 4;
        QVector<quint8> cells;
        QString names;
        bool namesEdited = false;
    };
    State state() const;
    void setState(const State &state);
    QStringList names() const { return effectiveNames(); }     // имена переменных (недостающие — A, B, …)

signals:
    // Пользователь изменил карту (клетки, размер, имена) либо состояние выставили целиком; по нему связанные
    // элементы схемы обновляют свои данные (tools/KarnaughLinks).
    void stateChanged();

private:
    QStringList effectiveNames() const;          // имена из поля, недостающие дополняются A, B, …
    int variableCount() const;
    void onSizeEdited();
    void onNamesChanged();
    void syncDefaultNames();
    void updateTableHeight();
    void scheduleRecompute();
    void recompute();
    void applyResult(const Karnaugh::Result &r, int generation);

    KarnaughCanvas *m_canvas = nullptr;
    QScrollArea *m_tableScroll = nullptr;
    QLineEdit *m_namesEdit = nullptr;
    QLineEdit *m_widthEdit = nullptr;
    QLineEdit *m_heightEdit = nullptr;
    QLabel *m_status = nullptr;
    QTextEdit *m_sdnf = nullptr;
    QTextEdit *m_mdnf = nullptr;
    QTextEdit *m_sknf = nullptr;
    QTextEdit *m_mknf = nullptr;
    QTimer *m_debounce = nullptr;

    bool m_namesEdited = false;                  // пользователь правил имена вручную
    bool m_syncingNames = false;
    bool m_applying = false;                     // идёт setState: промежуточные состояния наружу не отдаём
    void emitState();
    std::shared_ptr<std::atomic<bool>> m_cancel;
    int m_generation = 0;
};
