#include "sim/IssueLog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

#include "theme/Theme.h"

namespace {

constexpr int kRoleUid = Qt::UserRole;
constexpr int kRolePins = Qt::UserRole + 1;
constexpr int kRoleSeverity = Qt::UserRole + 2;

QColor colorFor(SimIssue::Severity s)
{
    const Palette &pal = Theme::instance().palette();
    switch (s) {
    case SimIssue::Severity::Error:   return pal.negative;
    case SimIssue::Severity::Warning: return pal.accent;
    default:                          return pal.textDim;
    }
}

} // namespace

IssueLog::IssueLog(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 6, 8, 8);
    lay->setSpacing(6);

    auto *bar = new QHBoxLayout;
    auto *hint = new QLabel(QStringLiteral("Двойной клик по строке — показать элемент на схеме"), this);
    hint->setObjectName(QStringLiteral("hint"));
    auto *clear = new QPushButton(QStringLiteral("Очистить"), this);
    bar->addWidget(hint, 1);
    bar->addWidget(clear);
    lay->addLayout(bar);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("issueList"));
    m_list->setUniformItemSizes(false);
    m_list->setWordWrap(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    lay->addWidget(m_list, 1);

    connect(clear, &QPushButton::clicked, this, &IssueLog::clear);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const quint64 uid = item->data(kRoleUid).toULongLong();
        if (uid)
            emit issueActivated(uid, item->data(kRolePins).value<QVector<int>>());
    });
    connect(&Theme::instance(), &Theme::changed, this, &IssueLog::refreshColors);
}

void IssueLog::addMessage(SimIssue::Severity severity, const QString &text, const QString &hint,
                          quint64 itemUid, const QVector<int> &pins)
{
    QString line = QStringLiteral("%1   %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text);
    if (!hint.isEmpty())
        line += QStringLiteral("\n          ") + hint;

    auto *item = new QListWidgetItem(line, m_list);
    item->setData(kRoleUid, itemUid);
    item->setData(kRolePins, QVariant::fromValue(pins));
    item->setData(kRoleSeverity, int(severity));
    item->setForeground(colorFor(severity));
    m_list->scrollToBottom();
}

void IssueLog::addIssue(const SimIssue &issue)
{
    addMessage(issue.severity, issue.text, issue.hint, issue.itemUid, issue.pins);
}

void IssueLog::clear()
{
    m_list->clear();
}

int IssueLog::count() const
{
    return m_list->count();
}

void IssueLog::refreshColors()
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        item->setForeground(colorFor(SimIssue::Severity(item->data(kRoleSeverity).toInt())));
    }
}
