#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>

// Расчёт по карте Карно произвольного размера (ширина × высота клеток).
//
// Строки и столбцы нумеруются кодом Грея. На каждую ось уходит ceil(log2(n)) переменных,
// первые переменные (A, B, …) — по строкам, остальные — по столбцам. Если размер оси не
// степень двойки, отсутствующие наборы считаются безразличными (X): карта остаётся верной.
namespace Karnaugh {

enum Cell : quint8 { Zero = 0, One = 1, DontCare = 2 };

struct Input {
    int width = 0;                 // столбцов
    int height = 0;                // строк
    QVector<quint8> cells;         // height * width значений Cell, по строкам
    QStringList names;             // имена переменных (строки, затем столбцы); недостающие — A, B, …
};

struct Result {
    QStringList vars;              // использованные имена переменных: сначала строки, затем столбцы
    int rowBits = 0;
    int colBits = 0;
    QString sdnf, sknf;            // совершенные формы
    QString mdnf, mknf;            // минимальные формы
    QString note;                  // пояснение (усечён вывод, слишком большая карта, отмена)
    bool minimized = false;        // удалось ли минимизировать
    bool cancelled = false;
};

// Число бит, нужных для нумерации n позиций (n = 1 -> 0).
int bitsFor(int n);
// Код Грея индекса.
quint32 gray(quint32 i);
// Код Грея индекса в виде строки из bits символов (старший бит слева).
QString grayLabel(int index, int bits);
// Имена переменных A, B, … (после Z — x27, x28, …).
QStringList variableNames(int count);

// Считает все формы. cancel (может быть nullptr) проверяется во время работы.
Result solve(const Input &in, const std::atomic<bool> *cancel = nullptr);

} // namespace Karnaugh
