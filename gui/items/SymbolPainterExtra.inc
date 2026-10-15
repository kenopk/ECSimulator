// ============================================================================
//  Дополнительные символы (подключается из SymbolPainter.cpp внутрь анонимного
//  пространства имён — использует его вспомогательные функции lead/line/text…).
//
//  1) универсальный корпус микросхемы (триггеры, счётчики, ОЗУ, ПЗУ, АЦП …);
//  2) особые рисунки: реле, оптопара, индикаторы, диодная матрица;
//  3) радиоэлементы и полупроводники, которых нет в базовом наборе.
// ============================================================================

// ------------------------------------------------------------ корпус микросхемы

// Положение вывода относительно центра символа: для выводов слева/справа важна только y,
// для выводов сверху/снизу — смещение dx от центра и y.
struct PinPos {
    qreal dx = 0;
    qreal y = 0;
    int edge = 0;          // 0 — слева, 1 — справа, 2 — сверху, 3 — снизу
};

struct ChipGeom {
    govno::core::ChipLayout layout;
    Spec spec;
    qreal w = 48;          // ширина корпуса (или рисунка модуля)
    qreal H = 26;          // полувысота корпуса
    qreal rx0 = -24, rx1 = 24, ry0 = -26, ry1 = 26;   // прямоугольник рисунка относительно (центр, 0)
    qreal padS = 16;       // минимальная длина вывода слева/справа
    qreal pitchS = 32;     // шаг выводов слева/справа
    qreal pitchT = 32;     // шаг выводов сверху/снизу
    bool glyph = false;    // заголовок вида "@имя" — особый рисунок вместо надписи
    bool module = false;   // «модуль»: рисуется сам модуль без корпуса-обёртки
    unsigned variant = 0;
    std::vector<PinPos> pp;
};

int utf8Length(const std::string &s)
{
    int n = 0;
    for (unsigned char ch : s)
        if ((ch & 0xC0) != 0x80)
            ++n;
    return n;
}

// Координата j-го из n выводов с шагом pitch, центрированная и «посаженная» на сетку 16 px.
qreal pinSlot(int j, int n, qreal pitch)
{
    qreal raw = (j - (n - 1) / 2.0) * pitch;
    if (std::fabs(std::fmod(std::fabs(raw), 16.0)) > 0.01)
        raw -= 8.0;
    return raw;
}

qreal ceil16(qreal v) { return std::ceil(v / 16.0 - 1e-9) * 16.0; }

const ChipGeom *chipGeom(ElementType t, unsigned variant)
{
    static std::map<std::pair<int, unsigned>, ChipGeom> cache;
    const auto key = std::make_pair(int(t), variant);
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second.layout.pins.empty() ? nullptr : &it->second;

    ChipGeom g;
    g.variant = variant;
    const ElementInfo &info = elementInfo(t);
    if (info.chip) {
        g.layout = govno::core::layoutFor(info, variant);
        const auto &lay = g.layout;
        const int nl = lay.leftCount, nr = lay.rightCount, nt = lay.topCount, nb = lay.bottomCount;
        const std::string &ti = lay.title;
        g.glyph = !ti.empty() && ti[0] == '@';
        const bool lcd = ti == "@lcd", keypad = ti == "@keypad", dip = ti == "@dip", ledm = ti == "@ledm";
        g.module = lcd || keypad || dip || ledm;

        // шаги выводов
        if (ledm || lcd) {
            g.pitchS = g.pitchT = 16.0;
        } else if (keypad) {
            const int r = int(variant & 0xFF), c = int((variant >> 8) & 0xFF);
            g.pitchS = g.pitchT = std::max(r, c) <= 8 ? 32.0 : 16.0;
        } else if (dip) {
            g.pitchT = int(variant) <= 8 ? 32.0 : 16.0;
        }

        // длина самых длинных имён по сторонам
        qreal maxL = 0, maxR = 0, maxT = 0;
        for (const auto &p : lay.pins) {
            const qreal len = utf8Length(p.name) * (g.module ? 6.0 : 6.8) + (p.clock ? 6.0 : 0.0);
            if (p.edge == 0) maxL = std::max(maxL, len);
            else if (p.edge == 1) maxR = std::max(maxR, len);
            else maxT = std::max(maxT, len);
        }

        // координаты выводов
        std::vector<qreal> ys0, ys1, xs2, xs3;
        for (int j = 0; j < nl; ++j) ys0.push_back(pinSlot(j, nl, g.pitchS));
        for (int j = 0; j < nr; ++j) ys1.push_back(pinSlot(j, nr, g.pitchS));
        for (int j = 0; j < nt; ++j) xs2.push_back(pinSlot(j, nt, g.pitchT));
        for (int j = 0; j < nb; ++j) xs3.push_back(pinSlot(j, nb, g.pitchT));
        if (dip) xs3 = xs2;                                  // у DIP обе стороны выровнены по переключателям

        if (g.module) {
            if (ledm || keypad) {
                const qreal pc = g.pitchT;
                const qreal hw = ledm ? 8.0 : pc / 2.0;
                const qreal hh = ledm ? 8.0 : g.pitchS / 2.0;
                g.rx0 = (xs2.empty() ? -hw : xs2.front() - hw);
                g.rx1 = (xs2.empty() ? hw : xs2.back() + hw);
                g.ry0 = (ys0.empty() ? -hh : ys0.front() - hh);
                g.ry1 = (ys0.empty() ? hh : ys0.back() + hh);
            } else if (dip) {
                g.rx0 = (xs2.empty() ? 0.0 : xs2.front()) - g.pitchT / 2.0;
                g.rx1 = (xs2.empty() ? 0.0 : xs2.back()) + g.pitchT / 2.0;
                g.ry0 = -24.0;
                g.ry1 = 24.0;
            } else {                                         // lcd
                int cc = 16, rr = 2;
                govno::core::lcdSize(variant, &cc, &rr);
                const qreal mw = std::max<qreal>(cc * 14.0 + 20.0, 8 * 16.0 + 16.0);
                const qreal mh = std::max<qreal>(rr * 14.0 + 8.0, 32.0);
                g.rx0 = -mw / 2.0;
                g.rx1 = mw / 2.0;
                g.ry0 = -mh;
                g.ry1 = mh;
            }
            g.w = g.rx1 - g.rx0;
            g.H = std::max(-g.ry0, g.ry1);
            g.padS = std::max(maxL, maxR) + 10.0;
        } else {
            const int rows = std::max({ nl, nr, 1 });
            qreal need = maxL + maxR + 26.0;
            const qreal titleW = g.glyph ? 0.0 : utf8Length(lay.title) * 9.5 + 18.0;
            need = std::max(need, titleW);
            if (nt > 1) need = std::max(need, (nt - 1) * g.pitchT + 44.0);
            if (nb > 1) need = std::max(need, (nb - 1) * g.pitchT + 44.0);
            if (nt == 1 || nb == 1) need = std::max(need, 56.0);
            if (g.glyph) {
                qreal minW = 80.0;
                if (ti == "@relay" || ti.compare(0, 4, "@seg") == 0)
                    minW = 96.0;
                else if (ti == "@dmat")
                    minW = 36.0 + nr * 12.0 + maxL + maxR;
                else if (ti == "@xsw")
                    minW = 96.0;
                else if (ti == "@rcont")
                    minW = 128.0;
                need = std::max(need, minW);
            }
            g.w = std::max<qreal>(48.0, std::ceil(need / 16.0) * 16.0);
            g.H = std::max(rows * 16.0 + 10.0, g.glyph ? 58.0 : 26.0);
            if (nt > 0 || nb > 0)
                g.H = ceil16(g.H + 12.0);
            g.rx0 = -g.w / 2.0;
            g.rx1 = g.w / 2.0;
            g.ry0 = -g.H;
            g.ry1 = g.H;
            g.padS = 16.0;
        }

        // выводы сверху/снизу: на 16 (или на длину надписи) дальше края рисунка
        qreal padV = 16.0;
        if (g.module && g.pitchT < 32.0)
            padV = std::max(16.0, ceil16(maxT + 8.0));
        const qreal yTop = -(ceil16(-g.ry0) + (g.module ? padV : 16.0));
        const qreal yBot = ceil16(g.ry1) + (g.module ? padV : 16.0);

        int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        for (const auto &p : lay.pins) {
            PinPos pp;
            pp.edge = p.edge;
            if (p.edge == 0) pp.y = ys0[std::size_t(c0++)];
            else if (p.edge == 1) pp.y = ys1[std::size_t(c1++)];
            else if (p.edge == 2) { pp.dx = xs2[std::size_t(c2++)]; pp.y = yTop; }
            else { pp.dx = xs3[std::size_t(c3++)]; pp.y = yBot; }
            g.pp.push_back(pp);
        }

        Spec s;
        s.body = g.w;
        qreal half = std::max(g.ry1, -g.ry0);
        if (nt > 0) half = std::max(half, -yTop);
        if (nb > 0) half = std::max(half, yBot);
        for (qreal y : ys0) half = std::max(half, std::fabs(y) + 8.0);
        for (qreal y : ys1) half = std::max(half, std::fabs(y) + 8.0);
        s.halfHeight = half + 2.0;
        const bool vert = nt > 0 || nb > 0;
        if (vert || g.module) {
            const qreal halfL = std::max(g.padS - g.rx0, g.padS + g.rx1);
            s.minGrid = int(std::ceil(2.0 * halfL / 32.0 - 1e-9)) * 2;
            s.stepGrid = 2;
        } else {
            s.minGrid = int(std::lround((g.w + 32.0) / kGrid));
            s.stepGrid = 1;
        }
        s.pinCount = int(std::min<std::size_t>(lay.pins.size(), kMaxPins));
        s.axisOnly = true;
        for (int i = 0; i < s.pinCount; ++i) {
            const PinPos &pp = g.pp[std::size_t(i)];
            PinSpec &ps = s.pin[i];
            ps.side = pp.edge;
            ps.v = pp.y / kGrid;
            ps.u = pp.edge == 0 ? 0.0 : pp.edge == 1 ? 1.0 : 0.5;
            ps.du = (pp.edge >= 2) ? pp.dx : 0.0;
        }
        g.spec = s;
    }
    auto res = cache.emplace(key, std::move(g));
    return res.first->second.layout.pins.empty() ? nullptr : &res.first->second;
}


