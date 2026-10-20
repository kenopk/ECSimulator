#pragma once

#include <QStringList>
#include <QWidget>

#include "pcb/PcbContext.h"
#include "pcb/PcbTrace.h"
#include "pcb/PcbWidgets.h"

class QGroupBox;
class QToolButton;
class QResizeEvent;

// Номера страниц в порядке на полосе вкладок (ряд 1, ряд 2, ряд 3).
namespace PcbPageId {
enum : int {
    Embedded = 0, ErEffective, Fusing, Mechanical, Spacing, OhmsLaw, Padstack,
    Pdn, Planar, PpmXtal, Thermal, Via, Wavelength, Reactance,
    Bandwidth, Impedance, Properties, Conversion, DiffPairs,
    Count
};
}

// Страница раздела «Печатные платы»: большая рамка слева, получает общие настройки и сама считает.
// Страницы — потомки этого класса, лежат в PcbPages*.cpp; наружу видна только createPcbPages().
class PcbPage : public QWidget
{
    Q_OBJECT
public:
    PcbPage(const QString &tabTitle, const QString &groupTitle, QWidget *parent = nullptr);

    QString tabTitle() const { return m_tabTitle; }

    // Какие блоки правой панели нужны странице.
    virtual PcbPageConfig config() const { return {}; }
    // Свои группы переключателей вместо «Толщина плоскости / Слой проводника» (nullptr — стандартный блок).
    virtual QWidget *optionsWidget() { return nullptr; }
    // «Толщина плоскости» доступна только когда на странице выбрано «Плоскость есть».
    virtual bool planeThicknessEnabled() const { return config().planeThickness; }
    // Строки панели «Информация» (HTML), вслед за «Полной толщиной меди».
    virtual QStringList infoItems() const { return {}; }
    virtual void solve() = 0;
    // Значение, присланное другой страницей (например «Er эфф.» → «Длина волны»).
    virtual void receiveValue(const QString &key, double value) { Q_UNUSED(key) Q_UNUSED(value) }

    // Связь со схемой: страница берёт длину и ширину провода (setTrace вызывается при каждой смене трассы), а её результат
    // можно записать в комментарий провода (traceSummary). usesTrace() — страница вообще работает с трассой.
    virtual bool usesTrace() const { return false; }
    virtual void setTrace(const PcbTrace &trace) { Q_UNUSED(trace) }
    virtual QString traceSummary() const { return QString(); }

    // Справка по вкладке: что считает, как пользоваться, что значат результаты. Первый абзац <p> — краткая подсказка
    // при наведении на «?», целиком текст открывается по щелчку.
    virtual QString helpHtml() const { return m_helpHtml; }
    void setHelpHtml(const QString &html) { m_helpHtml = html; }

    void setContext(const PcbContext &ctx);
    const PcbContext &context() const { return m_ctx; }

signals:
    void changed();                       // пересчитано: можно обновить «Информацию»
    // Перейти на страницу и передать ей значение (key пустой — просто перейти).
    void sendValue(int pageId, const QString &key, double value);

protected:
    // Вызывается после смены общих настроек, до пересчёта: страница обновляет единицы полей и подписи.
    virtual void contextChanged() {}
    void recalc();                        // solve() + changed()
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

    QGroupBox *frame() const { return m_frame; }
    // Поле ввода/вывода с подписью сверху, подключённое к пересчёту.
    PcbField *makeInput(double base, const QString &unit = QString());
    PcbField *makeOutput(const QString &unit = QString(), int decimals = 4);

private:
    void placeHelpButton();

    QString m_tabTitle;
    QGroupBox *m_frame = nullptr;
    QToolButton *m_help = nullptr;
    QString m_helpHtml;
    PcbContext m_ctx;
};

// Все страницы в порядке появления на полосе вкладок (по рядам, как в исходной программе).
QVector<PcbPage *> createPcbPages(QWidget *parent);
