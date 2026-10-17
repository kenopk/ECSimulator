#include "tools/KarnaughSolver.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Karnaugh {

namespace {

struct Cancelled {};
struct TooBig {};

constexpr qint64 kBudget = 120'000'000;     // предел «операций» на одну минимизацию
constexpr int kMaxTermsShown = 4000;        // сколько слагаемых совершенной формы выводить
constexpr long kExactNodes = 300000;        // предел перебора в точном покрытии

struct Budget {
    const std::atomic<bool> *cancel = nullptr;
    qint64 used = 0;

    void spend(qint64 n)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            throw Cancelled{};
        used += n;
        if (used > kBudget)
            throw TooBig{};
    }
};

// Импликанта: v — значения фиксированных бит, m — маска «чёрточек» (бит свободен).
struct Impl {
    quint32 v = 0;
    quint32 m = 0;
};

inline int popcount(quint32 x)
{
    int c = 0;
    while (x) { x &= x - 1; ++c; }
    return c;
}

// ------------------------------------------------------------ минимизация

// Простые импликанты (Куайн–Мак-Класки). terms — наборы со значением target и безразличные.
std::vector<Impl> primeImplicants(const std::vector<quint32> &terms, int n, Budget &budget)
{
    auto pack = [](quint32 v, quint32 m) { return (std::uint64_t(m) << 32) | v; };

    std::vector<std::uint64_t> level;
    level.reserve(terms.size());
    for (quint32 t : terms)
        level.push_back(pack(t, 0));

    std::vector<Impl> primes;
    while (!level.empty()) {
        std::unordered_set<std::uint64_t> set(level.begin(), level.end());
        std::unordered_set<std::uint64_t> used;
        std::unordered_set<std::uint64_t> next;

        for (std::uint64_t key : level) {
            budget.spend(n + 1);
            const quint32 v = quint32(key & 0xFFFFFFFFu);
            const quint32 m = quint32(key >> 32);
            for (int bit = 0; bit < n; ++bit) {
                const quint32 b = quint32(1) << bit;
                if ((m & b) || (v & b))
                    continue;
                if (set.count(key | b)) {                 // сосед отличается только этим битом
                    used.insert(key);
                    used.insert(key | b);
                    next.insert(key | (std::uint64_t(b) << 32));
                }
            }
        }
        for (std::uint64_t key : level)
            if (!used.count(key))
                primes.push_back({ quint32(key & 0xFFFFFFFFu), quint32(key >> 32) });

        level.assign(next.begin(), next.end());
    }
    return primes;
}

struct CoverProblem {
    std::vector<std::vector<int>> coversOf;     // для простой импликанты — индексы покрытых единиц
    std::vector<int> cost;                      // стоимость: 1000 за слагаемое + число литералов
};

