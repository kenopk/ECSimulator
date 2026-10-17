#include "widgets/PropertiesPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>

#include "canvas/CircuitView.h"
#include "items/CircuitItem.h"
#include "items/SymbolPainter.h"
#include "theme/Theme.h"
#include "widgets/ParamForm.h"

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("panelBody"));
    setAttribute(Qt::WA_StyledBackground, true);

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(0);

    rebuild();
}

void PropertiesPanel::setView(CircuitView *view)
{
    for (const QMetaObject::Connection &c : std::as_const(m_connections))
        QObject::disconnect(c);
    m_connections.clear();

    m_view = view;
    if (view) {
        m_connections << connect(view, &CircuitView::selectionChanged, this, [this] { scheduleRebuild(); });
        m_connections << connect(view, &CircuitView::modeChanged,      this, [this] { scheduleRebuild(); });
        m_connections << connect(view->undoStack(), &QUndoStack::indexChanged, this, [this] { syncValues(); });
    }
    rebuild();
}

void PropertiesPanel::scheduleRebuild()
{
    if (m_rebuildPending)
        return;
    m_rebuildPending = true;
    QTimer::singleShot(0, this, [this] { rebuild(); });
}

void PropertiesPanel::showHint(const QString &text)
{
    auto *lay = qobject_cast<QVBoxLayout *>(m_body->layout());
    auto *hint = new QLabel(text, m_body);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    lay->addWidget(hint);
    lay->addStretch(1);
}

void PropertiesPanel::rebuild()
{
    m_rebuildPending = false;
    if (m_form)
        m_form->disconnect(this);        // старая форма больше ничего не коммитит
    m_form = nullptr;
    m_item = nullptr;

    if (m_body) {
        m_root->removeWidget(m_body);
        m_body->hide();
        m_body->deleteLater();
    }
    m_body = new QWidget(this);
    m_body->setAttribute(Qt::WA_StyledBackground, true);
    auto *lay = new QVBoxLayout(m_body);
    lay->setContentsMargins(20, 18, 20, 20);
    lay->setSpacing(12);
    m_root->addWidget(m_body);

    if (!m_view) {
        showHint(QStringLiteral("Нет открытой схемы."));
        return;
    }

    const QList<CircuitItem *> sel = m_view->selectedCircuitItems();

    if (sel.isEmpty()) {
        if (m_view->isSelectMode()) {
            showHint(QStringLiteral(
                "Выберите элемент на схеме, чтобы изменить его параметры.\n\n"
                "Двойной клик по элементу — окно со всеми характеристиками.\n"
                "ПКМ по холсту — список всех элементов."));
        } else {
            const ElementInfo &info = elementInfo(m_view->placeType());
            showHint(QStringLiteral("Размещение: %1\n\n"
                                    "Клик или протяжка ЛКМ — поставить.\n"
                                    "Esc — выйти из режима.")
                         .arg(QString::fromUtf8(info.name)));
        }
        return;
    }
    if (sel.size() > 1) {
        showHint(QStringLiteral("Выбрано элементов: %1\n\n"
                                "Ctrl+R — повернуть, Ctrl+F — отразить, Delete — удалить.").arg(sel.size()));
        return;
    }

    CircuitItem *item = sel.first();
    m_item = item;
    const ElementInfo &info = elementInfo(item->elementType());

    // заголовок
    auto *head = new QWidget(m_body);
    auto *headLay = new QGridLayout(head);
    headLay->setContentsMargins(0, 0, 0, 0);
    headLay->setHorizontalSpacing(12);
    auto *icon = new QLabel(head);
    icon->setPixmap(SymbolPainter::icon(item->elementType(), 40, Theme::instance().palette().text));
    auto *name = new QLabel(QString::fromUtf8(info.name), head);
    name->setObjectName(QStringLiteral("panelTitle"));
    name->setWordWrap(true);
    headLay->addWidget(icon, 0, 0);
    headLay->addWidget(name, 0, 1);
    headLay->setColumnStretch(1, 1);
    lay->addWidget(head);

    if (!item->hasProperties()) {
        const QString text = item->isToggle()
            ? QStringLiteral("Клик по элементу (или Пробел) — переключить.")
            : QStringLiteral("У этого элемента нет настраиваемых параметров.");
        auto *none = new QLabel(text, m_body);
        none->setObjectName(QStringLiteral("hint"));
        none->setWordWrap(true);
        lay->addWidget(none);
    } else {
        if (item->isText()) {
            auto *textEdit = new QLineEdit(item->text(), m_body);
            textEdit->setPlaceholderText(QStringLiteral("Текст надписи"));
            connect(textEdit, &QLineEdit::editingFinished, this, [this, textEdit] {
                if (m_view && m_item)
                    m_view->setItemText(m_item, textEdit->text());
            });
            lay->addWidget(textEdit);
        }
        m_form = new ParamForm(item->elementType(), item->params(), m_body);
        connect(m_form, &ParamForm::valueEdited, this, [this](const QString &key, double value) {
            if (m_view && m_item)
                m_view->setItemParam(m_item, key, value);
        });
        lay->addWidget(m_form);

        auto *tip = new QLabel(ParamForm::suffixHelpShort(), m_body);
        tip->setObjectName(QStringLiteral("hint"));
        tip->setWordWrap(true);
        lay->addWidget(tip);

        if (!item->wheelKey(false).isEmpty()) {
            auto *wheel = new QLabel(item->wheelKey(true).isEmpty()
                                         ? QStringLiteral("Колесо мыши над элементом — менять значение.")
                                         : QStringLiteral("Колесо мыши над элементом — менять значение, "
                                                          "Shift + колесо — номинал/частоту."),
                                     m_body);
            wheel->setObjectName(QStringLiteral("hint"));
            wheel->setWordWrap(true);
            lay->addWidget(wheel);
        }
    }

    if (item->hasProperties()) {
        auto *details = new QPushButton(QStringLiteral("Все характеристики…"), m_body);
        connect(details, &QPushButton::clicked, this, [this] {
            if (m_item)
                emit detailsRequested(m_item);
        });
        lay->addWidget(details);
    }
    lay->addStretch(1);
}

void PropertiesPanel::syncValues()
{
    if (!m_view || !m_item)
        return;
    const QList<CircuitItem *> sel = m_view->selectedCircuitItems();
    if (sel.size() != 1 || sel.first() != m_item) {
        scheduleRebuild();
        return;
    }
    if (m_form)
        m_form->setValues(m_item->params());
}

void PropertiesPanel::focusFirstField()
{
    if (m_form)
        m_form->focusFirst();
}