// Надпись внутри корпуса: остаётся горизонтальной и «читается» при любом повороте.
// align: -1 — от точки вправо, +1 — от точки влево, 0 — по центру.
void chipText(const Ctx &c, const P &pos, const QString &s, qreal px, int align, bool overline, bool bold)
{
    QPainter *p = c.p;
    p->save();
    p->translate(pos);
    p->scale(1.0, c.flip ? -1.0 : 1.0);
    p->rotate(-c.angle);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(std::max(1, int(px * c.textScale)));
    f.setBold(bold);
    p->setFont(f);
    p->setPen(c.body);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(s);
    const qreal h = fm.height();
    qreal x = -w / 2.0;
    if (align < 0)
        x = 0;
    else if (align > 0)
        x = -w;
    p->drawText(QRectF(x - 2, -h / 2.0, w + 4, h), Qt::AlignCenter, s);
    if (overline) {
        p->setPen(QPen(c.body, std::max<qreal>(1.0, c.width * 0.5)));
        p->drawLine(QPointF(x, -h / 2.0 + 1.5), QPointF(x + w, -h / 2.0 + 1.5));
    }
    p->restore();
}

void chipGlyph(const Ctx &c, const ChipGeom &g, qreal cx);

// Перевод вектора «на экране» в локальные координаты символа (с учётом поворота и отражения).
P screenToLocal(const Ctx &c, qreal sx, qreal sy)
{
    const qreal a = c.angle * kPi / 180.0;
    const qreal x = sx * std::cos(a) + sy * std::sin(a);
    const qreal y = -sx * std::sin(a) + sy * std::cos(a);
    return P(x, c.flip ? -y : y);
}

// Направление локального вектора на экране.
P localToScreen(const Ctx &c, qreal vx, qreal vy)
{
    const qreal a = c.angle * kPi / 180.0;
    const qreal fy = c.flip ? -vy : vy;
    return P(vx * std::cos(a) - fy * std::sin(a), vx * std::sin(a) + fy * std::cos(a));
}

// Надпись, повёрнутая на 90° (читается снизу вверх), по центру точки.
void chipTextRot(const Ctx &c, const P &pos, const QString &s, qreal px)
{
    QPainter *p = c.p;
    p->save();
    p->translate(pos);
    p->scale(1.0, c.flip ? -1.0 : 1.0);
    p->rotate(-c.angle);
    p->rotate(-90.0);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(std::max(1, int(px * c.textScale)));
    p->setFont(f);
    p->setPen(c.body);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(s);
    const qreal h = fm.height();
    p->drawText(QRectF(-w / 2.0 - 2, -h / 2.0, w + 4, h), Qt::AlignCenter, s);
    p->restore();
}

// Имя вывода рядом с выводом снаружи рисунка: вдоль вывода, сбоку (используется у модулей).
// S — точка вывода, E — конец вывода у рисунка, perpPitch — шаг между соседними выводами.
void leadName(const Ctx &c, const P &S, const P &E, const QString &name, qreal px, qreal perpPitch, bool overline)
{
    const P d = E - S;
    const qreal len = std::hypot(d.x(), d.y());
    if (len < 1.0 || name.isEmpty())
        return;
    const P u(d.x() / len, d.y() / len);
    const P sd = localToScreen(c, u.x(), u.y());
    if (std::fabs(sd.y()) <= 0.5) {                          // вывод горизонтален на экране: подпись над ним
        const P anchor = S + P(u.x() * 3.0, u.y() * 3.0) + screenToLocal(c, 0.0, -(px * 0.5 + 3.5));
        chipText(c, anchor, name, px, sd.x() > 0 ? -1 : 1, overline, false);
    } else if (perpPitch >= 31.0) {                          // вертикален, места хватает: обычная надпись сбоку
        const P mid((S.x() + E.x()) / 2.0, (S.y() + E.y()) / 2.0);
        chipText(c, mid + screenToLocal(c, 3.0, 0.0), name, px, -1, overline, false);
    } else {                                                 // вертикален и тесно: надпись «лёжа»
        const P mid((S.x() + E.x()) / 2.0, (S.y() + E.y()) / 2.0);
        chipTextRot(c, mid + screenToLocal(c, px * 0.5 + 3.0, 0.0), name, px);
    }
}

