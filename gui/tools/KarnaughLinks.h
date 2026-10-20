#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

class CircuitItem;
class CircuitView;
class KarnaughWidget;

// Связь элемента схемы «Карта Карно» с картой из раздела «Карты Карно» (того же проекта, по имени карты).
//
// Данные идут в обе стороны:
//   • правка карты (клетки, размер, имена переменных) сразу видна на всех связанных элементах схем проекта;
//   • правка элемента на схеме (щелчок по клетке, размер в свойствах) уходит в карту.
// Размеры: у элемента не больше 16×16. Пока карта не больше 16×16, размер синхронизируется в обе стороны;
// у большей карты на схеме показан фрагмент 16×16, а размер карты правится только в разделе «Карты Карно».
// Правки карты, которые приходят на схему извне, в историю отмены схемы не попадают.
class KarnaughLinks : public QObject
{
    Q_OBJECT
public:
    static KarnaughLinks &instance();

    // Подписка: вид схемы (его сцена сообщает об изменениях элементов) и карта (сообщает о своих правках).
    void attachView(CircuitView *view);
    void attachMap(KarnaughWidget *map);

    QStringList mapNames(int projectId) const;
    KarnaughWidget *mapOf(int projectId, const QString &name) const;

    // То, что связь меняет у элемента; нужно, чтобы отменять смену связи.
    struct ElementState {
        double width = 4, height = 4;
        QVector<quint8> cells;
        QStringList names;
    };
    static ElementState captureElement(const CircuitItem *item);
    void restoreElement(CircuitItem *item, const ElementState &state);     // без отправки в карту

    // Взять данные элемента из связанной карты. false — связи нет или карты с таким именем нет.
    bool pull(CircuitItem *item);
    // Все связанные элементы проекта берут данные из карт (после загрузки проекта, появления карты).
    void syncProject(int projectId);

private:
    KarnaughLinks();

    void onElementChanged(CircuitItem *item);
    void onMapChanged(KarnaughWidget *map);
    void applyToElement(CircuitItem *item, KarnaughWidget *map);
    QVector<CircuitItem *> linkedElements(int projectId, const QString &name, const CircuitItem *except = nullptr) const;
    static CircuitView *viewOf(const CircuitItem *item);

    int m_busy = 0;                          // идёт перенос данных: встречные сигналы игнорируются
};
