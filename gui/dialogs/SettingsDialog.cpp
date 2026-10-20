#include "dialogs/SettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QSignalBlocker>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "theme/Theme.h"

namespace {

QIcon swatch(const QColor &color, const QColor &frame)
{
    QPixmap pm(36, 18);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(frame, 1));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(0.5, 0.5, 35, 17), 4, 4);
    return QIcon(pm);
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Настройки"));
    setMinimumWidth(460);

    Theme &theme = Theme::instance();

    auto *themeBox = new QComboBox(this);
    themeBox->addItems(Theme::presetNames());
    themeBox->setCurrentIndex(theme.preset());

    auto *bgButton = new QPushButton(this);
    bgButton->setMinimumWidth(150);
    auto *accentButton = new QPushButton(this);
    accentButton->setMinimumWidth(150);
    auto *accentReset = new QPushButton(QStringLiteral("Синий по умолчанию"), this);

    // Обновление значков-образцов под текущие цвета темы.
    auto refresh = [=]() {
        const Theme &t = Theme::instance();
        const QColor frame = t.palette().border.lighter(150);
        bgButton->setIcon(swatch(t.customBackground(), frame));
        bgButton->setText(t.customBackground().name().toUpper());
        accentButton->setIcon(swatch(t.palette().accent, frame));
        accentButton->setText(t.palette().accent.name().toUpper());
        accentReset->setEnabled(t.customAccent().isValid());
        bgButton->setEnabled(t.preset() == Theme::customPresetIndex());
    };
    refresh();

    // Выбор своего цвета фона. Возвращает false, если пользователь отказался.
    QPointer<SettingsDialog> self(this);
    auto pickBackground = [=]() -> bool {
        QColorDialog dlg(Theme::instance().customBackground(), self);
        dlg.setWindowTitle(QStringLiteral("Цвет фона"));
        if (dlg.exec() != QDialog::Accepted)
            return false;
        Theme::instance().setCustomBackground(dlg.selectedColor());
        return true;
    };

    connect(themeBox, QOverload<int>::of(&QComboBox::activated), this, [=](int i) {
        if (i == Theme::customPresetIndex()) {
            if (!pickBackground()) {                       // отказ — возвращаем прежнюю схему
                QSignalBlocker block(themeBox);
                themeBox->setCurrentIndex(Theme::instance().preset());
                return;
            }
        } else {
            Theme::instance().setPreset(i);
        }
        refresh();
    });
    connect(bgButton, &QPushButton::clicked, this, [=]() { if (pickBackground()) refresh(); });
    connect(accentButton, &QPushButton::clicked, this, [=]() {
        QColorDialog dlg(Theme::instance().palette().accent, self);
        dlg.setWindowTitle(QStringLiteral("Цвет выделения"));
        if (dlg.exec() == QDialog::Accepted) {
            Theme::instance().setCustomAccent(dlg.selectedColor());
            refresh();
        }
    });
    connect(accentReset, &QPushButton::clicked, this, [=]() {
        Theme::instance().resetAccent();
        refresh();
    });

    auto *accentRow = new QHBoxLayout;
    accentRow->setSpacing(8);
    accentRow->addWidget(accentButton);
    accentRow->addWidget(accentReset);
    accentRow->addStretch(1);

    auto *bgRow = new QHBoxLayout;
    bgRow->setSpacing(8);
    bgRow->addWidget(themeBox, 1);
    bgRow->addWidget(bgButton);

    auto *gridBox = new QComboBox(this);
    gridBox->addItems({ QStringLiteral("Точки"), QStringLiteral("Линии"), QStringLiteral("Без сетки") });
    gridBox->setCurrentIndex(static_cast<int>(theme.gridStyle()));
    connect(gridBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [](int i) { Theme::instance().setGridStyle(static_cast<GridStyle>(i)); });

    auto *labels = new QCheckBox(QStringLiteral("Показывать значения рядом с элементами"), this);
    labels->setChecked(theme.showLabels());
    connect(labels, &QCheckBox::toggled, this, [](bool on) { Theme::instance().setShowLabels(on); });

    auto *junctions = new QCheckBox(QStringLiteral("Показывать узлы и свободные выводы"), this);
    junctions->setChecked(theme.showJunctions());
    connect(junctions, &QCheckBox::toggled, this, [](bool on) { Theme::instance().setShowJunctions(on); });

    auto *nearMiss = new QCheckBox(QStringLiteral("Подсвечивать красным концы, стоящие вплотную к чужим (не соединены)"), this);
    nearMiss->setChecked(theme.showNearMiss());
    connect(nearMiss, &QCheckBox::toggled, this, [](bool on) { Theme::instance().setShowNearMiss(on); });

    auto *lengthMarks = new QCheckBox(QStringLiteral("Дифф. пары на схеме: штрихи по номеру пары, красным — проводник короче напарника"), this);
    lengthMarks->setChecked(theme.showLengthMarks());
    connect(lengthMarks, &QCheckBox::toggled, this, [](bool on) { Theme::instance().setShowLengthMarks(on); });

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->addRow(QStringLiteral("Цветовая схема"), bgRow);
    form->addRow(QStringLiteral("Цвет выделения"), accentRow);
    form->addRow(QStringLiteral("Сетка"), gridBox);
    form->addRow(QString(), labels);
    form->addRow(QString(), junctions);
    form->addRow(QString(), nearMiss);
    form->addRow(QString(), lengthMarks);

    auto *close = new QPushButton(QStringLiteral("Закрыть"), this);
    close->setProperty("primary", true);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(close);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(18);
    root->addLayout(form);
    root->addLayout(buttons);
}