// Рисунок модулей (ЖК-дисплей, клавиатура, DIP-переключатель, матрица светодиодов): без корпуса-обёртки.
void paintModuleArt(const Ctx &c, const ChipGeom &g, qreal cx)
{
    QPainter *p = c.p;
    const std::string &ti = g.layout.title;
    std::vector<qreal> ys, xs;
    for (const PinPos &pp : g.pp) {
        if (pp.edge == 0)
            ys.push_back(pp.y);
        else if (pp.edge == 2)
            xs.push_back(pp.dx);
    }
    QColor faint = c.body;
    faint.setAlpha(70);
    setBody(c, 0.8);

    if (ti == "@lcd") {
        int cc = 16, rr = 2;
        govno::core::lcdSize(g.variant, &cc, &rr);
        const bool on = c.state != 0;
        QColor fill = c.body;
        fill.setAlpha(on ? 50 : 16);
        p->setBrush(fill);
        p->drawRoundedRect(QRectF(cx + g.rx0, g.ry0, g.w, g.ry1 - g.ry0), 4, 4);
        const qreal cw = 14.0;
        const qreal ch = (g.ry1 - g.ry0 - 16.0) / rr;
        const qreal xs0 = cx - cc * cw / 2.0;
        static const char *kText[4] = { "Hello, World!", "GOVNO circuits", "0123456789ABCDEF", "The quick brown fox" };
        for (int r = 0; r < rr; ++r)
            for (int k = 0; k < cc; ++k) {
                const qreal x = xs0 + k * cw;
                const qreal y = g.ry0 + 8.0 + r * ch;
                p->setPen(QPen(faint, std::max<qreal>(0.6, c.width * 0.35)));
                QColor cell = c.body;
                cell.setAlpha(on ? 28 : 8);
                p->setBrush(cell);
                p->drawRect(QRectF(x + 0.8, y + 1.0, cw - 1.6, ch - 2.0));
                const char *s = kText[r % 4];
                if (on && k < int(std::strlen(s)) && s[k] != ' ')
                    chipText(c, P(x + cw / 2.0, y + ch / 2.0), QString(QChar(s[k])), 12.0, 0, false, true);
            }
        p->setBrush(Qt::NoBrush);
    } else if (ti == "@keypad") {
        const int cols = int(xs.size());
        const qreal pk = g.pitchT;
        for (std::size_t j = 0; j < ys.size(); ++j)
            for (std::size_t k = 0; k < xs.size(); ++k) {
                const int num = int(j) * cols + int(k) + 1;
                const qreal x = cx + xs[k];
                const qreal y = ys[j];
                const bool pressed = c.state == num;
                QColor fill = c.body;
                fill.setAlpha(pressed ? 110 : 0);
                p->setBrush(fill);
                p->setPen(penFor(c, c.body, pk >= 32.0 ? 0.6 : 0.45));
                const qreal half = pk / 2.0 - 3.0;
                p->drawRoundedRect(QRectF(x - half, y - half, 2 * half, 2 * half), pk >= 32.0 ? 4 : 2, pk >= 32.0 ? 4 : 2);
                const QString label = QString::number(num);
                chipText(c, P(x, y), label, pk >= 32.0 ? (label.size() >= 3 ? 8.0 : 11.0) : (label.size() >= 3 ? 5.5 : 7.0),
                         0, false, true);
            }
        p->setBrush(Qt::NoBrush);
    } else if (ti == "@dip") {
        const quint32 mask = quint32(c.state);
        const qreal tw = g.pitchT - 8.0;
        for (std::size_t k = 0; k < xs.size(); ++k) {
            const qreal x = cx + xs[k];
            const bool on = (mask >> k) & 1u;
            p->setBrush(Qt::NoBrush);
            p->setPen(penFor(c, c.body, 0.6));
            p->drawRoundedRect(QRectF(x - tw / 2.0, g.ry0, tw, g.ry1 - g.ry0), 3, 3);
            QColor knob = c.body;
            knob.setAlpha(on ? 230 : 90);
            p->setBrush(knob);
            const qreal mid = (g.ry0 + g.ry1) / 2.0;
            p->drawRoundedRect(QRectF(x - tw / 2.0, on ? g.ry0 : mid, tw, (g.ry1 - g.ry0) / 2.0), 3, 3);
        }
        p->setBrush(Qt::NoBrush);
    } else if (ti == "@ledm") {                              // светодиоды на пересечениях строк и столбцов
        p->setPen(QPen(faint, std::max<qreal>(0.5, c.width * 0.3)));
        for (qreal y : ys)
            p->drawLine(P(cx + g.rx0 + 2.0, y), P(cx + g.rx1 - 2.0, y));
        for (qreal x : xs)
            p->drawLine(P(cx + x, g.ry0 + 2.0), P(cx + x, g.ry1 - 2.0));
        p->setPen(penFor(c, c.body, 0.6));
        QColor led = c.body;
        led.setAlpha(40);
        p->setBrush(led);
        for (qreal y : ys)
            for (qreal x : xs)
                p->drawEllipse(P(cx + x, y), 4.6, 4.6);
        p->setBrush(Qt::NoBrush);
    }
}

