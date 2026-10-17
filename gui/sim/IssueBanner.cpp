#include "sim/IssueBanner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "theme/Theme.h"

IssueBanner::IssueBanner(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("issueBanner"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16, 10, 10, 10);
    row->setSpacing(12);

    auto *col = new QVBoxLayout;
    col->setSpacing(2);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("bannerTitle"));
    m_message = new QLabel(this);
    m_message->setWordWrap(true);
    col->addWidget(m_title);
    col->addWidget(m_message);
    row->addLayout(col, 1);

    auto *show = new QPushButton(QStringLiteral("Показать"), this);
    show->setCursor(Qt::PointingHandCursor);
    auto *close = new QPushButton(QStringLiteral("×"), this);
    close->setObjectName(QStringLiteral("bannerClose"));
    close->setCursor(Qt::PointingHandCursor);
    close->setFixedWidth(34);
    row->addWidget(show, 0, Qt::AlignVCenter);
    row->addWidget(close, 0, Qt::AlignVCenter);

    connect(show, &QPushButton::clicked, this, &IssueBanner::showClicked);
    connect(close, &QPushButton::clicked, this, &IssueBanner::closeClicked);
    connect(&Theme::instance(), &Theme::changed, this, &IssueBanner::applyColors);
    applyColors();
    hide();
}

void IssueBanner::setContent(const QString &title, const QString &message)
{
    m_title->setText(title);
    m_message->setText(message);
}

void IssueBanner::applyColors()
{
    const Palette &pal = Theme::instance().palette();
    const QColor red = pal.negative;
    setStyleSheet(QStringLiteral(
        "#issueBanner { background: rgba(%1, %2, %3, 40); border: 1px solid %4; border-radius: 4px; }"
        "#issueBanner QLabel { background: transparent; color: %5; }"
        "#issueBanner QLabel#bannerTitle { color: %4; font-weight: 600; font-size: 14px; }")
        .arg(red.red()).arg(red.green()).arg(red.blue()).arg(red.name(), pal.text.name()));
}
