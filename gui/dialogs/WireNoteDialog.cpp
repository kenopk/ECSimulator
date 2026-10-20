#include "dialogs/WireNoteDialog.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "canvas/CircuitScene.h"
#include "items/CircuitItem.h"

WireNoteDialog::WireNoteDialog(CircuitItem *wire, QWidget *parent)
    : QDialog(parent), m_original(wire->note())
{
    setWindowTitle(QStringLiteral("Провод: комментарий и дифф. пара"));
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto *title = new QLabel(wire->traceName() == QStringLiteral("Провод") ? QStringLiteral("Провод") : wire->traceName(), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(title);

    auto *info = new QLabel(wire->noteFooter(), this);
    info->setObjectName(QStringLiteral("hint"));
    root->addWidget(info);

    auto *section = new QLabel(QStringLiteral("КОММЕНТАРИЙ"), this);
    section->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(section);

    m_text = new QPlainTextEdit(wire->note().text, this);
    m_text->setPlaceholderText(QStringLiteral("Например: CLK_P, диф. пара 100 Ом, не длиннее 30 мм"));
    m_text->setMinimumHeight(84);
    root->addWidget(m_text);

    m_shown = new QCheckBox(QStringLiteral("Показывать блоком на схеме, с линией привязки к проводу"), this);
    m_shown->setChecked(wire->note().shown);
    root->addWidget(m_shown);
    auto *tip = new QLabel(QStringLiteral("Без блока комментарий виден подсказкой при наведении курсора на провод. "
                                          "Блок можно двигать мышью, «×» в его углу скрывает блок (текст остаётся)."), this);
    tip->setObjectName(QStringLiteral("hint"));
    tip->setWordWrap(true);
    root->addWidget(tip);

    auto *form = new QFormLayout;
    m_width = new QDoubleSpinBox(this);
    m_width->setRange(0.0, 100.0);
    m_width->setDecimals(4);
    m_width->setSingleStep(0.05);
    m_width->setSuffix(QStringLiteral(" мм"));
    m_width->setSpecialValueText(QStringLiteral("не задана"));
    m_width->setValue(wire->pcbWidthMm());
    form->addRow(QStringLiteral("Ширина проводника на плате:"), m_width);
    root->addLayout(form);
    auto *wtip = new QLabel(QStringLiteral("Ширина и длина трассы передаются в раздел «Печатные платы», когда вы считаете этот провод в калькуляторах."), this);
    wtip->setObjectName(QStringLiteral("hint"));
    wtip->setWordWrap(true);
    root->addWidget(wtip);

    // --- дифференциальная пара: явная пометка, а не совпадение длин ---------------------------------
    auto *pairSection = new QLabel(QStringLiteral("ДИФФЕРЕНЦИАЛЬНАЯ ПАРА"), this);
    pairSection->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(pairSection);

    const int traceNumber = wire->traceMark().pair;
    m_pair = new QCheckBox(QStringLiteral("Этот провод — часть дифференциальной пары"), this);
    m_pair->setChecked(traceNumber > 0);
    root->addWidget(m_pair);

    int suggested = traceNumber > 0 ? traceNumber : 1;
    if (traceNumber <= 0)
        if (auto *scene = qobject_cast<CircuitScene *>(wire->scene()))
            suggested = scene->nextFreePair();
    auto *pairForm = new QFormLayout;
    m_pairNumber = new QSpinBox(this);
    m_pairNumber->setRange(1, 99);
    m_pairNumber->setValue(suggested);
    m_pairNumber->setEnabled(traceNumber > 0);
    pairForm->addRow(QStringLiteral("Номер пары:"), m_pairNumber);
    root->addLayout(pairForm);
    connect(m_pair, &QCheckBox::toggled, m_pairNumber, &QWidget::setEnabled);

    auto *pairTip = new QLabel(QStringLiteral("Пометка действует на всю трассу — цепочку проводов без ответвлений от вывода до вывода, "
                                              "хоть квадратом с четырьмя поворотами: она считается одним проводником пары. "
                                              "Трассы с одним номером — одна пара: их длины сравниваются, более короткая рисуется красной, "
                                              "а число штрихов на проводе равно номеру пары. Второй проводник пары получает тот же номер."), this);
    pairTip->setObjectName(QStringLiteral("hint"));
    pairTip->setWordWrap(true);
    root->addWidget(pairTip);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new QPushButton(QStringLiteral("Отмена"), this);
    auto *ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setProperty("primary", true);
    ok->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    m_text->setFocus();
}

WireNote WireNoteDialog::note() const
{
    WireNote n = m_original;
    n.text = m_text->toPlainText().trimmed();
    n.shown = m_shown->isChecked();
    n.autoText = m_original.autoText && n.text == m_original.text.trimmed();   // правили руками — больше не «расчётный»
    return n;
}

double WireNoteDialog::widthMm() const { return m_width->value(); }

int WireNoteDialog::pairNumber() const { return m_pair->isChecked() ? m_pairNumber->value() : 0; }