void paintChip(const Ctx &c, const ChipGeom &g)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal x0 = cx + g.rx0;
    const qreal x1 = cx + g.rx1;
    const qreal y0 = g.ry0;
    const qreal y1 = g.ry1;

    const qreal ca = std::cos(c.angle * kPi / 180.0);
    const bool vertical = std::fabs(ca) < 0.5;
    const qreal px = g.module ? 8.5 : (vertical ? 8.0 : 11.0);

    const int n = int(std::min<std::size_t>(g.layout.pins.size(), kMaxPins));
    for (int i = 0; i < n; ++i) {
        const auto &pin = g.layout.pins[std::size_t(i)];
        const PinPos &pp = g.pp[std::size_t(i)];
        const qreal bubble = (pin.inverted && !g.module) ? 7.0 : 0.0;
        switch (pp.edge) {
        case 0:  lead(c, i, P(0, pp.y), P(x0 - bubble, pp.y)); break;
        case 1:  lead(c, i, P(L, pp.y), P(x1 + bubble, pp.y)); break;
        case 2:  lead(c, i, P(cx + pp.dx, pp.y), P(cx + pp.dx, y0 - bubble)); break;
        default: lead(c, i, P(cx + pp.dx, pp.y), P(cx + pp.dx, y1 + bubble)); break;
        }
        if (bubble > 0.0) {
            setBody(c);
            switch (pp.edge) {
            case 0:  c.p->drawEllipse(P(x0 - 3.5, pp.y), 3.5, 3.5); break;
            case 1:  c.p->drawEllipse(P(x1 + 3.5, pp.y), 3.5, 3.5); break;
            case 2:  c.p->drawEllipse(P(cx + pp.dx, y0 - 3.5), 3.5, 3.5); break;
            default: c.p->drawEllipse(P(cx + pp.dx, y1 + 3.5), 3.5, 3.5); break;
            }
        }
    }

    if (!g.module) {
        setBody(c);
        c.p->drawRect(QRectF(x0, y0, x1 - x0, y1 - y0));
    }

    for (int i = 0; i < n; ++i) {
        const auto &pin = g.layout.pins[std::size_t(i)];
        const PinPos &pp = g.pp[std::size_t(i)];
        const QString name = QString::fromStdString(pin.name);
        if (g.module) {
            P S, E;
            qreal perp = g.pitchS;
            switch (pp.edge) {
            case 0:  S = P(0, pp.y);  E = P(x0, pp.y); break;
            case 1:  S = P(L, pp.y);  E = P(x1, pp.y); break;
            case 2:  S = P(cx + pp.dx, pp.y); E = P(cx + pp.dx, y0); perp = g.pitchT; break;
            default: S = P(cx + pp.dx, pp.y); E = P(cx + pp.dx, y1); perp = g.pitchT; break;
            }
            leadName(c, S, E, name, px, perp, pin.inverted);
            continue;
        }
        if (pp.edge >= 2) {
            if (!name.isEmpty())
                chipText(c, P(cx + pp.dx, pp.edge == 2 ? y0 + 9.0 : y1 - 9.0), name, px, 0, pin.inverted, false);
            continue;
        }
        const bool left = pp.edge == 0;
        if (pin.clock) {                                     // знак динамического входа
            setBody(c);
            const qreal ex = left ? x0 : x1;
            const qreal d = left ? 1.0 : -1.0;
            QPolygonF tri;
            tri << P(ex, pp.y - 5) << P(ex + d * 7, pp.y) << P(ex, pp.y + 5);
            c.p->drawPolyline(tri);
        }
        if (name.isEmpty())
            continue;
        const qreal inset = 4.0 + (pin.clock ? 7.0 : 0.0);
        const qreal ax = left ? x0 + inset : x1 - inset;
        int align;
        if (vertical) {
            align = 0;
        } else {
            const bool leftOnScreen = (ca > 0) == left;      // метка прижата к левому краю экрана
            align = leftOnScreen ? -1 : 1;
        }
        chipText(c, P(ax, pp.y), name, px, align, pin.inverted, false);
    }

    if (g.module) {
        paintModuleArt(c, g, cx);
    } else if (g.glyph) {
        chipGlyph(c, g, cx);
    } else {
        const qreal ty = y0 + (g.layout.topCount > 0 ? 24.0 : 9.0);
        chipText(c, P(cx, ty), QString::fromStdString(g.layout.title), vertical ? 9.0 : 12.0, 0, false, true);
    }
}

// Номер клавиши/переключателя под точкой (локальные координаты без отражения); 0 — мимо.
int partAtImpl(const ChipGeom &g, qreal length, const P &pt)
{
    const std::string &ti = g.layout.title;
    const qreal cx = length / 2.0;
    std::vector<qreal> ys, xs;
    for (const PinPos &pp : g.pp) {
        if (pp.edge == 0)
            ys.push_back(pp.y);
        else if (pp.edge == 2)
            xs.push_back(pp.dx);
    }
    if (ti == "@keypad") {
        const qreal hp = g.pitchT / 2.0;
        for (std::size_t j = 0; j < ys.size(); ++j)
            for (std::size_t k = 0; k < xs.size(); ++k)
                if (std::fabs(pt.x() - (cx + xs[k])) <= hp && std::fabs(pt.y() - ys[j]) <= hp)
                    return int(j * xs.size() + k) + 1;
    } else if (ti == "@dip") {
        const qreal hp = g.pitchT / 2.0;
        if (pt.y() >= g.ry0 && pt.y() <= g.ry1)
            for (std::size_t k = 0; k < xs.size(); ++k)
                if (std::fabs(pt.x() - (cx + xs[k])) <= hp)
                    return int(k) + 1;
    }
    return 0;
}


// Рисунки внутри корпуса.
void segmentDigit(const Ctx &c, int kind, qreal cx)       // kind: 0 — 7, 1 — 14, 2 — 16, 3 — 21 сегмент
{
    QPainter *p = c.p;
    const qreal k = kind == 0 ? 1.0 : 1.35;
    p->save();
    p->translate(cx, 0);
    p->scale(k, k);
    const qreal w = 10, h = 18;
    p->setPen(penFor(c, c.body, 0.9 / k));
    p->setBrush(Qt::NoBrush);
    auto seg = [&](qreal x0, qreal y0, qreal x1, qreal y1) { p->drawLine(P(x0, y0), P(x1, y1)); };
    if (kind == 0 || kind == 1) {
        seg(-w + 2, -h, w - 2, -h);
        seg(-w + 2, h, w - 2, h);
    } else {                                              // верх и низ разрезаны посередине
        seg(-w + 2, -h, -1, -h);  seg(1, -h, w - 2, -h);
        seg(-w + 2, h, -1, h);    seg(1, h, w - 2, h);
    }
    seg(w, -h + 2, w, -2);  seg(w, 2, w, h - 2);          // b, c
    seg(-w, -h + 2, -w, -2); seg(-w, 2, -w, h - 2);       // f, e
    if (kind == 0) {
        seg(-w + 2, 0, w - 2, 0);                         // g
    } else {
        seg(-w + 2, 0, -1, 0);  seg(1, 0, w - 2, 0);      // g1, g2
        seg(-w + 3, -h + 3, -2, -3);  seg(w - 3, -h + 3, 2, -3);   // диагонали сверху
        seg(-w + 3, h - 3, -2, 3);    seg(w - 3, h - 3, 2, 3);     // диагонали снизу
        seg(0, -h + 2, 0, -2);  seg(0, 2, 0, h - 2);               // вертикали посередине
    }
    if (kind == 3) {                                      // дополнительные короткие сегменты
        seg(-4, -9, 4, -9);  seg(-4, 9, 4, 9);
        seg(-w - 3, -h, -w - 3, h);  seg(w + 3, -h, w + 3, h);
        seg(-w + 2, -h - 3, w - 2, -h - 3);
    }
    p->setBrush(c.body);
    p->drawEllipse(P(w + 6, h), 1.6, 1.6);                // десятичная точка
    p->restore();
}

