#include "pcb/PcbHelp.h"
#include "pcb/PcbPagesInternal.h"

// Порядок — по рядам полосы вкладок исходной программы: ряд 1, ряд 2, ряд 3 (см. PcbPageId).
QVector<PcbPage *> createPcbPages(QWidget *parent)
{
    QVector<PcbPage *> pages;
    pages << makeEmbeddedPage(parent) << makeErEffectivePage(parent) << makeFusingPage(parent) << makeMechanicalPage(parent)
          << makeSpacingPage(parent) << makeOhmsLawPage(parent) << makePadstackPage(parent)
          << makePdnPage(parent) << makePlanarPage(parent) << makePpmXtalPage(parent) << makeThermalPage(parent)
          << makeViaPage(parent) << makeWavelengthPage(parent) << makeReactancePage(parent)
          << makeBandwidthPage(parent) << makeImpedancePage(parent) << makePropertiesPage(parent)
          << makeConversionPage(parent) << makeDiffPairsPage(parent);
    Q_ASSERT(pages.size() == PcbPageId::Count);
    for (int i = 0; i < pages.size(); ++i)
        pages[i]->setHelpHtml(pcbHelpHtml(i));
    return pages;
}
