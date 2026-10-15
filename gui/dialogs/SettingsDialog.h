#pragma once

#include <QDialog>

// Настройки внешнего вида. Изменения применяются сразу (см. Theme).
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
};