void chipGlyph(const Ctx &c, const ChipGeom &g, qreal cx)
{
    const std::string &name = g.layout.title;
    QPainter *p = c.p;
    setBody(c, 0.8);
    if (name == "@relay") {                               // катушка, пунктир, контакт
        p->drawRect(QRectF(cx - 22, -11, 12, 22));
        line(c, P(cx - 22, 0), P(cx - 10, 0));
        p->save();
        p->setPen(QPen(c.body, c.width * 0.6, Qt::DashLine, Qt::FlatCap));
        p->drawLine(P(cx - 4, -8), P(cx + 6, -8));
        p->restore();
        line(c, P(cx + 6, 12), P(cx + 16, -10));
        p->drawEllipse(P(cx + 6, 12), 1.8, 1.8);
        p->drawEllipse(P(cx + 18, -12), 1.8, 1.8);
    } else if (name == "@opto") {                         // светодиод — стрелки — фототранзистор
        QPolygonF tri;
        tri << P(cx - 22, -8) << P(cx - 22, 8) << P(cx - 11, 0);
        p->drawPolygon(tri);
        line(c, P(cx - 11, -8), P(cx - 11, 8));
        for (qreal dy : { -6.0, 2.0 }) {
            line(c, P(cx - 4, dy), P(cx + 4, dy + 6));
            arrowHead(c, P(cx + 6, dy + 8), P(0.6, 0.8), 4.5);
        }
        line(c, P(cx + 12, -9), P(cx + 12, 9));
        line(c, P(cx + 12, -4), P(cx + 22, -11));
        line(c, P(cx + 12, 4), P(cx + 22, 11));
        arrowHead(c, P(cx + 22, 11), P(0.77, 0.64), 4.5);
    } else if (name == "@seg7") {
        segmentDigit(c, 0, cx);
    } else if (name == "@seg14") {
        segmentDigit(c, 1, cx);
    } else if (name == "@seg16") {
        segmentDigit(c, 2, cx);
    } else if (name == "@seg21") {
        segmentDigit(c, 3, cx);
    } else if (name == "@xsw" || name == "@rcont") {      // перекрёстный переключатель / контакт реле
        const qreal hx = std::max<qreal>(18.0, g.w / 2.0 - 22.0);
        p->setBrush(c.body);
        if (name == "@xsw") {
            const bool crossed = c.state != 0;
            const qreal y1 = -kGrid, y2 = kGrid;
            line(c, P(cx - hx, y1), P(cx + hx, crossed ? y2 : y1));
            line(c, P(cx - hx, y2), P(cx + hx, crossed ? y1 : y2));
            for (qreal yy : { y1, y2 }) {
                p->drawEllipse(P(cx - hx, yy), 2.0, 2.0);
                p->drawEllipse(P(cx + hx, yy), 2.0, 2.0);
            }
        } else {
            p->setBrush(Qt::NoBrush);
            line(c, P(cx - hx, 0), P(cx + hx - 6, -kGrid + 6));            // подвижный контакт
            p->drawEllipse(P(cx - hx, 0), 2.2, 2.2);
            p->drawEllipse(P(cx + hx, -kGrid), 2.2, 2.2);
            p->drawEllipse(P(cx + hx, kGrid), 2.2, 2.2);
        }
    } else if (name == "@dmat") {                         // входные строки, выходные столбцы, диоды
        const int rows = std::max(1, g.layout.leftCount);
        const int cols = std::max(1, g.layout.rightCount);
        qreal nameL = 0, nameR = 0;
        for (const auto &pn : g.layout.pins) {
            const qreal len = utf8Length(pn.name) * 6.8 + (pn.clock ? 6.0 : 0.0);
            (pn.left ? nameL : nameR) = std::max(pn.left ? nameL : nameR, len);
        }
        const qreal gw = std::max<qreal>(24.0, g.w - nameL - nameR - 24.0);
        cx += (nameL - nameR) / 2.0;
        const qreal pitch = gw / cols;
        const qreal xl = cx - gw / 2.0;
        const qreal top = (-(rows - 1)) * kGrid - 8.0;
        const qreal bottom = (rows - 1) * kGrid + 8.0;
        for (int i = 0; i < rows; ++i) {
            const qreal y = (-(rows - 1) + 2.0 * i) * kGrid;
            line(c, P(xl - 6, y), P(xl + gw - pitch / 2.0 + 6, y));
        }
        for (int j = 0; j < cols; ++j) {
            const qreal x = xl + pitch * (j + 0.5);
            line(c, P(x, top), P(x, bottom));
        }
        const int preset = int((g.variant >> 16) & 0xFF);
        p->setBrush(c.body);
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j) {
                bool on = false;
                switch (preset) {
                case 0:  on = (i % cols) == j; break;
                case 1:  on = true; break;
                case 2:  on = ((i + j) % 2) == 0; break;
                default: on = false; break;
                }
                if (on)
                    p->drawEllipse(P(xl + pitch * (j + 0.5), (-(rows - 1) + 2.0 * i) * kGrid), 2.8, 2.8);
            }
    }
}

// ----------------------------------------------------------- одновыводные

// Шины, генераторы, метки: ножка и знак на её конце.
// Линия оформления: просто отрезок, электрически ни с чем не соединяется.
void paintLine(const Ctx &c)
{
    c.p->setPen(QPen(c.body, std::max<qreal>(1.5, c.width * 0.8), Qt::SolidLine, Qt::RoundCap));
    c.p->drawLine(P(0, 0), P(c.length, 0));
}

// Рамка: p1 и p2 — противоположные углы; стороны всегда горизонтальны и вертикальны на экране.
void paintBox(const Ctx &c)
{
    QPainter *p = c.p;
    p->save();
    if (c.flip)
        p->scale(1.0, -1.0);                                 // CircuitItem уже отразил систему координат
    const qreal th = c.angle * kPi / 180.0;
    const qreal co = std::cos(th), si = std::sin(th);
    const qreal dx = c.length * co, dy = c.length * si;      // диагональ в координатах экрана
    const P pts[4] = { P(0, 0), P(dx, 0), P(dx, dy), P(0, dy) };
    QPolygonF poly;
    for (const P &q : pts)
        poly << P(q.x() * co + q.y() * si, -q.x() * si + q.y() * co);   // обратно в локальные оси
    p->setPen(QPen(c.body, std::max<qreal>(1.5, c.width * 0.8), Qt::DashLine, Qt::SquareCap, Qt::MiterJoin));
    p->setBrush(Qt::NoBrush);
    p->drawPolygon(poly);
    p->restore();
}

