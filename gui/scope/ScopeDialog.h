#pragma once

#include <QDialog>

#include "scope/ScopeSource.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSlider;

// Окно «Параметры осциллографа» (как свойства элемента): вертикальная и горизонтальная шкалы,
// триггер, каналы, X-Y, вывод информации, название. У каждой осциллограммы свои значения.
class ScopeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScopeDialog(ScopeSource *source, QWidget *parent = nullptr);

    // Открывает окно и применяет изменения при «OK». Возвращает true, если нажали «OK».
    static bool edit(QWidget *parent, ScopeSource *source);

private:
    void load(const ScopeSettings &s);
    ScopeSettings collect() const;
    void updateTimeLabel();
    void syncEnabled();
    QWidget *section(const QString &title, QWidget *body, bool expanded);

    ScopeSource *m_source;

    QRadioButton *m_auto = nullptr;
    QRadioButton *m_autoMax = nullptr;
    QRadioButton *m_manual = nullptr;
    QLineEdit *m_vMax = nullptr;
    QLineEdit *m_iMax = nullptr;

    QCheckBox *m_timeAuto = nullptr;
    QSlider *m_timeSlider = nullptr;
    QLabel *m_timeLabel = nullptr;

    QCheckBox *m_trigger = nullptr;
    QComboBox *m_triggerChannel = nullptr;
    QComboBox *m_triggerEdge = nullptr;
    QLineEdit *m_triggerLevel = nullptr;

    QCheckBox *m_showV = nullptr;
    QCheckBox *m_showI = nullptr;
    QCheckBox *m_xy = nullptr;

    QCheckBox *m_showMax = nullptr;
    QCheckBox *m_infoName = nullptr;
    QCheckBox *m_infoI = nullptr;
    QCheckBox *m_infoV = nullptr;
    QCheckBox *m_infoR = nullptr;
    QCheckBox *m_infoP = nullptr;
    QCheckBox *m_infoF = nullptr;

    QLineEdit *m_name = nullptr;
};