// Выбор покрытия: обязательные → точный перебор (малый остаток) или жадный выбор → удаление лишних.
std::vector<int> chooseCover(const CoverProblem &p, int ones, Budget &budget)
{
    const int P = int(p.coversOf.size());
    std::vector<std::vector<int>> primesOf(ones);
    for (int i = 0; i < P; ++i)
        for (int o : p.coversOf[i])
            primesOf[o].push_back(i);

    std::vector<char> selected(P, 0), covered(ones, 0);
    auto take = [&](int i) {
        if (selected[i])
            return;
        selected[i] = 1;
        for (int o : p.coversOf[i])
            covered[o] = 1;
    };

    for (int o = 0; o < ones; ++o)
        if (primesOf[o].size() == 1)
            take(primesOf[o][0]);

    std::vector<int> rem;
    for (int o = 0; o < ones; ++o)
        if (!covered[o])
            rem.push_back(o);

    if (!rem.empty()) {
        std::vector<int> cand;
        std::vector<char> isCand(P, 0);
        for (int o : rem)
            for (int i : primesOf[o])
                if (!selected[i] && !isCand[i]) {
                    isCand[i] = 1;
                    cand.push_back(i);
                }

        // жадный вариант — он же верхняя граница для точного
        std::vector<int> greedy;
        {
            std::vector<char> cov = covered;
            int left = int(rem.size());
            while (left > 0) {
                budget.spend(qint64(cand.size()) + 1);
                int best = -1, bestGain = 0;
                for (int i : cand) {
                    int gain = 0;
                    for (int o : p.coversOf[i])
                        if (!cov[o])
                            ++gain;
                    if (gain > bestGain || (gain == bestGain && gain > 0 && best >= 0 && p.cost[i] < p.cost[best])) {
                        best = i;
                        bestGain = gain;
                    }
                }
                if (best < 0)
                    break;
                greedy.push_back(best);
                for (int o : p.coversOf[best])
                    if (!cov[o]) {
                        cov[o] = 1;
                        --left;
                    }
            }
        }
        std::vector<int> bestSet = greedy;

        // точный перебор, пока остаток небольшой
        if (rem.size() <= 48 && cand.size() <= 48) {
            std::vector<int> pos(ones, -1);
            for (size_t k = 0; k < rem.size(); ++k)
                pos[rem[k]] = int(k);
            std::vector<std::uint64_t> cmask(cand.size(), 0);
            for (size_t c = 0; c < cand.size(); ++c)
                for (int o : p.coversOf[cand[c]])
                    if (pos[o] >= 0)
                        cmask[c] |= std::uint64_t(1) << pos[o];

            long bestCost = 0;
            for (int i : greedy)
                bestCost += p.cost[i];
            long nodes = 0;
            std::vector<int> cur;

            struct Rec {
                const std::vector<std::uint64_t> &cmask;
                const std::vector<int> &cand;
                const CoverProblem &p;
                std::vector<int> &cur;
                std::vector<int> &bestSet;
                long &bestCost;
                long &nodes;

                void go(std::uint64_t left, long cost)
                {
                    if (++nodes > kExactNodes || cost >= bestCost)
                        return;
                    if (!left) {
                        bestCost = cost;
                        bestSet.clear();
                        for (int c : cur)
                            bestSet.push_back(cand[c]);
                        return;
                    }
                    // единица с наименьшим числом покрывающих кандидатов
                    int bestBit = -1, bestCnt = 1 << 30;
                    for (int b = 0; b < 64; ++b) {
                        if (!(left >> b & 1))
                            continue;
                        int cnt = 0;
                        for (size_t c = 0; c < cmask.size(); ++c)
                            if (cmask[c] >> b & 1)
                                ++cnt;
                        if (cnt < bestCnt) {
                            bestCnt = cnt;
                            bestBit = b;
                        }
                    }
                    for (size_t c = 0; c < cmask.size(); ++c) {
                        if (!(cmask[c] >> bestBit & 1))
                            continue;
                        cur.push_back(int(c));
                        go(left & ~cmask[c], cost + p.cost[cand[c]]);
                        cur.pop_back();
                    }
                }
            };
            const std::uint64_t all = rem.size() == 64 ? ~std::uint64_t(0)
                                                       : (std::uint64_t(1) << rem.size()) - 1;
            Rec rec{ cmask, cand, p, cur, bestSet, bestCost, nodes };
            rec.go(all, 0);
        }
        for (int i : bestSet)
            take(i);
    }

    // удаляем лишние импликанты (начиная с самых дорогих)
    std::vector<int> chosen;
    for (int i = 0; i < P; ++i)
        if (selected[i])
            chosen.push_back(i);
    std::sort(chosen.begin(), chosen.end(), [&](int a, int b) { return p.cost[a] > p.cost[b]; });
    std::vector<int> cnt(ones, 0);
    for (int i : chosen)
        for (int o : p.coversOf[i])
            ++cnt[o];
    std::vector<int> out;
    for (int i : chosen) {
        bool needed = false;
        for (int o : p.coversOf[i])
            if (cnt[o] < 2) {
                needed = true;
                break;
            }
        if (needed)
            out.push_back(i);
        else
            for (int o : p.coversOf[i])
                --cnt[o];
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Минимальное покрытие значений target (безразличные можно использовать).
std::vector<Impl> minimize(const std::vector<quint8> &vals, int n, quint8 target, Budget &budget)
{
    std::vector<quint32> terms, ones;
    std::vector<int> idxOf(vals.size(), -1);
    for (quint32 t = 0; t < quint32(vals.size()); ++t) {
        if (vals[t] == target) {
            idxOf[t] = int(ones.size());
            ones.push_back(t);
            terms.push_back(t);
        } else if (vals[t] == DontCare) {
            terms.push_back(t);
        }
        if ((t & 0xFFFF) == 0)
            budget.spend(1);
    }
    if (ones.empty())
        return {};

    const std::vector<Impl> primes = primeImplicants(terms, n, budget);

    CoverProblem prob;
    std::vector<Impl> useful;
    for (const Impl &im : primes) {
        std::vector<int> cov;
        const int pc = popcount(im.m);
        if (pc < 24 && (std::uint64_t(1) << pc) <= ones.size()) {
            budget.spend(qint64(1) << pc);
            quint32 s = im.m;
            for (;;) {                                   // все подмаски m
                const int idx = idxOf[im.v | s];
                if (idx >= 0)
                    cov.push_back(idx);
                if (s == 0)
                    break;
                s = (s - 1) & im.m;
            }
        } else {
            budget.spend(qint64(ones.size()));
            for (size_t o = 0; o < ones.size(); ++o)
                if ((ones[o] & ~im.m) == im.v)
                    cov.push_back(int(o));
        }
        if (cov.empty())
            continue;                                    // покрывает только безразличные
        useful.push_back(im);
        prob.cost.push_back(1000 + (n - pc));
        prob.coversOf.push_back(std::move(cov));
    }

    std::vector<Impl> out;
    for (int i : chooseCover(prob, int(ones.size()), budget))
        out.push_back(useful[size_t(i)]);
    return out;
}

// ------------------------------------------------------------ форматирование

// Литералы набора: bit равен 1 -> переменная, 0 -> отрицание (invert меняет это местами).
QString literals(quint32 v, quint32 m, int n, const QStringList &names, bool invert, const QString &sep)
{
    QStringList parts;
    for (int i = 0; i < n; ++i) {
        const int bit = n - 1 - i;
        if (m >> bit & 1)
            continue;
        const bool one = (v >> bit & 1) != 0;
        parts << ((one != invert) ? names[i] : QStringLiteral("¬") + names[i]);
    }
    return parts.join(sep);
}

QString productText(quint32 v, quint32 m, int n, const QStringList &names)
{
    const QString s = literals(v, m, n, names, false, QStringLiteral("·"));
    return s.isEmpty() ? QStringLiteral("1") : s;
}

QString clauseText(quint32 v, quint32 m, int n, const QStringList &names, bool wrap)
{
    const QString s = literals(v, m, n, names, true, QStringLiteral(" ∨ "));
    if (s.isEmpty())
        return QStringLiteral("0");
    return wrap && s.contains(QLatin1Char(' ')) ? QStringLiteral("(") + s + QStringLiteral(")") : s;
}

QString joinDnf(const std::vector<Impl> &sel, int n, const QStringList &names)
{
    if (sel.empty())
        return QStringLiteral("0");
    QStringList terms;
    for (const Impl &im : sel)
        terms << productText(im.v, im.m, n, names);
    return terms.join(QStringLiteral(" ∨ "));
}

QString joinCnf(const std::vector<Impl> &sel, int n, const QStringList &names)
{
    if (sel.empty())
        return QStringLiteral("1");
    QStringList clauses;
    const bool wrap = sel.size() > 1;
    for (const Impl &im : sel)
        clauses << clauseText(im.v, im.m, n, names, wrap);
    return clauses.join(QStringLiteral("·"));
}

// Совершенная форма: по одному слагаемому на набор со значением target (не более kMaxTermsShown).
QString perfect(const std::vector<quint8> &vals, int n, const QStringList &names, quint8 target,
                const std::atomic<bool> *cancel, QString *note)
{
    QStringList parts;
    qint64 total = 0;
    for (quint32 t = 0; t < quint32(vals.size()); ++t) {
        if ((t & 0xFFFF) == 0 && cancel && cancel->load(std::memory_order_relaxed))
            throw Cancelled{};
        if (vals[t] != target)
            continue;
        ++total;
        if (parts.size() < kMaxTermsShown)
            parts << (target == One ? productText(t, 0, n, names) : clauseText(t, 0, n, names, true));
    }
    if (total == 0)
        return target == One ? QStringLiteral("0") : QStringLiteral("1");
    QString s = parts.join(target == One ? QStringLiteral(" ∨ ") : QStringLiteral("·"));
    if (total > parts.size()) {
        s += QStringLiteral(" … (ещё %1)").arg(total - parts.size());
        if (note && note->isEmpty())
            *note = QStringLiteral("Совершенные формы усечены: показано %1 слагаемых.").arg(kMaxTermsShown);
    }
    return s;
}

} // namespace

// ================================================================== публичное

int bitsFor(int n)
{
    int bits = 0;
    while ((qint64(1) << bits) < n)
        ++bits;
    return bits;
}

quint32 gray(quint32 i)
{
    return i ^ (i >> 1);
}

QString grayLabel(int index, int bits)
{
    if (bits <= 0)
        return QString();
    return QStringLiteral("%1").arg(gray(quint32(index)), bits, 2, QLatin1Char('0'));
}

QStringList variableNames(int count)
{
    QStringList names;
    for (int i = 0; i < count; ++i)
        names << (i < 26 ? QString(QChar('A' + i)) : QStringLiteral("x%1").arg(i + 1));
    return names;
}

Result solve(const Input &in, const std::atomic<bool> *cancel)
{
    Result r;
    r.rowBits = bitsFor(in.height);
    r.colBits = bitsFor(in.width);
    const int n = r.rowBits + r.colBits;
    r.vars = variableNames(n);
    for (int i = 0; i < n && i < in.names.size(); ++i)
        if (!in.names[i].trimmed().isEmpty())
            r.vars[i] = in.names[i].trimmed();

    if (in.width < 1 || in.height < 1 || in.cells.size() != qsizetype(in.width) * in.height) {
        r.note = QStringLiteral("Некорректный размер карты.");
        return r;
    }
    if (n > 30) {
        r.note = QStringLiteral("Карта слишком велика для расчёта.");
        return r;
    }

    std::vector<quint8> vals(size_t(1) << n, DontCare);       // отсутствующие наборы — безразличные
    for (int row = 0; row < in.height; ++row)
        for (int col = 0; col < in.width; ++col) {
            const quint32 t = (gray(quint32(row)) << r.colBits) | gray(quint32(col));
            vals[t] = in.cells[qsizetype(row) * in.width + col];
        }

    try {
        r.sdnf = perfect(vals, n, r.vars, One, cancel, &r.note);
        r.sknf = perfect(vals, n, r.vars, Zero, cancel, &r.note);

        Budget b1; b1.cancel = cancel;
        Budget b2; b2.cancel = cancel;
        try {
            r.mdnf = joinDnf(minimize(vals, n, One, b1), n, r.vars);
            r.mknf = joinCnf(minimize(vals, n, Zero, b2), n, r.vars);
            r.minimized = true;
        } catch (const TooBig &) {
            r.mdnf.clear();
            r.mknf.clear();
            const QString msg = QStringLiteral("Карта слишком велика для минимизации (переменных: %1).").arg(n);
            r.note = r.note.isEmpty() ? msg : r.note + QLatin1Char(' ') + msg;
        }
    } catch (const Cancelled &) {
        r.cancelled = true;
    }
    return r;
}

} // namespace Karnaugh