void paintSingle(const Ctx &c, int kind)
{
    const qreal L = c.length;
    QPainter *p = c.p;
    const qreal r = 10.0;

    if (kind == 9) {                                         // антенна
        lead(c, 0, P(0, 0), P(L - 11, 0));
        line(c, P(L - 11, 0), P(L, -10));
        line(c, P(L - 11, 0), P(L, 10));
        return;
    }
    if (kind == 13) {                                        // метка узла: «флажок»
        lead(c, 0, P(0, 0), P(L - 24, 0));
        setBody(c);
        QPolygonF tag;
        tag << P(L - 24, 0) << P(L - 16, -8) << P(L, -8) << P(L, 8) << P(L - 16, 8);
        p->drawPolygon(tag);
        text(c, P(L - 9, 0), QStringLiteral("#"), 11);
        return;
    }
    if (kind == 10 || kind == 11) {                          // логический вход/выход: окошко 0/1
        lead(c, 0, P(0, 0), P(L - 22, 0));
        setBody(c);
        p->drawRect(QRectF(L - 22, -9, 22, 18));
        text(c, P(L - 11, 0), kind == 10 ? QString::number(c.state != 0 ? 1 : 0) : QStringLiteral("0/1"),
             kind == 10 ? 14 : 10);
        return;
    }
    if (kind >= 14 && kind <= 21) {                          // прямоугольные «значки» ввода/вывода
        static const char *const kText[] = { "AUD", "DAT", "EXT", "OUT", "TP", "REC", "SND", "STOP" };
        const QString t = QString::fromLatin1(kText[kind - 14]);
        if (kind == 18) {                                    // контрольная точка
            lead(c, 0, P(0, 0), P(L - 12, 0));
            setBody(c);
            p->drawEllipse(P(L - 6, 0), 6.0, 6.0);
            text(c, P(L - 6, 0), t, 6);
            return;
        }
        const qreal w = kind == 21 ? 32.0 : 26.0;
        lead(c, 0, P(0, 0), P(L - w, 0));
        setBody(c);
        p->drawRect(QRectF(L - w, -9, w, 18));
        text(c, P(L - w / 2.0, 0), t, kind == 21 ? 8 : 9);
        return;
    }
    if (kind == 12) {                                        // щуп
        lead(c, 0, P(0, 0), P(L - 14, 0));
        setBody(c);
        p->drawEllipse(P(L - 7, 0), 7.0, 7.0);
        text(c, P(L - 7, 0), QStringLiteral("V"), 9);
        return;
    }

    lead(c, 0, P(0, 0), P(L - 2 * r, 0));
    setBody(c);
    const P o(L - r, 0);
    p->drawEllipse(o, r, r);
    QPolygonF w;
    switch (kind) {
    case 0:                                                  // шина постоянного напряжения
        plusSign(c, o, 4.5);
        break;
    case 1:                                                  // синус
        for (int t = -8; t <= 8; ++t)
            w << P(o.x() + t * 0.9, -4.0 * std::sin(t * kPi / 8.0));
        p->drawPolyline(w);
        break;
    case 2:                                                  // прямоугольные импульсы
        w << P(o.x() - 6, 3) << P(o.x() - 6, -4) << P(o.x(), -4) << P(o.x(), 4) << P(o.x() + 6, 4) << P(o.x() + 6, -1);
        p->drawPolyline(w);
        break;
    case 3:                                                  // тактовый генератор
        text(c, o, QStringLiteral("CK"), 8);
        break;
    case 4:                                                  // качающаяся частота
        for (int t = -14; t <= 14; ++t) {
            const qreal x = t * 0.45;
            w << P(o.x() + x, -4.0 * std::sin(0.45 * t + 0.045 * t * t));
        }
        p->drawPolyline(w);
        break;
    case 5:                                                  // регулируемый
        text(c, P(o.x() - 1, 0), QStringLiteral("V"), 9);
        line(c, P(o.x() - 6, 7), P(o.x() + 6, -7));
        arrowHead(c, P(o.x() + 8, -9), P(0.7071, -0.7071), 5);
        break;
    case 6:                                                  // шум
        w << P(o.x() - 7, 1) << P(o.x() - 5, -3) << P(o.x() - 3, 4) << P(o.x() - 1, -5)
          << P(o.x() + 1, 2) << P(o.x() + 3, -2) << P(o.x() + 5, 4) << P(o.x() + 7, -1);
        p->drawPolyline(w);
        break;
    case 7:  text(c, o, QStringLiteral("AM"), 8); break;
    case 8:  text(c, o, QStringLiteral("FM"), 8); break;
    default: break;
    }
}

// ------------------------------------------------------ управляемые источники

// kind: 0 ИНУН, 1 ИТУН, 2 ИНУТ, 3 ИТУТ.
void paintControlled(const Ctx &c, int kind)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const bool currentControl = (kind == 2 || kind == 3);
    const bool currentOutput = (kind == 1 || kind == 3);
    const qreal xa = cx - 14.0;                              // входной порт
    const qreal xb = cx + 14.0;                              // ромб
    const qreal rd = 16.0;

    leadPoly(c, 0, { P(0, -32), P(xa, -32) });
    leadPoly(c, 1, { P(0, 32),  P(xa, 32) });
    leadPoly(c, 2, { P(L, -32), P(xb, -32), P(xb, -rd) });
    leadPoly(c, 3, { P(L, 32),  P(xb, 32),  P(xb, rd)  });

    setBody(c);
    if (currentControl) {
        c.p->drawLine(P(xa, -32), P(xa, 32));                // входной порт замкнут (измерение тока)
        arrowHead(c, P(xa, 6), P(0, 1), 6);
    } else {
        c.p->drawLine(P(xa, -32), P(xa, -13));               // входной порт разомкнут (измерение напряжения)
        c.p->drawLine(P(xa, 32), P(xa, 13));
        c.p->drawEllipse(P(xa, -10.5), 2.5, 2.5);
        c.p->drawEllipse(P(xa, 10.5), 2.5, 2.5);
    }

    QPolygonF d;
    d << P(xb, -rd) << P(xb + rd, 0) << P(xb, rd) << P(xb - rd, 0);
    c.p->drawPolygon(d);
    if (currentOutput) {                                     // стрелка тока вверх, к «Вых +»
        c.p->drawLine(P(xb, 8), P(xb, -4));
        arrowHead(c, P(xb, -9), P(0, -1), 6);
    } else {
        plusSign(c, P(xb, -7), 3.2);
        minusSign(c, P(xb, 7), 3.2);
    }
}

// ----------------------------------------------------- пассивные и радио

void paintMemristor(const Ctx &c)
{
    const Span s = spanOf(c, 40);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -8, 40, 16));
    c.p->setBrush(c.body);
    c.p->drawRect(QRectF(s.x1 - 8, -8, 8, 16));              // залитая «сторона» — направление дрейфа
}

void lightArrows(const Ctx &c, const P &at, const P &dir)   // две стрелки света, направленные внутрь
{
    const P n(-dir.y(), dir.x());
    for (qreal k : { -4.5, 4.5 }) {
        const P a = at + n * k;
        line(c, a, a + dir * 6.0);
        arrowHead(c, a + dir * 11.0, dir, 5);
    }
}

void paintPhotoresistor(const Ctx &c)
{
    const Span s = spanOf(c, 40);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -8, 40, 16));
    lightArrows(c, P(s.cx - 10, -24), P(0.5, 0.866));
}

void paintThermistor(const Ctx &c)
{
    const Span s = spanOf(c, 40);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -8, 40, 16));
    QPolygonF w;
    w << P(s.cx - 20, 13) << P(s.cx - 12, 13) << P(s.cx + 12, -13);
    c.p->drawPolyline(w);
    text(c, P(s.cx + 18, -17), QStringLiteral("t°"), 10);
}

void paintVaristor(const Ctx &c)
{
    const Span s = spanOf(c, 40);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -8, 40, 16));
    c.p->drawLine(P(s.cx - 14, 13), P(s.cx + 14, -13));
    text(c, P(s.cx + 18, -17), QStringLiteral("U"), 10);
}

void paintSparkGap(const Ctx &c)
{
    const Span s = spanOf(c, 20);
    twoLeads(c, s);
    setBody(c);
    QPolygonF a, b;
    a << P(s.x0, -8) << P(s.cx - 2.5, 0) << P(s.x0, 8);
    b << P(s.x1, -8) << P(s.cx + 2.5, 0) << P(s.x1, 8);
    c.p->drawPolyline(a);
    c.p->drawPolyline(b);
    c.p->drawLine(P(s.x0, -8), P(s.x0, 8));
    c.p->drawLine(P(s.x1, -8), P(s.x1, 8));
}

void paintCrystal(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    twoLeads(c, s);
    line(c, P(s.x0, -11), P(s.x0, 11));
    line(c, P(s.x1, -11), P(s.x1, 11));
    setBody(c);
    c.p->drawRect(QRectF(s.cx - 3.5, -8, 7, 16));
}

void paintNeonLamp(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 12.0, 12.0);
    c.p->drawLine(P(s.cx - 4, -6), P(s.cx - 4, 6));          // электроды
    c.p->drawLine(P(s.cx + 4, -6), P(s.cx + 4, 6));
    c.p->setBrush(c.body);
    for (qreal x : { -8.0, 8.0 })
        c.p->drawEllipse(P(s.cx + x, 0), 1.2, 1.2);
}

