#pragma once
// Внутренний заголовок страниц раздела «Печатные платы»: фабрики страниц и мелкие помощники раскладки.
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include "pcb/PcbPage.h"

// Фабрики по файлам (порядок в createPcbPages — см. PcbPageId).
PcbPage *makeBandwidthPage(QWidget *parent);
PcbPage *makeImpedancePage(QWidget *parent);
PcbPage *makeErEffectivePage(QWidget *parent);
PcbPage *makeWavelengthPage(QWidget *parent);
PcbPage *makeDiffPairsPage(QWidget *parent);

PcbPage *makePropertiesPage(QWidget *parent);
PcbPage *makeFusingPage(QWidget *parent);
PcbPage *makeViaPage(QWidget *parent);
PcbPage *makeSpacingPage(QWidget *parent);

PcbPage *makePdnPage(QWidget *parent);
PcbPage *makePlanarPage(QWidget *parent);
PcbPage *makePadstackPage(QWidget *parent);
PcbPage *makeThermalPage(QWidget *parent);
PcbPage *makeMechanicalPage(QWidget *parent);
PcbPage *makeEmbeddedPage(QWidget *parent);

PcbPage *makeConversionPage(QWidget *parent);
PcbPage *makeOhmsLawPage(QWidget *parent);
PcbPage *makePpmXtalPage(QWidget *parent);
PcbPage *makeReactancePage(QWidget *parent);

// Сетка внутри группы: отступ сверху оставляет место под заголовок рамки.
inline QGridLayout *pcbGrid(QWidget *group, int margin = 12)
{
    auto *g = new QGridLayout(group);
    g->setContentsMargins(margin, 20, margin, margin);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(8);
    return g;
}

// Вложенная группа со своей сеткой.
inline QGroupBox *pcbSubGroup(const QString &title, QWidget *parent, QGridLayout **layout = nullptr)
{
    QGroupBox *g = makePcbGroup(title, parent);
    QGridLayout *l = pcbGrid(g, 10);
    if (layout)
        *layout = l;
    return g;
}

// Светлая карточка с формулой, как на иллюстрациях исходной программы.
inline QLabel *pcbFormula(const QString &html, QWidget *parent)
{
    auto *l = new QLabel(html, parent);
    l->setTextFormat(Qt::RichText);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral("background:#f1f3f7; color:#232a36; border-radius:10px; padding:10px 14px; font-size:15px;"));
    return l;
}

inline QPushButton *pcbButton(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

// Подсказка-примечание мелким шрифтом.
inline QLabel *pcbNote(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("pcbHint"));
    l->setWordWrap(true);
    return l;
}
