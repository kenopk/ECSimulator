#pragma once

#include <QPoint>
#include <QTabBar>

class QMimeData;

// Полоса вкладок, из которой вкладку можно вытащить, как в VS Code:
//   • потянули вкладку вверх/вниз/в сторону за пределы полосы — начинается перетаскивание;
//   • отпустили над полосой вкладок другого окна приложения — вкладка переезжает туда (tabDropped);
//   • отпустили в любом другом месте — вкладка уходит в новое окно (detachRequested).
// Единственную вкладку окна можно унести в другое окно, но «оторвать» в новое окно нельзя (это было бы
// простое перемещение окна). Внутри полосы вкладки переставляются обычным образом.
class DetachableTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit DetachableTabBar(QWidget *parent = nullptr);

    // Вид вкладок (0 — схемы, 1 — карты Карно): вкладка принимается только полосой того же вида.
    void setKind(int kind) { m_kind = kind; }
    int kind() const { return m_kind; }

    // Кто владеет полосой (окно редактора) — нужно получателю, чтобы забрать вкладку.
    void setOwner(QObject *owner) { m_owner = owner; }
    QObject *owner() const { return m_owner; }

    // Сейчас где-то в приложении идёт перетаскивание вкладки.
    static bool dragActive();
    // Годится ли перетаскиваемое для приёма, и приём «в конец» (для всей области вкладок, не только полосы).
    static bool canAccept(const QMimeData *mime, int kind);
    void dropAtEnd();

signals:
    void detachRequested(int index, const QPoint &globalPos);
    void tabDropped(DetachableTabBar *source, int sourceIndex, int targetIndex);
    void dragEnded();                 // перетаскивание, начатое с этой полосы, закончилось

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void startDrag(int index);
    int insertIndexAt(const QPoint &pos) const;

    QObject *m_owner = nullptr;
    int m_kind = 0;
    QPoint m_pressPos;
    int m_pressIndex = -1;
};