void paintSpeaker(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -6, 9, 12));
    QPolygonF cone;
    cone << P(s.x0 + 9, -6) << P(s.x1, -13) << P(s.x1, 13) << P(s.x0 + 9, 6);
    c.p->drawPolygon(cone);
}

void paintMicrophone(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 12.0, 12.0);
    c.p->drawLine(P(s.cx + 4, -8), P(s.cx + 4, 8));          // мембрана
}

void paintScope(const Ctx &c)
{
    const Span s = spanOf(c, 32);
    twoLeads(c, s);
    setBody(c);
    c.p->drawEllipse(P(s.cx, 0), 16.0, 16.0);
    QPolygonF w;
    for (int t = -10; t <= 10; ++t)
        w << P(s.cx + t * 1.1, -5.0 * std::sin(t * kPi / 5.0));
    c.p->drawPolyline(w);
}

void paintDecimalDisplay(const Ctx &c)
{
    const Span s = spanOf(c, 36);
    twoLeads(c, s);
    setBody(c);
    c.p->drawRect(QRectF(s.x0, -10, 36, 20));
    text(c, P(s.cx, 0), QStringLiteral("8.8."), 11);
}

// ----------------------------------------------------- автотрансформатор

void paintAutoTransformer(const Ctx &c)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    leadPoly(c, 0, { P(0, -32), P(cx, -32), P(cx, -24) });
    leadPoly(c, 1, { P(L, 0),   P(cx, 0) });
    leadPoly(c, 2, { P(0, 32),  P(cx, 32),  P(cx, 24)  });
    setBody(c);
    for (int i = 0; i < 4; ++i)
        c.p->drawArc(QRectF(cx - 6, -24.0 + 12.0 * i, 12, 12), 90 * 16, 180 * 16);
    line(c, P(cx + 5, -24), P(cx + 5, 24));                   // магнитопровод
}

// ----------------------------------------------------- полупроводники

void paintVaractor(const Ctx &c)
{
    const Span s = spanOf(c, 20);
    twoLeads(c, s);
    setBody(c);
    const qreal xa = s.x1 - 6;
    QPolygonF tri;
    tri << P(s.x0, -9) << P(s.x0, 9) << P(xa, 0);
    c.p->drawPolygon(tri);
    c.p->drawLine(P(xa, -9), P(xa, 9));
    c.p->drawLine(P(s.x1, -9), P(s.x1, 9));
}

void paintTunnelDiode(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    c.p->drawLine(P(s.x1, -9), P(s.x1 - 4, -9));             // оба конца катода загнуты к аноду
    c.p->drawLine(P(s.x1, 9),  P(s.x1 - 4, 9));
}

void paintPhotodiode(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    lightArrows(c, P(s.cx - 6, -22), P(0.5, 0.866));
}

void paintDiac(const Ctx &c)
{
    const Span s = spanOf(c, 20);
    twoLeads(c, s);
    setBody(c);
    QPolygonF a, b;
    a << P(s.x0, -9) << P(s.x0, 9) << P(s.cx, 0);
    b << P(s.x1, -9) << P(s.x1, 9) << P(s.cx, 0);
    c.p->drawPolygon(a);
    c.p->drawPolygon(b);
}

void paintTriac(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    twoLeads(c, s);
    setBody(c);
    QPolygonF a, b;
    a << P(s.x0, -9) << P(s.x0, 9) << P(s.x1, 0);
    b << P(s.x1, -9) << P(s.x1, 9) << P(s.x0, 0);
    c.p->drawPolygon(a);
    c.p->drawPolygon(b);
    leadPoly(c, 2, { P(c.length, 32), P(s.x1, 32), P(s.x1, 9) });
}

void paintTl431(const Ctx &c)
{
    const Span s = spanOf(c, 16);
    diodeBody(c, s);
    c.p->drawLine(P(s.x1, -9), P(s.x1 - 4, -9));
    c.p->drawLine(P(s.x1, 9),  P(s.x1 + 4, 9));
    leadPoly(c, 2, { P(c.length, 32), P(s.x1 + 4, 32), P(s.x1 + 4, 9) });
}

void paintDarlington(const Ctx &c, bool pnp)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal s = pnp ? -1.0 : 1.0;                        // зеркально для p-n-p
    const qreal bx1 = cx - 18, bx2 = cx + 2, cc = cx + 12;

    lead(c, 0, P(0, 0), P(bx1, 0));
    leadPoly(c, 1, { P(L, -16 * s), P(cc, -16 * s), P(cc, -6 * s) });
    leadPoly(c, 2, { P(L, 16 * s), P(bx2 + 10, 16 * s) });

    setBody(c);
    auto Y = [&](qreal y) { return y * s; };
    c.p->drawLine(P(bx1, Y(-9)), P(bx1, Y(9)));              // база Q1
    c.p->drawLine(P(bx1, Y(-4)), P(bx1 + 8, Y(-12)));        // коллектор Q1
    c.p->drawLine(P(bx1 + 8, Y(-12)), P(cc, Y(-12)));
    c.p->drawLine(P(cc, Y(-16)), P(cc, Y(-6)));
    c.p->drawLine(P(bx1, Y(4)), P(bx1 + 8, Y(12)));          // эмиттер Q1 → база Q2
    c.p->drawLine(P(bx1 + 8, Y(12)), P(bx2, Y(12)));
    c.p->drawLine(P(bx2, Y(4)), P(bx2, Y(20)));              // база Q2
    c.p->drawLine(P(bx2, Y(8)), P(cc, Y(-6)));               // коллектор Q2
    c.p->drawLine(P(bx2, Y(16)), P(bx2 + 10, Y(16)));        // эмиттер Q2

    const P d1 = unit(P(8, Y(8)));
    const P d2 = unit(P(10, 0));
    if (pnp) {
        arrowHead(c, P(bx1, Y(4)) + d1 * 3.0, P(-d1.x(), -d1.y()), 6);
        arrowHead(c, P(bx2 + 3, Y(16)), P(-1, 0), 6);
    } else {
        arrowHead(c, P(bx1 + 8, Y(12)), d1, 6);
        arrowHead(c, P(bx2 + 10, Y(16)), d2, 6);
    }
}

void paintPhototransistor(const Ctx &c)
{
    paintBjt(c, false);
    const qreal cx = c.length / 2.0;
    lightArrows(c, P(cx - 30, -22), P(0.7071, 0.7071));
}

void paintIgbt(const Ctx &c)
{
    paintMosfet(c, true);
    const qreal cx = c.length / 2.0;
    arrowHead(c, P(cx + 8, 15), P(0, 1), 5);                 // стрелка эмиттера
}

void paintUjt(const Ctx &c)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal xb = cx - 4.0;
    lead(c, 0, P(0, 0), P(xb - 8, 0));
    leadPoly(c, 1, { P(L, -16), P(xb + 10, -16), P(xb + 3, -10) });   // база 2
    leadPoly(c, 2, { P(L, 16),  P(xb + 10, 16),  P(xb + 3, 10)  });   // база 1
    setBody(c);
    c.p->drawEllipse(P(cx, 0), 20.0, 20.0);
    c.p->drawLine(P(xb, -12), P(xb, 12));
    c.p->drawLine(P(xb - 8, 0), P(xb, 4));
    arrowHead(c, P(xb - 1, 3.6), unit(P(8, 4)), 6);          // эмиттер → канал
}

