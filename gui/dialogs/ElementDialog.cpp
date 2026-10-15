#include "dialogs/ElementDialog.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <cmath>

#include "canvas/CircuitScene.h"
#include "items/CircuitItem.h"
#include "items/SymbolPainter.h"
#include "theme/Theme.h"
#include "widgets/ParamForm.h"

ElementDialog::ElementDialog(CircuitItem *item, QWidget *parent)
    : QDialog(parent)
{
    const ElementInfo &info = elementInfo(item->elementType());
    setWindowTitle(QStringLiteral("Свойства: ") + QString::fromUtf8(info.name));
    setMinimumWidth(500);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    // --- заголовок -------------------------------------------------------
    auto *head = new QHBoxLayout;
    head->setSpacing(14);
    auto *icon = new QLabel(this);
    icon->setPixmap(SymbolPainter::icon(item->elementType(), 56, Theme::instance().palette().text));
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *name = new QLabel(QString::fromUtf8(info.name), this);
    name->setObjectName(QStringLiteral("panelTitle"));
    name->setWordWrap(true);
    auto *category = new QLabel(QString::fromUtf8(categoryName(info.category)), this);
    category->setObjectName(QStringLiteral("hint"));
    titles->addWidget(name);
    titles->addWidget(category);
    head->addWidget(icon);
    head->addLayout(titles, 1);
    root->addLayout(head);

    auto *line = new QFrame(this);
    line->setObjectName(QStringLiteral("separator"));
    line->setFrameShape(QFrame::HLine);
    root->addWidget(line);

    // --- текст надписи ---------------------------------------------------
    if (item->isText()) {
        auto *section = new QLabel(QStringLiteral("ТЕКСТ"), this);
        section->setObjectName(QStringLiteral("sectionTitle"));
        root->addWidget(section);
        m_textEdit = new QLineEdit(item->text(), this);
        root->addWidget(m_textEdit);
    }

    // --- параметры -------------------------------------------------------
    if (info.params.empty()) {
        auto *none = new QLabel(QStringLiteral("У этого элемента нет настраиваемых параметров."), this);
        none->setObjectName(QStringLiteral("hint"));
        root->addWidget(none);
    } else {
        auto *section = new QLabel(QStringLiteral("ХАРАКТЕРИСТИКИ"), this);
        section->setObjectName(QStringLiteral("sectionTitle"));
        root->addWidget(section);

        m_form = new ParamForm(item->elementType(), item->params(), this);
        if (info.params.size() > 7) {                       // длинные списки — с прокруткой
            auto *scroll = new QScrollArea(this);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setMinimumHeight(280);
            scroll->setWidget(m_form);
            root->addWidget(scroll, 1);
        } else {
            root->addWidget(m_form);
        }

        auto *tip = new QLabel(ParamForm::suffixHelp(), this);
        tip->setObjectName(QStringLiteral("hint"));
        tip->setWordWrap(true);
        root->addWidget(tip);
    }

    // --- кнопки ----------------------------------------------------------
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    if (m_form) {
        auto *defaults = new QPushButton(QStringLiteral("По умолчанию"), this);
        connect(defaults, &QPushButton::clicked, this, [this] { m_form->resetToDefaults(); });
        buttons->addWidget(defaults);
    }
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

    if (m_textEdit) {
        m_textEdit->setFocus();
        m_textEdit->selectAll();
    } else if (m_form) {
        m_form->focusFirst();
    }
}

void ElementDialog::accept()
{
    if (m_form)
        m_form->commitPending();
    QDialog::accept();
}

QMap<QString, double> ElementDialog::values() const
{
    return m_form ? m_form->values() : QMap<QString, double>();
}
