#pragma once

#include <QDialog>
#include <QWidget>

#include "pcb/PcbContext.h"

class PcbChoice;
class PcbField;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

// Правая панель раздела: медь, покрытие, материал, температуры, единицы, кнопки «Печать» и «Расчёт!».
// Серым показываются настройки, которые текущая страница не использует.
class PcbOptions : public QWidget
{
    Q_OBJECT
public:
    explicit PcbOptions(QWidget *parent = nullptr);

    const PcbContext &context() const { return m_ctx; }

    // Включает нужные блоки и подставляет блок страницы (nullptr — стандартные «Плоскость» и «Слой»).
    void setPageConfig(const PcbPageConfig &cfg, QWidget *pageOptions);
    void setPlaneThicknessEnabled(bool on);
    void editSettings();

signals:
    void contextChanged();
    void printRequested();
    void solveRequested();

private:
    void build();
    void readUi();                 // из виджетов в контекст
    void applyContextToUi();       // из контекста в виджеты (после «Параметров»)
    void updateMaterialFromCombo();
    void updateTempLabels();
    void loadSettings();
    void saveSettings() const;

    PcbContext m_ctx;
    bool m_updating = false;

    PcbChoice *m_base = nullptr;
    PcbField *m_baseManual = nullptr;
    PcbChoice *m_plating = nullptr;
    PcbField *m_platingManual = nullptr;
    QWidget *m_slotHost = nullptr;
    QVBoxLayout *m_slotLayout = nullptr;
    QWidget *m_defaultSlot = nullptr;
    QWidget *m_currentSlot = nullptr;
    PcbChoice *m_plane = nullptr;
    PcbChoice *m_layer = nullptr;

    PcbChoice *m_units = nullptr;
    QGroupBox *m_substrate = nullptr;
    QComboBox *m_material = nullptr;
    PcbField *m_er = nullptr;
    PcbField *m_tg = nullptr;
    QGroupBox *m_riseBox = nullptr;
    QSpinBox *m_rise = nullptr;
    QProgressBar *m_riseBar = nullptr;
    QLabel *m_riseF = nullptr;
    QGroupBox *m_ambBox = nullptr;
    QSpinBox *m_amb = nullptr;
    QProgressBar *m_ambBar = nullptr;
    QLabel *m_ambF = nullptr;
};

// Окно «Параметры»: версия IPC, ввод меди вручную, травление, высота переходного, предел аспекта, микроны.
class PcbSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PcbSettingsDialog(const PcbContext &ctx, QWidget *parent = nullptr);
    PcbContext result() const;

private:
    PcbContext m_ctx;
    QComboBox *m_ipc = nullptr;
    QCheckBox *m_manual = nullptr;
    QComboBox *m_etch = nullptr;
    QCheckBox *m_viaHeight = nullptr;
    QDoubleSpinBox *m_aspectThrough = nullptr;
    QDoubleSpinBox *m_aspectMicro = nullptr;
    QCheckBox *m_microns = nullptr;
};