void paintTriode(const Ctx &c)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    leadPoly(c, 0, { P(0, 0), P(cx - 10, 0) });              // сетка
    leadPoly(c, 1, { P(L, -16), P(cx, -16), P(cx, -9) });    // анод
    leadPoly(c, 2, { P(L, 16),  P(cx, 16),  P(cx, 9)  });    // катод
    setBody(c);
    c.p->drawEllipse(P(cx, 0), 20.0, 20.0);
    c.p->drawLine(P(cx - 9, -9), P(cx + 9, -9));             // анод
    c.p->drawLine(P(cx + 9, 9),  P(cx - 9, 9));              // катод
    c.p->setPen(QPen(c.body, c.width, Qt::DashLine, Qt::FlatCap));
    c.p->drawLine(P(cx - 10, 0), P(cx + 10, 0));             // сетка (пунктир)
}

void diodeAlong(const Ctx &c, const P &a, const P &b)
{
    const P m((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0);
    const P d = unit(b - a);
    const P n(-d.y(), d.x());
    setBody(c);
    c.p->drawLine(a, m - d * 5.0);
    c.p->drawLine(m + d * 5.0, b);
    QPolygonF tri;
    tri << (m - d * 5.0 + n * 5.5) << (m - d * 5.0 - n * 5.5) << (m + d * 5.0);
    c.p->drawPolygon(tri);
    c.p->drawLine(m + d * 5.0 + n * 5.5, m + d * 5.0 - n * 5.5);
}

void paintDiodeBridge(const Ctx &c)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const P left(cx - 32, 0), right(cx + 32, 0), top(cx, -32), bottom(cx, 32);
    lead(c, 0, P(0, 0), left);
    lead(c, 1, P(L, 0), right);
    lead(c, 2, P(cx, -48), top);
    lead(c, 3, P(cx, 48), bottom);
    diodeAlong(c, left, top);
    diodeAlong(c, right, top);
    diodeAlong(c, bottom, left);
    diodeAlong(c, bottom, right);
    plusSign(c, P(cx + 9, -41), 3);
    minusSign(c, P(cx + 9, 41), 3);
}

// ----------------------------------------------------- коммутация

// Один контакт-рычаг на высоте y.
void leverAt(const Ctx &c, const Span &s, qreal y, bool closed)
{
    constexpr qreal r = 2.2;
    setBody(c);
    const P pivot(s.x0, y);
    const P tip = closed ? P(s.x1, y)
                         : P(s.x0 + 22.0 * std::cos(25.0 * kPi / 180.0), y - 22.0 * std::sin(25.0 * kPi / 180.0));
    c.p->drawLine(pivot, tip);
    c.p->drawEllipse(pivot, r, r);
    c.p->drawEllipse(P(s.x1, y), r, r);
}

void paintSwitchDPST(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    const bool closed = c.state != 0;
    for (int pole = 0; pole < 2; ++pole) {
        const qreal y = pole == 0 ? -32.0 : 32.0;
        lead(c, pole * 2, P(0, y), P(s.x0, y));
        lead(c, pole * 2 + 1, P(c.length, y), P(s.x1, y));
        leverAt(c, s, y, closed);
    }
    c.p->setPen(QPen(c.body, c.width * 0.8, Qt::DashLine, Qt::FlatCap));
    c.p->drawLine(P(s.cx, closed ? -30 : -40), P(s.cx, closed ? 30 : 24));    // механическая связь
}

// Рычаг SPDT: общий вывод на yc, верхний контакт yTop, нижний yBottom.
void spdtPole(const Ctx &c, const Span &s, qreal yc, qreal yTop, qreal yBottom,
              int pinCommon, int pinTop, int pinBottom, int position)
{
    constexpr qreal r = 2.2;
    lead(c, pinCommon, P(0, yc), P(s.x0, yc));
    lead(c, pinTop, P(c.length, yTop), P(s.x1, yTop));
    lead(c, pinBottom, P(c.length, yBottom), P(s.x1, yBottom));
    setBody(c);
    const P pivot(s.x0, yc);
    c.p->drawLine(pivot, P(s.x1, position == 0 ? yTop : yBottom));
    c.p->drawEllipse(pivot, r, r);
    c.p->drawEllipse(P(s.x1, yTop), r, r);
    c.p->drawEllipse(P(s.x1, yBottom), r, r);
}

void paintSwitchDPDT(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    const int pos = c.state != 0 ? 1 : 0;
    spdtPole(c, s, -48, -64, -32, 0, 1, 2, pos);
    spdtPole(c, s, 48, 32, 64, 3, 4, 5, pos);
    c.p->setPen(QPen(c.body, c.width * 0.8, Qt::DashLine, Qt::FlatCap));
    c.p->drawLine(P(s.x0 + 6, -44), P(s.x0 + 6, 44));
}

void paintSwitchSP3T(const Ctx &c)
{
    const Span s = spanOf(c, 24);
    constexpr qreal r = 2.2;
    lead(c, 0, P(0, 0), P(s.x0, 0));
    const qreal ys[3] = { -32.0, 0.0, 32.0 };
    for (int i = 0; i < 3; ++i) {
        lead(c, i + 1, P(c.length, ys[i]), P(s.x1, ys[i]));
        setBody(c);
        c.p->drawEllipse(P(s.x1, ys[i]), r, r);
    }
    setBody(c);
    const int pos = std::clamp(c.state, 0, 2);
    c.p->drawLine(P(s.x0, 0), P(s.x1, ys[pos]));
    c.p->drawEllipse(P(s.x0, 0), r, r);
}

// ----------------------------------------------------- логика

void paintSchmitt(const Ctx &c, bool inverted)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal extra = inverted ? 7.0 : 0.0;
    const qreal x0 = cx - (28.0 + extra) / 2.0;
    const qreal x1 = x0 + 28.0;
    lead(c, 0, P(0, 0), P(x0, 0));
    lead(c, 1, P(L, 0), P(x1 + extra, 0));
    setBody(c);
    c.p->drawRect(QRectF(x0, -14, 28, 28));
    if (inverted)
        c.p->drawEllipse(P(x1 + 3.5, 0), 3.5, 3.5);
    // петля гистерезиса
    QPolygonF w;
    const qreal mx = x0 + 14;
    w << P(mx - 7, 5) << P(mx + 3, 5) << P(mx + 3, -5) << P(mx + 7, -5);
    c.p->drawPolyline(w);
    QPolygonF w2;
    w2 << P(mx - 7, 5) << P(mx - 3, 5) << P(mx - 3, -5) << P(mx + 7, -5);
    c.p->drawPolyline(w2);
}

void paintBuffer(const Ctx &c)
{
    const qreal L = c.length;
    const qreal cx = L / 2.0;
    const qreal x0 = cx - 14.0;
    lead(c, 0, P(0, 0), P(x0, 0));
    lead(c, 1, P(L, 0), P(x0 + 28, 0));
    setBody(c);
    c.p->drawRect(QRectF(x0, -14, 28, 28));
    text(c, P(cx, 0), QStringLiteral("1"), 18);
}
