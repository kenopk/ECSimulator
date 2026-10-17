#pragma once
// ============================================================================
//  govno::core — контракт типов элементов.
//
//  Заголовок БЕЗ зависимостей от Qt: только стандартная библиотека.
//  Его используют и ядро (расчёт, сериализация), и gui (меню элементов,
//  панель свойств, диалог параметров).
//
//  Как добавить новый элемент:
//    1) добавить значение в enum ElementType (перед Count_);
//    2) добавить ОДНУ запись в allElements() — в том же порядке, что и enum;
//    3) (gui) описать выводы и символ в items/SymbolPainter.cpp.
//  Порядок выводов (pinNames) — это контракт между gui и ядром:
//  pin[i] в gui соответствует pinNames[i] здесь.
// ============================================================================
#include <cstddef>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace govno::core {

enum class Category : int {
    Wiring = 0,        // провода, земля, метки
    Passive,           // пассивные элементы
    Sources,           // источники и генераторы
    Controlled,        // управляемые источники
    Semiconductors,    // полупроводники
    Switches,          // коммутация и реле
    Analog,            // усилители и аналоговые микросхемы
    Logic,             // логические элементы
    Digital,           // цифровые микросхемы
    Memory,            // память и матрицы
    Meters,            // измерительные приборы
    Display,           // индикация
    Radio,             // радиоэлементы
    Modules,           // готовые модули (дисплеи, клавиатуры, DIP-переключатели)
    Count_
};

inline const char* categoryName(Category c)
{
    switch (c) {
    case Category::Wiring:         return "Провода, земля, метки, оформление";
    case Category::Passive:        return "Пассивные элементы";
    case Category::Sources:        return "Источники и генераторы";
    case Category::Controlled:     return "Управляемые источники";
    case Category::Semiconductors: return "Полупроводники";
    case Category::Switches:       return "Коммутация и реле";
    case Category::Analog:         return "Усилители и аналоговые микросхемы";
    case Category::Logic:          return "Логические элементы";
    case Category::Digital:        return "Цифровые микросхемы";
    case Category::Memory:         return "Память и матрицы";
    case Category::Meters:         return "Измерительные приборы";
    case Category::Display:        return "Индикация";
    case Category::Radio:          return "Радиоэлементы";
    case Category::Modules:        return "Модули и платы";
    default:                       return "";
    }
}

enum class ElementType : int {
    // провода и земля
    Wire = 0,
    Ground,
    Chassis,
    // пассивные
    Resistor,
    VariableResistor,
    Potentiometer,
    Capacitor,
    PolarCapacitor,
    VariableCapacitor,
    Inductor,
    Choke,
    Transformer,
    Fuse,
    Lamp,
    // источники
    VoltageSourceDC,
    Battery,
    VoltageSourceAC,
    Generator,
    CurrentSource,
    // полупроводники
    Diode,
    ZenerDiode,
    SchottkyDiode,
    LED,
    NPN,
    PNP,
    NMOS,
    PMOS,
    NJFET,
    PJFET,
    Thyristor,
    // коммутация
    Switch,
    PushButton,
    SwitchSPDT,
    // усилители
    OpAmp,
    // логика
    GateAnd,
    GateOr,
    GateNot,
    GateNand,
    GateNor,
    GateXor,
    // приборы
    Voltmeter,
    Ammeter,
    // ---- расширение набора (Falstad + радиоэлементы)
    NodeLabel,
    Memristor,
    Photoresistor,
    Thermistor,
    Varistor,
    SparkGap,
    InductorVar,
    AutoTransformer,
    TransformerTapped,
    CoupledInductors,
    RailDC,
    RailAC,
    RailSquare,
    RailClock,
    RailSweep,
    RailVariable,
    RailNoise,
    RailAM,
    RailFM,
    LogicInput,
    VCVS,
    VCCS,
    CCVS,
    CCCS,
    Varactor,
    TunnelDiode,
    Diac,
    Triac,
    DarlingtonNPN,
    DarlingtonPNP,
    Photodiode,
    Phototransistor,
    IGBT,
    UJT,
    TL431,
    PushButtonNC,
    SwitchMBB,
    SwitchSP3T,
    SwitchDPST,
    SwitchDPDT,
    Relay,
    AnalogSwitch,
    OpAmpPower,
    Comparator,
    OTA,
    CCII,
    Regulator,
    Timer555,
    VCO,
    PLL,
    PhaseComparator,
    SampleHold,
    GateXnor,
    GateBuffer,
    SchmittTrigger,
    SchmittInverting,
    TriStateBuffer,
    DFlipFlop,
    DFlipFlopSR,
    JKFlipFlop,
    TFlipFlop,
    SRLatch,
    DLatch,
    DecadeCounter,
    HalfAdder,
    FullAdder,
    Decoder7Seg,
    Monostable,
    SeqGen,
    DelayLine,
    RAM,
    ROM,
    DiodeMatrix,
    SegDisplay,
    LedMatrix,
    LedRGB,
    DecimalDisplay,
    LogicOutput,
    Ohmmeter,
    Wattmeter,
    Scope,
    Probe,
    Crystal,
    Antenna,
    Speaker,
    Microphone,
    Triode,
    Optocoupler,
    DiodeBridge,
    NeonLamp,
    Motor,
    TransmissionLine,
    TextLabel,
    Box,
    Line,
    TransformerCustom,
    RelayCoil,
    RelayContact,
    CrossSwitch,
    Gyrator,
    AudioInput,
    DataInput,
    ExtVoltage,
    AnalogOutput,
    TestPoint,
    InstructionDisplay,
    DataExport,
    AudioOutput,
    StopTrigger,
    Motor3Phase,
    WattmeterOld,
    AnalogSwitchSPDT,
    NortonAmp,
    TimeDelayRelay,
    LM317,
    MotorProtection,
    Mux,
    Demux,
    Decoder,
    Encoder,
    Register,
    Shift,
    PISO,
    Counter,
    CounterLoad,
    RingCounter,
    Adder,
    CodeComparator,
    BusTransceiver,
    BusSplitter,
    BusInput,
    LUT,
    ADC,
    DAC,
    AnalogMux,
    LCD,
    Keypad,
    DipSwitch,
    KarnaughMap,

    Count_
};

// Описание одного параметра элемента (значения — в СИ).
// Если choices != nullptr, параметр — перечисление: значение это индекс
// варианта в строке choices вида "Разомкнут|Замкнут".
struct ParamDef {
    const char* key;      // ключ в карте параметров
    const char* label;    // подпись (UTF-8)
    const char* unit;     // "Ω","F","H","V","A","Hz","W","%","°" или "" (безразмерный)
    double defaultValue;
    double minValue;
    double maxValue;
    const char* choices = nullptr;
    bool integer = false;                  // целое число: поле округляется
    bool color = false;                    // цвет: значение — RGB как целое 0xRRGGBB, в форме — кнопка выбора цвета
};

// Элементы, у которых число выводов зависит от параметров (вентили — число входов,
// память — объём, матрицы — размер, индикатор — тип). См. variantOf()/layoutFor().
enum class Dynamic : int { None = 0, GateInputs, Memory, DiodeMatrix, SegDisplay, LedMatrix, Bits, Keypad, Lcd, Dip, Karnaugh };

struct ElementInfo {
    ElementType type;
    const char* id;                          // стабильный идентификатор (для файлов)
    const char* name;                        // отображаемое имя (UTF-8)
    Category category;
    std::vector<const char*> pinNames;       // имена выводов в порядке gui/ядра
    std::vector<ParamDef> params;
    // Для микросхем: раскладка корпуса "ЗАГОЛОВОК;левые,выводы|правые,выводы".
    // Перед именем вывода: '>' — вход синхронизации, '/' — инверсия (кружок).
    // Заголовок, начинающийся с '@', выбирает особый рисунок (@relay, @seg7 ...).
    const char* chip = nullptr;
    Dynamic dynamic = Dynamic::None;

    int pinCount() const { return static_cast<int>(pinNames.size()); }
};

// -------------------------------------------------------------- микросхемы
struct ChipPin {
    std::string name;
    bool left = true;
    bool clock = false;
    bool inverted = false;
    int edge = 0;                  // сторона: 0 — слева, 1 — справа, 2 — сверху, 3 — снизу
};

struct ChipLayout {
    std::string title;
    std::vector<ChipPin> pins;     // сначала левые сверху вниз, затем правые
    int leftCount = 0;
    int rightCount = 0;
    int topCount = 0;
    int bottomCount = 0;
};

inline ChipLayout parseChipLayout(const char* text)
{
    ChipLayout out;
    std::string str(text ? text : "");
    const std::size_t semi = str.find(';');
    if (semi != std::string::npos) {
        out.title = str.substr(0, semi);
        str = str.substr(semi + 1);
    }
    int edge = 0;                  // секции через «|»: слева | справа | сверху | снизу
    std::string tok;
    auto flush = [&]() {
        if (tok.empty())
            return;
        ChipPin p;
        p.left = edge == 0;
        p.edge = edge;
        std::size_t i = 0;
        while (i < tok.size() && (tok[i] == '>' || tok[i] == '/')) {
            (tok[i] == '>' ? p.clock : p.inverted) = true;
            ++i;
        }
        p.name = tok.substr(i);
        out.pins.push_back(p);
        (edge == 0 ? out.leftCount : edge == 1 ? out.rightCount : edge == 2 ? out.topCount : out.bottomCount)++;
        tok.clear();
    };
    for (char ch : str) {
        if (ch == ',') {
            flush();
        } else if (ch == '|') {
            flush();
            if (edge < 3)
                ++edge;
        } else {
            tok += ch;
        }
    }
    flush();
    return out;
}

// Имя вывода для показа: у инверсных выводов каждая буква надчёркнута.
inline std::string chipPinDisplayName(const ChipPin& pin)
{
    if (!pin.inverted)
        return pin.name;
    std::string n;
    for (std::size_t i = 0; i < pin.name.size(); ++i) {
        n += pin.name[i];
        const bool lastByteOfChar = i + 1 >= pin.name.size()
            || (static_cast<unsigned char>(pin.name[i + 1]) & 0xC0) != 0x80;
        if (lastByteOfChar)
            n += "\xCC\x85";
    }
    return n;
}

// Запись реестра для микросхемы: имена выводов берутся из раскладки.
inline ElementInfo chipInfo(ElementType type, const char* id, const char* name, Category cat,
                            const char* layout, std::vector<ParamDef> params,
                            Dynamic dynamic = Dynamic::None)
{
    static std::deque<std::string> pool;     // стабильное хранилище имён выводов
    ElementInfo info{ type, id, name, cat, {}, std::move(params), layout, dynamic };
    for (const ChipPin& pin : parseChipLayout(layout).pins) {
        pool.push_back(chipPinDisplayName(pin));
        info.pinNames.push_back(pool.back().c_str());
    }
    return info;
}

// ------------------------------------------- элементы с переменным числом выводов
using ParamGetter = std::function<double(const char*)>;     // значение параметра по ключу

inline int clampInt(double v, int lo, int hi)
{
    const int i = static_cast<int>(std::lround(v));
    return i < lo ? lo : (i > hi ? hi : i);
}

// «Вариант» — число, однозначно задающее раскладку выводов. 0 — исходная раскладка из реестра.
inline unsigned variantOf(const ElementInfo& info, const ParamGetter& get)
{
    switch (info.dynamic) {
    case Dynamic::GateInputs:
        return static_cast<unsigned>(clampInt(get("inputs"), 2, 63));
    case Dynamic::Memory:         // адресных бит (1…16) | разрядность (1…16) << 8
        return static_cast<unsigned>(clampInt(get("addrBits"), 0, 15) + 1)
             | static_cast<unsigned>(clampInt(get("dataBits"), 1, 16)) << 8;
    case Dynamic::DiodeMatrix:    // входов | выходов << 8 | рисунок << 16
        return static_cast<unsigned>(clampInt(get("inputs"), 1, 16))
             | static_cast<unsigned>(clampInt(get("outputs"), 1, 16)) << 8
             | static_cast<unsigned>(clampInt(get("preset"), 0, 3)) << 16;
    case Dynamic::SegDisplay:     // тип (сегментов + 1) | число разрядов << 8
        return (static_cast<unsigned>(clampInt(get("segments"), 0, 5)) + 1)
             | static_cast<unsigned>(clampInt(get("digits"), 1, 16)) << 8;
    case Dynamic::LedMatrix:      // строк | столбцов << 8 | «по порядку» << 16 (иначе — как у реальной матрицы 8×8)
        return static_cast<unsigned>(clampInt(get("rows"), 1, 32))
             | static_cast<unsigned>(clampInt(get("cols"), 1, 32)) << 8
             | (get("pinout") > 0.5 ? 1u << 16 : 0u);
    case Dynamic::Bits: {         // разрядность (или число адресных/входных линий) — параметр "bits"
        int lo = 1, hi = 32;
        for (const ParamDef& pd : info.params)
            if (std::strcmp(pd.key, "bits") == 0) {
                lo = static_cast<int>(pd.minValue);
                hi = static_cast<int>(pd.maxValue);
            }
        return static_cast<unsigned>(clampInt(get("bits"), lo, hi));
    }
    case Dynamic::Keypad:         // строк | столбцов << 8
        return static_cast<unsigned>(clampInt(get("rows"), 1, 32))
             | static_cast<unsigned>(clampInt(get("cols"), 1, 32)) << 8;
    case Dynamic::Lcd:            // тип индикатора + 1 (см. lcdSize)
        return static_cast<unsigned>(clampInt(get("size"), 0, 7)) + 1;
    case Dynamic::Dip:            // число позиций
        return static_cast<unsigned>(clampInt(get("positions"), 1, 32));
    case Dynamic::Karnaugh:       // столбцов | строк << 8 | «без выводов» << 16
        return static_cast<unsigned>(clampInt(get("width"), 1, 16))
             | static_cast<unsigned>(clampInt(get("height"), 1, 16)) << 8
             | (get("pins") > 0.5 ? 1u << 16 : 0u);
    default:
        return 0;
    }
}

// Размер знакоместа ЖК-индикатора по типу (variant − 1): { столбцов, строк }.
inline void lcdSize(unsigned variant, int* cols, int* rows)
{
    static const int kSizes[8][2] = { { 16, 2 }, { 16, 1 }, { 8, 2 }, { 20, 2 }, { 20, 4 }, { 16, 4 }, { 40, 2 }, { 40, 4 } };
    const unsigned i = variant >= 1 && variant <= 8 ? variant - 1 : 0;
    *cols = kSizes[i][0];
    *rows = kSizes[i][1];
}

// Раскладка выводов типовых «шинных» микросхем по разрядности n.
inline std::string bitsLayoutString(ElementType t, int n)
{
    using T = ElementType;
    const auto seq = [](const char* prefix, int count) {
        std::string s;
        for (int i = 0; i < count; ++i) {
            if (i)
                s += ',';
            s += prefix + std::to_string(i);
        }
        return s;
    };
    const int w = 1 << n;
    switch (t) {
    case T::Mux:            return "MUX;" + seq("D", w) + "|Y||" + seq("S", n);
    case T::Demux:          return "DMX;D|" + seq("Y", w) + "||" + seq("S", n);
    case T::Decoder:        return "DC;" + seq("A", n) + "|" + seq("Y", w) + "||EN";
    case T::Encoder:        return "CD;" + seq("I", w) + "|" + seq("A", n) + ",V";
    case T::Register:       return "RG;" + seq("D", n) + ",>C|" + seq("Q", n);
    case T::Shift:          return "SRG;D,>C,R|" + seq("Q", n);
    case T::PISO:           return "PISO;" + seq("D", n) + ",LD,>C|Q";
    case T::Counter:        return "CT;>C,EN,R|" + seq("Q", n);
    case T::CounterLoad:    return "CT;>C,EN,R,LD," + seq("D", n) + "|" + seq("Q", n) + ",CO";
    case T::RingCounter:    return "CT;>C,R|" + seq("Q", n);
    case T::Adder:          return "SM;" + seq("A", n) + "," + seq("B", n) + ",CI|" + seq("S", n) + ",CO";
    case T::CodeComparator: return "CMP;" + seq("A", n) + "," + seq("B", n) + "|A>B,A=B,A<B";
    case T::BusTransceiver: return "BUF;" + seq("A", n) + "|" + seq("B", n) + "||DIR,/OE";
    case T::BusSplitter:    return "BUS;BUS|" + seq("D", n);
    case T::BusInput:       return "BUS;|" + seq("Q", n);
    case T::ADC:            return "A/D;Вх|" + seq("D", n);
    case T::DAC:            return "D/A;" + seq("D", n) + "|Вых";
    case T::AnalogMux:      return "MUX;" + seq("I", w) + "|Y||" + seq("S", n);
    case T::LUT:            return "LUT;" + seq("I", n) + "|O";
    default:                return "";
    }
}

inline ChipLayout layoutFor(const ElementInfo& info, unsigned variant)
{
    ChipLayout out;
    auto addE = [&](int edge, const std::string& name, bool inverted = false) {
        ChipPin p;
        p.edge = edge;
        p.left = edge == 0;
        p.name = name;
        p.inverted = inverted;
        out.pins.push_back(p);
        (edge == 0 ? out.leftCount : edge == 1 ? out.rightCount : edge == 2 ? out.topCount : out.bottomCount)++;
    };
    auto add = [&](bool left, const std::string& name, bool inverted = false) {
        addE(left ? 0 : 1, name, inverted);
    };
    auto seq = [&](bool left, const char* prefix, int n) {
        for (int i = 0; i < n; ++i)
            add(left, std::string(prefix) + std::to_string(i));
    };
    auto words = [](int addrBits) {
        const long w = 1L << addrBits;
        return w >= 1024 ? std::to_string(w / 1024) + "K" : std::to_string(w);
    };

    switch (info.dynamic) {
    case Dynamic::GateInputs: {
        const int n = variant ? static_cast<int>(variant) : 2;
        for (int i = 0; i < n; ++i)
            add(true, n <= 26 ? std::string("Вход ") + static_cast<char>('A' + i)
                              : "Вход " + std::to_string(i + 1));
        add(false, "Выход");
        return out;
    }
    case Dynamic::Memory:
        if (variant) {
            const int a = static_cast<int>(variant & 0xFF);
            const int d = static_cast<int>((variant >> 8) & 0xFF);
            const bool ram = info.type == ElementType::RAM;
            out.title = std::string(ram ? "RAM " : "ROM ") + words(a) + "×" + std::to_string(d);
            seq(true, "A", a);
            add(true, "CS", true);
            if (ram) {
                add(true, "WE", true);
                add(true, "OE", true);
            }
            seq(false, "D", d);
            return out;
        }
        break;
    case Dynamic::DiodeMatrix:
        if (variant) {
            out.title = "@dmat";
            seq(true, "I", static_cast<int>(variant & 0xFF));
            seq(false, "O", static_cast<int>((variant >> 8) & 0xFF));
            return out;
        }
        break;
    case Dynamic::LedMatrix:
        if (variant) {
            out.title = "@ledm";
            const int rows = static_cast<int>(variant & 0xFF), cols = static_cast<int>((variant >> 8) & 0xFF);
            const bool ordered = ((variant >> 16) & 1u) != 0;
            if (rows == 8 && cols == 8 && !ordered) {
                // Как у реальной матрицы 8×8 (16 выводов), вид сверху: выводы 16…9 слева направо — сверху,
                // 1…8 слева направо — снизу. Названия: R — строка, C — столбец.
                static const char* const top[] = { "C8", "C7", "R2", "C1", "R4", "C6", "C4", "R1" };
                static const char* const bottom[] = { "R5", "R7", "C2", "C3", "R8", "C5", "R6", "R3" };
                for (const char* n : top)
                    addE(2, n);
                for (const char* n : bottom)
                    addE(3, n);
            } else {                                   // по порядку: столбцы сверху, строки снизу
                for (int i = 0; i < cols; ++i)
                    addE(2, "C" + std::to_string(i + 1));
                for (int i = 0; i < rows; ++i)
                    addE(3, "R" + std::to_string(i + 1));
            }
            return out;
        }
        break;
    case Dynamic::SegDisplay:
        if (variant) {
            // Выводы идут по двум длинным сторонам корпуса, как ножки реального индикатора.
            // Несколько разрядов — общие сегментные выводы и по одному общему выводу D1…Dn на разряд.
            // Виды (variant & 0xFF): 1 — 3 сегмента (цифра «1» и знак «−»), 2 — 5, 3 — 7, 4 — 9 (семь и две диагонали),
            // 5 — 14 («звезда»), 6 — 16 сегментов.
            static const char* const k14[] = { "a", "b", "c", "d", "e", "f", "g1", "g2", "h", "j", "k", "l", "m", "n" };
            static const char* const k16[] = { "a1", "a2", "b", "c", "d1", "d2", "e", "f", "g1", "g2",
                                               "h", "i", "j", "k", "l", "m" };
            static const char* const k9[]  = { "a", "b", "c", "d", "e", "f", "g", "h", "i" };
            static const char* const k5[]  = { "a", "b", "c", "d", "g" };
            static const char* const k3[]  = { "b", "c", "g" };
            const unsigned kind = variant & 0xFF;
            const int digits = std::max(1, static_cast<int>((variant >> 8) & 0xFF));
            std::vector<std::string> names;
            bool withDp = true;
            switch (kind) {
            case 1:  out.title = "@seg3";  names.assign(std::begin(k3), std::end(k3)); withDp = false; break;
            case 2:  out.title = "@seg5";  names.assign(std::begin(k5), std::end(k5)); break;
            case 4:  out.title = "@seg9";  names.assign(std::begin(k9), std::end(k9)); break;
            case 5:  out.title = "@seg14"; names.assign(std::begin(k14), std::end(k14)); break;
            case 6:  out.title = "@seg16"; names.assign(std::begin(k16), std::end(k16)); break;
            default: out.title = "@seg7";  break;
            }
            if (digits == 1 && names.empty()) {
                // Как у реального 10-выводного индикатора (5161AS и аналоги), вид сверху.
                // Сверху слева направо (выводы 10…6): g, f, общий, a, b. Снизу слева направо (выводы 1…5): e, d, общий, c, DP.
                // Два общих вывода внутри корпуса соединены между собой.
                addE(2, "g"); addE(2, "f"); addE(2, "K"); addE(2, "a"); addE(2, "b");
                addE(3, "e"); addE(3, "d"); addE(3, "K"); addE(3, "c"); addE(3, "DP");
            } else if (digits == 1) {                     // остальные: сегменты, затем DP и общий — поровну сверху и снизу
                if (withDp)
                    names.push_back("DP");
                names.push_back("K");
                const std::size_t top = (names.size() + 1) / 2;
                for (std::size_t i = 0; i < names.size(); ++i)
                    addE(i < top ? 2 : 3, names[i]);
            } else if (names.empty()) {
                // Как у реального 4-разрядного индикатора на 12 выводов: сверху D1, a, f, D2, D3, b, снизу e, d, DP, c, g, D4.
                // Для другого числа разрядов средние общие выводы идут сверху, последний — снизу.
                addE(2, "D1"); addE(2, "a"); addE(2, "f");
                for (int d = 2; d <= digits - 1; ++d)
                    addE(2, "D" + std::to_string(d));
                addE(2, "b");
                addE(3, "e"); addE(3, "d"); addE(3, "DP"); addE(3, "c"); addE(3, "g");
                addE(3, "D" + std::to_string(digits));
            } else {
                if (withDp)
                    names.push_back("DP");
                const std::size_t top = (names.size() + 1) / 2;
                for (std::size_t i = 0; i < names.size(); ++i)
                    addE(i < top ? 2 : 3, names[i]);
                for (int d = 1; d <= digits; ++d)
                    addE(d <= (digits + 1) / 2 ? 2 : 3, "D" + std::to_string(d));
            }
            return out;
        }
        break;
    case Dynamic::Bits:
        if (variant)
            return parseChipLayout(bitsLayoutString(info.type, static_cast<int>(variant)).c_str());
        break;
    case Dynamic::Keypad:
        if (variant) {
            out.title = "@keypad";
            for (unsigned i = 1; i <= (variant & 0xFF); ++i)
                add(true, "R" + std::to_string(i));
            for (unsigned i = 1; i <= ((variant >> 8) & 0xFF); ++i)
                addE(2, "C" + std::to_string(i));
            return out;
        }
        break;
    case Dynamic::Lcd:
        if (variant) {
            out.title = "@lcd";
            for (const char* n : { "VSS", "VDD", "V0", "RS", "RW", "E", "A", "K" })
                addE(2, n);
            for (int i = 0; i < 8; ++i)
                addE(3, "D" + std::to_string(i));
            return out;
        }
        break;
    case Dynamic::Karnaugh:
        if (variant) {
            // Карта Карно как таблица истинности: переменные A, B, … слева (сначала по строкам, затем по столбцам),
            // выход F справа. Клетки (0 / 1 / X) хранит сам элемент: их число — ширина × высота.
            out.title = "@kmap";
            if ((variant >> 16) & 1u)            // «без выводов»: карта служит только таблицей для кликов
                return out;
            const auto bits = [](unsigned n) { int b = 0; while ((1u << b) < n) ++b; return b; };
            const int vars = bits(variant & 0xFF) + bits((variant >> 8) & 0xFF);
            for (int i = 0; i < vars; ++i)
                add(true, std::string(1, static_cast<char>('A' + i)));
            add(false, "F");
            return out;
        }
        break;
    case Dynamic::Dip:
        if (variant) {
            out.title = "@dip";
            for (unsigned i = 1; i <= variant; ++i)
                addE(2, std::to_string(i) + "A");
            for (unsigned i = 1; i <= variant; ++i)
                addE(3, std::to_string(i) + "B");
            return out;
        }
        break;
    default:
        break;
    }
    return parseChipLayout(info.chip);
}

inline int pinCountFor(const ElementInfo& info, unsigned variant)
{
    if (info.dynamic == Dynamic::None)
        return info.pinCount();
    return static_cast<int>(layoutFor(info, variant).pins.size());
}

inline std::vector<std::string> pinNamesFor(const ElementInfo& info, unsigned variant)
{
    std::vector<std::string> names;
    if (info.dynamic == Dynamic::None) {
        for (const char* n : info.pinNames)
            names.emplace_back(n);
        return names;
    }
    for (const ChipPin& p : layoutFor(info, variant).pins)
        names.push_back(chipPinDisplayName(p));
    return names;
}

inline const std::vector<ElementInfo>& allElements()
{
    using T = ElementType;
    using C = Category;
    static const std::vector<ElementInfo> kAll = {
        // ------------------------------------------------ провода и земля
        { T::Wire,    "wire",    "Провод",                  C::Wiring, { "A", "B" }, {} },
        { T::Ground,  "ground",  "Земля",                   C::Wiring, { "G" },      {} },
        { T::Chassis, "chassis", "Корпус (шасси)",          C::Wiring, { "G" },      {} },

        // ------------------------------------------------------ пассивные
        { T::Resistor, "resistor", "Резистор", C::Passive, { "1", "2" }, {
            { "resistance", "Сопротивление", "Ω", 1000.0, 1e-3, 1e12 },
            { "power",      "Мощность",      "W", 0.25,   0.0,  1e6  },
            { "tolerance",  "Допуск",        "%", 5.0,    0.0,  100.0 },
        } },

        { T::VariableResistor, "resistor_var", "Резистор переменный (реостат)", C::Passive, { "1", "2" }, {
            { "resistance", "Сопротивление полное", "Ω", 10e3, 1e-3, 1e12 },
            { "position",   "Положение движка",     "%", 50.0, 0.0,  100.0 },
            { "power",      "Мощность",             "W", 0.5,  0.0,  1e6   },
        } },

        { T::Potentiometer, "potentiometer", "Потенциометр", C::Passive, { "1", "2", "Движок" }, {
            { "resistance", "Сопротивление полное", "Ω", 10e3, 1e-3, 1e12 },
            { "position",   "Положение движка",     "%", 50.0, 0.0,  100.0 },
            { "power",      "Мощность",             "W", 0.25, 0.0,  1e6   },
        } },

        { T::Capacitor, "capacitor", "Конденсатор", C::Passive, { "1", "2" }, {
            { "capacitance", "Ёмкость",          "F", 10e-6, 1e-15, 1e3   },
            { "voltage",     "Макс. напряжение", "V", 50.0,  0.0,   1e6   },
            { "tolerance",   "Допуск",           "%", 10.0,  0.0,   100.0 },
            { "esr",         "ESR",              "Ω", 0.1,   0.0,   1e6   },
        } },

        { T::PolarCapacitor, "capacitor_polar", "Конденсатор электролитический", C::Passive, { "+", "−" }, {
            { "capacitance", "Ёмкость",          "F", 100e-6, 1e-15, 1e3   },
            { "voltage",     "Макс. напряжение", "V", 25.0,   0.0,   1e6   },
            { "tolerance",   "Допуск",           "%", 20.0,   0.0,   100.0 },
            { "esr",         "ESR",              "Ω", 0.5,    0.0,   1e6   },
            { "leakage",     "Ток утечки",       "A", 5e-6,   0.0,   1.0   },
        } },

        { T::VariableCapacitor, "capacitor_var", "Конденсатор переменной ёмкости", C::Passive, { "1", "2" }, {
            { "capacitance", "Ёмкость максимальная", "F", 100e-12, 1e-15, 1e3 },
            { "position",    "Положение ротора",     "%", 50.0,    0.0,   100.0 },
            { "voltage",     "Макс. напряжение",     "V", 250.0,   0.0,   1e6 },
        } },

        { T::Inductor, "inductor", "Катушка индуктивности", C::Passive, { "1", "2" }, {
            { "inductance", "Индуктивность",         "H", 1e-3, 1e-12, 1e3 },
            { "dcr",        "Сопротивление обмотки", "Ω", 0.0,  0.0,   1e6 },
        } },

        { T::Choke, "choke", "Дроссель (с магнитопроводом)", C::Passive, { "1", "2" }, {
            { "inductance", "Индуктивность",         "H", 10e-3, 1e-12, 1e3 },
            { "dcr",        "Сопротивление обмотки", "Ω", 0.5,   0.0,   1e6 },
            { "maxCurrent", "Макс. ток насыщения",   "A", 1.0,   0.0,   1e3 },
        } },

        { T::Transformer, "transformer", "Трансформатор", C::Passive,
          { "Первичная 1", "Первичная 2", "Вторичная 1", "Вторичная 2" }, {
            { "inductance", "Индуктивность первичной обмотки", "H", 1.0,  1e-9, 1e4 },
            { "ratio",      "Коэффициент трансформации n₂/n₁", "",  1.0,  1e-3, 1e3 },
            { "coupling",   "Коэффициент связи",               "",  0.99, 0.0,  1.0 },
        } },

        { T::Fuse, "fuse", "Предохранитель", C::Passive, { "1", "2" }, {
            { "current",    "Ток срабатывания", "A", 1.0,  1e-6, 1e4 },
            { "resistance", "Сопротивление",    "Ω", 0.01, 0.0,  1e3 },
        } },

        { T::Lamp, "lamp", "Лампа накаливания", C::Passive, { "1", "2" }, {
            { "voltage", "Номинальное напряжение", "V", 12.0, 1e-3, 1e4 },
            { "power",   "Номинальная мощность",   "W", 5.0,  1e-6, 1e5 },
            { "glow", "Цвет свечения", "", 16762967.0, 0.0, 16777215.0, nullptr, true, true },
        } },

        // ---------------------------------------------------------- источники
        { T::VoltageSourceDC, "vsource_dc", "Источник напряжения (постоянный)", C::Sources, { "+", "−" }, {
            { "voltage", "Напряжение", "V", 5.0, -1e6, 1e6 },
        } },

        { T::Battery, "battery", "Гальванический элемент (батарея)", C::Sources, { "+", "−" }, {
            { "voltage",    "ЭДС",                         "V", 9.0, 0.0, 1e6 },
            { "resistance", "Внутреннее сопротивление",    "Ω", 0.1, 0.0, 1e6 },
        } },

        { T::VoltageSourceAC, "vsource_ac", "Источник напряжения (синусоидальный)", C::Sources, { "A", "B" }, {
            { "amplitude", "Амплитуда", "V",  5.0,  0.0,    1e6   },
            { "frequency", "Частота",   "Hz", 50.0, 1e-3,   1e9   },
            { "offset",    "Смещение",  "V",  0.0,  -1e6,   1e6   },
            { "phase",     "Фаза",      "°",  0.0,  -360.0, 360.0 },
        } },

        { T::Generator, "generator", "Генератор сигналов", C::Sources, { "A", "B" }, {
            { "waveform",  "Форма сигнала", "",   0.0,    0.0,   3.0, "Синус|Прямоугольник|Треугольник|Пила" },
            { "amplitude", "Амплитуда",     "V",  5.0,    0.0,   1e6   },
            { "frequency", "Частота",       "Hz", 1000.0, 1e-3,  1e9   },
            { "offset",    "Смещение",      "V",  0.0,    -1e6,  1e6   },
            { "duty",      "Скважность",    "%",  50.0,   0.0,   100.0 },
            { "phase",     "Фаза",          "°",  0.0,    -360.0, 360.0 },
        } },

        { T::CurrentSource, "isource", "Источник тока", C::Sources, { "A", "B" }, {
            { "current", "Ток", "A", 1e-3, -1e3, 1e3 },
        } },

        // ------------------------------------------------------ полупроводники
        { T::Diode, "diode", "Диод", C::Semiconductors, { "Анод", "Катод" }, {
            { "fwdDrop", "Прямое падение напряжения", "V", 0.7, 0.0, 100.0 },
        } },

        { T::ZenerDiode, "zener", "Стабилитрон", C::Semiconductors, { "Анод", "Катод" }, {
            { "zenerVoltage", "Напряжение стабилизации",    "V", 5.1, 0.1, 1e3   },
            { "fwdDrop",      "Прямое падение напряжения",  "V", 0.7, 0.0, 100.0 },
        } },

        { T::SchottkyDiode, "schottky", "Диод Шоттки", C::Semiconductors, { "Анод", "Катод" }, {
            { "fwdDrop", "Прямое падение напряжения", "V", 0.3, 0.0, 100.0 },
        } },

        { T::LED, "led", "Светодиод", C::Semiconductors, { "Анод", "Катод" }, {
            { "fwdDrop",    "Прямое падение напряжения", "V", 2.0,   0.0, 100.0 },
            { "maxCurrent", "Макс. ток",                 "A", 0.02,  0.0, 100.0 },
            { "glow", "Цвет свечения", "", 15026237.0, 0.0, 16777215.0, nullptr, true, true },
        } },

        { T::NPN, "npn", "Транзистор биполярный n-p-n", C::Semiconductors, { "База", "Коллектор", "Эмиттер" }, {
            { "beta", "Коэффициент усиления β", "",  100.0, 1.0,  1e5 },
            { "vbe",  "Напряжение база–эмиттер", "V", 0.7,   0.0,  10.0 },
        } },

        { T::PNP, "pnp", "Транзистор биполярный p-n-p", C::Semiconductors, { "База", "Коллектор", "Эмиттер" }, {
            { "beta", "Коэффициент усиления β", "",  100.0, 1.0,  1e5 },
            { "vbe",  "Напряжение эмиттер–база", "V", 0.7,   0.0,  10.0 },
        } },

        { T::NMOS, "nmos", "Транзистор полевой MOSFET n-канал", C::Semiconductors, { "Затвор", "Сток", "Исток" }, {
            { "threshold", "Пороговое напряжение",   "V", 2.0, 0.0, 100.0 },
            { "rdson",     "Сопротивление канала",   "Ω", 0.1, 0.0, 1e6   },
        } },

        { T::PMOS, "pmos", "Транзистор полевой MOSFET p-канал", C::Semiconductors, { "Затвор", "Сток", "Исток" }, {
            { "threshold", "Пороговое напряжение",   "V", -2.0, -100.0, 0.0 },
            { "rdson",     "Сопротивление канала",   "Ω", 0.1,  0.0,    1e6 },
        } },

        { T::NJFET, "njfet", "Транзистор полевой с p-n переходом, n-канал", C::Semiconductors, { "Затвор", "Сток", "Исток" }, {
            { "pinchoff", "Напряжение отсечки",   "V", -2.0,  -100.0, 0.0 },
            { "idss",     "Начальный ток стока",  "A", 0.01,  0.0,    100.0 },
        } },

        { T::PJFET, "pjfet", "Транзистор полевой с p-n переходом, p-канал", C::Semiconductors, { "Затвор", "Сток", "Исток" }, {
            { "pinchoff", "Напряжение отсечки",   "V", 2.0,   0.0,    100.0 },
            { "idss",     "Начальный ток стока",  "A", 0.01,  0.0,    100.0 },
        } },

        { T::Thyristor, "thyristor", "Тиристор", C::Semiconductors, { "Анод", "Катод", "Управляющий электрод" }, {
            { "gateVoltage", "Напряжение включения",  "V", 0.7,   0.0, 100.0 },
            { "holdCurrent", "Ток удержания",         "A", 0.005, 0.0, 100.0 },
        } },

        // ------------------------------------------------------------ коммутация
        { T::Switch, "switch", "Выключатель", C::Switches, { "1", "2" }, {
            { "state", "Состояние", "", 0.0, 0.0, 1.0, "Разомкнут|Замкнут" },
        } },

        { T::PushButton, "button", "Кнопка (замыкающая)", C::Switches, { "1", "2" }, {
            { "state", "Состояние", "", 0.0, 0.0, 1.0, "Не нажата|Нажата" },
        } },

        { T::SwitchSPDT, "switch_spdt", "Переключатель", C::Switches, { "Общий", "Контакт 1", "Контакт 2" }, {
            { "position", "Положение", "", 0.0, 0.0, 1.0, "Контакт 1|Контакт 2" },
        } },

        // -------------------------------------------------------------- усилители
        { T::OpAmp, "opamp", "Операционный усилитель", C::Analog,
          { "Инвертирующий вход", "Неинвертирующий вход", "Выход" }, {
            { "gain", "Коэффициент усиления Ku", "",  1e5,   1.0,   1e9 },
            { "vmax", "Макс. выходное напряжение", "V", 15.0,  -1e3,  1e3 },
            { "vmin", "Мин. выходное напряжение",  "V", -15.0, -1e3,  1e3 },
        } },

        // ------------------------------------------------------------------ логика
        { T::GateAnd,  "gate_and",  "Логический элемент И",       C::Logic, { "Вход A", "Вход B", "Выход" }, {
            { "inputs", "Число входов (2…63)", "", 2.0, 2.0, 63.0, nullptr, true },
            { "vhigh",     "Уровень логической 1", "V", 5.0, 0.0, 1e3 },
            { "threshold", "Порог переключения",   "V", 2.5, 0.0, 1e3 },
        }, nullptr, Dynamic::GateInputs },
        { T::GateOr,   "gate_or",   "Логический элемент ИЛИ",     C::Logic, { "Вход A", "Вход B", "Выход" }, {
            { "inputs", "Число входов (2…63)", "", 2.0, 2.0, 63.0, nullptr, true },
            { "vhigh",     "Уровень логической 1", "V", 5.0, 0.0, 1e3 },
            { "threshold", "Порог переключения",   "V", 2.5, 0.0, 1e3 },
        }, nullptr, Dynamic::GateInputs },
        { T::GateNot,  "gate_not",  "Логический элемент НЕ",      C::Logic, { "Вход", "Выход" }, {
            { "vhigh",     "Уровень логической 1", "V", 5.0, 0.0, 1e3 },
            { "threshold", "Порог переключения",   "V", 2.5, 0.0, 1e3 },
        } },
        { T::GateNand, "gate_nand", "Логический элемент И-НЕ",    C::Logic, { "Вход A", "Вход B", "Выход" }, {
            { "inputs", "Число входов (2…63)", "", 2.0, 2.0, 63.0, nullptr, true },
            { "vhigh",     "Уровень логической 1", "V", 5.0, 0.0, 1e3 },
            { "threshold", "Порог переключения",   "V", 2.5, 0.0, 1e3 },
        }, nullptr, Dynamic::GateInputs },
        { T::GateNor,  "gate_nor",  "Логический элемент ИЛИ-НЕ",  C::Logic, { "Вход A", "Вход B", "Выход" }, {
            { "inputs", "Число входов (2…63)", "", 2.0, 2.0, 63.0, nullptr, true },
            { "vhigh",     "Уровень логической 1", "V", 5.0, 0.0, 1e3 },
            { "threshold", "Порог переключения",   "V", 2.5, 0.0, 1e3 },
        }, nullptr, Dynamic::GateInputs },
        { T::GateXor,  "gate_xor",  "Логический элемент исключающее ИЛИ", C::Logic, { "Вход A", "Вход B", "Выход" }, {
            { "inputs", "Число входов (2…63)", "", 2.0, 2.0, 63.0, nullptr, true },
            { "vhigh",     "Уровень логической 1", "V", 5.0, 0.0, 1e3 },
            { "threshold", "Порог переключения",   "V", 2.5, 0.0, 1e3 },
        }, nullptr, Dynamic::GateInputs },

        // ------------------------------------------------------------------ приборы
        { T::Voltmeter, "voltmeter", "Вольтметр", C::Meters, { "+", "−" }, {
            { "resistance", "Входное сопротивление", "Ω", 10e6, 1.0, 1e15 },
        } },
        { T::Ammeter, "ammeter", "Амперметр", C::Meters, { "+", "−" }, {
            { "resistance", "Собственное сопротивление", "Ω", 1e-3, 0.0, 1e6 },
        } },

        // ============================ расширение набора ============================
        { T::NodeLabel, "label_node", "Метка узла (одноимённые метки соединены)", C::Wiring, { "A" }, {
            { "number", "Номер метки", "", 1.0, 1.0, 999.0 },
        } },
        { T::Memristor, "memristor", "Мемристор", C::Passive, { "1", "2" }, {
            { "ron", "Сопротивление Ron (проводящее)", "Ω", 100.0, 0.001, 1000000000.0 },
            { "roff", "Сопротивление Roff (непроводящее)", "Ω", 160000.0, 0.001, 1000000000000.0 },
            { "state0", "Начальное состояние (0…1)", "", 0.5, 0.0, 1.0 },
        } },
        { T::Photoresistor, "photoresistor", "Фоторезистор", C::Passive, { "1", "2" }, {
            { "dark", "Сопротивление в темноте", "Ω", 1000000.0, 1.0, 1000000000000.0 },
            { "light", "Сопротивление при полном освещении", "Ω", 1000.0, 0.001, 1000000000000.0 },
            { "illum", "Освещённость", "%", 50.0, 0.0, 100.0 },
        } },
        { T::Thermistor, "thermistor", "Терморезистор", C::Passive, { "1", "2" }, {
            { "kind", "Тип", "", 0.0, 0.0, 1.0, "NTC (с отрицательным ТКС)|PTC (с положительным ТКС)" },
            { "r25", "Сопротивление при 25 °C", "Ω", 10000.0, 0.001, 1000000000000.0 },
            { "beta", "Постоянная B, К", "", 3950.0, 1.0, 100000.0 },
            { "temp", "Температура, °C", "", 25.0, -273.0, 2000.0 },
        } },
        { T::Varistor, "varistor", "Варистор", C::Passive, { "1", "2" }, {
            { "vclamp", "Напряжение ограничения", "V", 18.0, 0.001, 100000.0 },
            { "roff", "Сопротивление ниже порога", "Ω", 10000000.0, 1.0, 1000000000000.0 },
            { "ron", "Сопротивление выше порога", "Ω", 1.0, 1e-06, 1000000000.0 },
        } },
        { T::SparkGap, "spark_gap", "Разрядник", C::Passive, { "1", "2" }, {
            { "vbreak", "Напряжение пробоя", "V", 1000.0, 1.0, 10000000.0 },
            { "ron", "Сопротивление дуги", "Ω", 0.1, 1e-06, 1000000.0 },
            { "hold", "Ток погасания", "A", 0.001, 0.0, 1000.0 },
        } },
        { T::InductorVar, "inductor_var", "Катушка переменной индуктивности (подстроечная)", C::Passive, { "1", "2" }, {
            { "inductance", "Индуктивность максимальная", "H", 0.01, 1e-12, 1000.0 },
            { "position", "Положение сердечника", "%", 50.0, 0.0, 100.0 },
            { "dcr", "Сопротивление обмотки", "Ω", 0.1, 0.0, 1000000.0 },
        } },
        { T::AutoTransformer, "autotransformer", "Автотрансформатор", C::Passive, { "Начало", "Отвод", "Конец" }, {
            { "inductance", "Индуктивность всей обмотки", "H", 1.0, 1e-09, 10000.0 },
            { "tap", "Положение отвода", "%", 50.0, 0.0, 100.0 },
            { "coupling", "Коэффициент связи", "", 0.99, 0.0, 1.0 },
        } },
        { T::TransformerTapped, "transformer_tap", "Трансформатор со средней точкой", C::Passive, { "Первичная 1", "Первичная 2", "Вторичная 1", "Вторичная (отвод)", "Вторичная 2" }, {
            { "inductance", "Индуктивность первичной обмотки", "H", 1.0, 1e-09, 10000.0 },
            { "ratio", "Коэффициент трансформации n₂/n₁", "", 1.0, 0.001, 1000.0 },
            { "coupling", "Коэффициент связи", "", 0.99, 0.0, 1.0 },
        } },
        { T::CoupledInductors, "coupled_inductors", "Связанные катушки (взаимная индуктивность)", C::Passive, { "Катушка 1а", "Катушка 1б", "Катушка 2а", "Катушка 2б" }, {
            { "l1", "Индуктивность катушки 1", "H", 0.001, 1e-12, 10000.0 },
            { "l2", "Индуктивность катушки 2", "H", 0.001, 1e-12, 10000.0 },
            { "coupling", "Коэффициент связи k", "", 0.9, 0.0, 1.0 },
        } },
        { T::RailDC, "rail_dc", "Шина питания (одновыводной источник)", C::Sources, { "A" }, {
            { "voltage", "Напряжение", "V", 5.0, -1000000.0, 1000000.0 },
        } },
        { T::RailAC, "rail_ac", "Источник переменного напряжения (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Амплитуда", "V", 5.0, 0.0, 1000000.0 },
            { "frequency", "Частота", "Hz", 50.0, 0.001, 1000000000.0 },
            { "offset", "Смещение", "V", 0.0, -1000000.0, 1000000.0 },
            { "phase", "Фаза", "°", 0.0, -360.0, 360.0 },
        } },
        { T::RailSquare, "rail_square", "Генератор прямоугольных импульсов (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Амплитуда", "V", 5.0, 0.0, 1000000.0 },
            { "frequency", "Частота", "Hz", 1000.0, 0.001, 1000000000.0 },
            { "offset", "Смещение", "V", 0.0, -1000000.0, 1000000.0 },
            { "duty", "Скважность", "%", 50.0, 0.0, 100.0 },
        } },
        { T::RailClock, "rail_clock", "Тактовый генератор (одновыводной)", C::Sources, { "A" }, {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "frequency", "Частота", "Hz", 1000.0, 0.001, 1000000000.0 },
            { "duty", "Скважность", "%", 50.0, 0.0, 100.0 },
        } },
        { T::RailSweep, "rail_sweep", "Генератор качающейся частоты (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Амплитуда", "V", 5.0, 0.0, 1000000.0 },
            { "fstart", "Начальная частота", "Hz", 20.0, 0.001, 1000000000.0 },
            { "fstop", "Конечная частота", "Hz", 20000.0, 0.001, 1000000000.0 },
            { "time", "Время развёртки", "s", 0.1, 1e-06, 1000000.0 },
        } },
        { T::RailVariable, "rail_var", "Регулируемый источник напряжения (ползунок)", C::Sources, { "A" }, {
            { "vmin", "Минимальное напряжение", "V", 0.0, -1000000.0, 1000000.0 },
            { "vmax", "Максимальное напряжение", "V", 5.0, -1000000.0, 1000000.0 },
            { "position", "Положение ползунка", "%", 50.0, 0.0, 100.0 },
        } },
        { T::RailNoise, "rail_noise", "Источник шума (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Размах шума", "V", 1.0, 0.0, 1000000.0 },
            { "offset", "Смещение", "V", 0.0, -1000000.0, 1000000.0 },
        } },
        { T::RailAM, "rail_am", "Источник AM-сигнала (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Амплитуда несущей", "V", 1.0, 0.0, 1000000.0 },
            { "carrier", "Несущая частота", "Hz", 100000.0, 1.0, 10000000000.0 },
            { "signal", "Частота сигнала", "Hz", 1000.0, 0.001, 1000000000.0 },
            { "depth", "Глубина модуляции", "%", 50.0, 0.0, 100.0 },
        } },
        { T::RailFM, "rail_fm", "Источник FM-сигнала (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Амплитуда", "V", 1.0, 0.0, 1000000.0 },
            { "carrier", "Несущая частота", "Hz", 100000.0, 1.0, 10000000000.0 },
            { "signal", "Частота сигнала", "Hz", 1000.0, 0.001, 1000000000.0 },
            { "deviation", "Девиация частоты", "Hz", 10000.0, 0.0, 1000000000.0 },
        } },
        { T::LogicInput, "logic_in", "Логический вход (переключатель 0/1)", C::Sources, { "A" }, {
            { "state", "Состояние", "", 0.0, 0.0, 1.0, "0 (низкий)|1 (высокий)" },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
        } },
        { T::VCVS, "vcvs", "ИНУН — источник напряжения, управляемый напряжением", C::Controlled, { "Упр +", "Упр −", "Вых +", "Вых −" }, {
            { "gain", "Коэффициент передачи (В/В)", "", 1.0, -1000000000.0, 1000000000.0 },
        } },
        { T::VCCS, "vccs", "ИТУН — источник тока, управляемый напряжением", C::Controlled, { "Упр +", "Упр −", "Вых +", "Вых −" }, {
            { "gm", "Крутизна", "S", 0.001, -1000000000.0, 1000000000.0 },
        } },
        { T::CCVS, "ccvs", "ИНУТ — источник напряжения, управляемый током", C::Controlled, { "Упр +", "Упр −", "Вых +", "Вых −" }, {
            { "rm", "Передаточное сопротивление", "Ω", 1000.0, -1000000000000.0, 1000000000000.0 },
        } },
        { T::CCCS, "cccs", "ИТУТ — источник тока, управляемый током", C::Controlled, { "Упр +", "Упр −", "Вых +", "Вых −" }, {
            { "gain", "Коэффициент передачи (А/А)", "", 1.0, -1000000000.0, 1000000000.0 },
        } },
        { T::Varactor, "varactor", "Варикап", C::Semiconductors, { "Анод", "Катод" }, {
            { "cj0", "Ёмкость при 0 В", "F", 1e-10, 1e-15, 0.001 },
            { "vj", "Контактная разность потенциалов", "V", 0.7, 0.01, 10.0 },
            { "m", "Коэффициент плавности перехода", "", 0.5, 0.1, 1.0 },
        } },
        { T::TunnelDiode, "tunnel_diode", "Туннельный диод", C::Semiconductors, { "Анод", "Катод" }, {
            { "ip", "Пиковый ток", "A", 0.001, 1e-09, 1000.0 },
            { "vp", "Пиковое напряжение", "V", 0.1, 0.01, 10.0 },
            { "vv", "Напряжение впадины", "V", 0.35, 0.02, 10.0 },
        } },
        { T::Diac, "diac", "Диак (симметричный динистор)", C::Semiconductors, { "Вывод 1", "Вывод 2" }, {
            { "vbo", "Напряжение переключения", "V", 32.0, 1.0, 1000.0 },
            { "hold", "Ток удержания", "A", 0.005, 0.0, 10.0 },
        } },
        { T::Triac, "triac", "Симистор", C::Semiconductors, { "Силовой 1", "Силовой 2", "Управляющий" }, {
            { "gateCurrent", "Ток управления", "A", 0.005, 0.0, 10.0 },
            { "hold", "Ток удержания", "A", 0.01, 0.0, 10.0 },
        } },
        { T::DarlingtonNPN, "darlington_npn", "Транзистор Дарлингтона n-p-n", C::Semiconductors, { "База", "Коллектор", "Эмиттер" }, {
            { "beta", "Общий коэффициент усиления β", "", 10000.0, 1.0, 10000000.0 },
            { "vbe", "Напряжение база–эмиттер", "V", 1.4, 0.0, 10.0 },
        } },
        { T::DarlingtonPNP, "darlington_pnp", "Транзистор Дарлингтона p-n-p", C::Semiconductors, { "База", "Коллектор", "Эмиттер" }, {
            { "beta", "Общий коэффициент усиления β", "", 10000.0, 1.0, 10000000.0 },
            { "vbe", "Напряжение эмиттер–база", "V", 1.4, 0.0, 10.0 },
        } },
        { T::Photodiode, "photodiode", "Фотодиод", C::Semiconductors, { "Анод", "Катод" }, {
            { "ifull", "Фототок при полном освещении", "A", 0.0001, 0.0, 10.0 },
            { "illum", "Освещённость", "%", 50.0, 0.0, 100.0 },
        } },
        { T::Phototransistor, "phototransistor", "Фототранзистор", C::Semiconductors, { "База", "Коллектор", "Эмиттер" }, {
            { "beta", "Коэффициент усиления β", "", 200.0, 1.0, 100000.0 },
            { "illum", "Освещённость", "%", 50.0, 0.0, 100.0 },
        } },
        { T::IGBT, "igbt", "IGBT (биполярный транзистор с изолированным затвором)", C::Semiconductors, { "Затвор", "Коллектор", "Эмиттер" }, {
            { "threshold", "Пороговое напряжение", "V", 5.0, 0.0, 100.0 },
            { "vcesat", "Напряжение насыщения", "V", 1.5, 0.0, 100.0 },
        } },
        { T::UJT, "ujt", "Однопереходный транзистор", C::Semiconductors, { "Эмиттер", "База 2", "База 1" }, {
            { "eta", "Коэффициент передачи η", "", 0.6, 0.1, 0.95 },
            { "rbb", "Межбазовое сопротивление", "Ω", 5000.0, 1.0, 1000000.0 },
        } },
        { T::TL431, "tl431", "Управляемый стабилитрон (TL431)", C::Semiconductors, { "Анод", "Катод", "Опорный вход" }, {
            { "vref", "Опорное напряжение", "V", 2.5, 0.1, 100.0 },
        } },
        { T::PushButtonNC, "button_nc", "Кнопка (размыкающая)", C::Switches, { "1", "2" }, {
            { "state", "Состояние", "", 0.0, 0.0, 1.0, "Не нажата (замкнута)|Нажата (разомкнута)" },
        } },
        { T::SwitchMBB, "switch_mbb", "Переключатель с перекрытием контактов (make-before-break)", C::Switches, { "Общий", "Контакт 1", "Контакт 2" }, {
            { "position", "Положение", "", 0.0, 0.0, 1.0, "Контакт 1|Контакт 2" },
        } },
        { T::SwitchSP3T, "switch_sp3t", "Переключатель на три направления", C::Switches, { "Общий", "Контакт 1", "Контакт 2", "Контакт 3" }, {
            { "position", "Положение", "", 0.0, 0.0, 2.0, "Контакт 1|Контакт 2|Контакт 3" },
        } },
        { T::SwitchDPST, "switch_dpst", "Двухгруппный выключатель", C::Switches, { "1А", "1Б", "2А", "2Б" }, {
            { "state", "Состояние", "", 0.0, 0.0, 1.0, "Разомкнут|Замкнут" },
        } },
        { T::SwitchDPDT, "switch_dpdt", "Двухгруппный переключатель", C::Switches, { "1 общий", "1.1", "1.2", "2 общий", "2.1", "2.2" }, {
            { "position", "Положение", "", 0.0, 0.0, 1.0, "Контакты 1|Контакты 2" },
        } },
        chipInfo(T::Relay, "relay", "Электромагнитное реле", C::Switches, "@relay;К+,К−|Общ,НР,НЗ", {
            { "coilR", "Сопротивление катушки", "Ω", 100.0, 0.1, 1000000.0 },
            { "pickup", "Ток срабатывания", "A", 0.02, 1e-06, 100.0 },
            { "release", "Ток отпускания", "A", 0.005, 1e-06, 100.0 },
        }),
        chipInfo(T::AnalogSwitch, "analog_switch", "Аналоговый ключ", C::Switches, "SW;A,У|B", {
            { "ron", "Сопротивление открытого ключа", "Ω", 100.0, 0.001, 1000000000.0 },
            { "roff", "Сопротивление закрытого ключа", "Ω", 1000000000.0, 1.0, 1000000000000000.0 },
            { "threshold", "Порог управления", "V", 2.5, 0.0, 1000.0 },
        }),
        { T::OpAmpPower, "opamp_power", "Операционный усилитель с выводами питания", C::Analog, { "Инвертирующий вход", "Неинвертирующий вход", "Выход", "Питание +", "Питание −" }, {
            { "gain", "Коэффициент усиления Ku", "", 100000.0, 1.0, 1000000000.0 },
            { "gbw", "Частота единичного усиления", "Hz", 1000000.0, 1.0, 1000000000000.0 },
            { "rout", "Выходное сопротивление", "Ω", 75.0, 0.0, 1000000.0 },
        } },
        { T::Comparator, "comparator", "Компаратор", C::Analog, { "Инвертирующий вход", "Неинвертирующий вход", "Выход" }, {
            { "vhigh", "Выход при V+ > V−", "V", 5.0, -1000.0, 1000.0 },
            { "vlow", "Выход при V+ < V−", "V", 0.0, -1000.0, 1000.0 },
            { "hyst", "Гистерезис", "V", 0.0, 0.0, 1000.0 },
        } },
        chipInfo(T::OTA, "ota", "Операционный усилитель-источник тока (OTA)", C::Analog, "OTA;U−,U+,Iу|Вых", {
            { "gm", "Крутизна", "S", 0.001, 0.0, 1000.0 },
            { "imax", "Макс. выходной ток", "A", 0.001, 0.0, 1000.0 },
        }),
        chipInfo(T::CCII, "ccii", "Токовый конвейер (CCII)", C::Analog, "CCII;Y,X|Z", {
            { "polarity", "Тип", "", 0.0, 0.0, 1.0, "CCII+|CCII−" },
        }),
        chipInfo(T::Regulator, "regulator", "Линейный стабилизатор напряжения (78xx)", C::Analog, "78xx;IN|OUT||GND", {
            { "vout", "Выходное напряжение", "V", 5.0, 0.1, 100.0 },
            { "dropout", "Минимальное падение", "V", 2.0, 0.0, 50.0 },
            { "imax", "Макс. ток", "A", 1.0, 0.0, 100.0 },
        }),
        chipInfo(T::Timer555, "timer555", "Таймер 555", C::Analog, "555;TRIG,THR,CTRL,RESET|OUT,DIS|V+|GND", {
            { "rdis", "Сопротивление ключа разряда", "Ω", 10.0, 0.1, 1000000.0 },
        }),
        chipInfo(T::VCO, "vco", "Генератор, управляемый напряжением (ГУН)", C::Analog, "VCO;Vу|Вых", {
            { "fmin", "Частота при минимальном напряжении", "Hz", 1000.0, 0.001, 10000000000.0 },
            { "fmax", "Частота при максимальном напряжении", "Hz", 100000.0, 0.001, 10000000000.0 },
            { "vmax", "Напряжение для максимальной частоты", "V", 5.0, 0.001, 1000.0 },
        }),
        chipInfo(T::PLL, "pll", "Система ФАПЧ (PLL)", C::Analog, "PLL;Вх|Вых,Vу", {
            { "fcenter", "Центральная частота", "Hz", 10000.0, 0.001, 10000000000.0 },
            { "range", "Полоса захвата", "Hz", 5000.0, 0.0, 10000000000.0 },
        }),
        chipInfo(T::PhaseComparator, "phase_comp", "Фазовый детектор", C::Analog, "φ;A,B|Вых", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
        }),
        chipInfo(T::SampleHold, "sample_hold", "Устройство выборки-хранения", C::Analog, "S/H;Вх,>Выб|Вых", {
            { "cap", "Ёмкость хранения", "F", 1e-09, 1e-15, 0.001 },
        }),
        { T::GateXnor, "gate_xnor", "Логический элемент исключающее ИЛИ-НЕ", C::Logic, { "Вход A", "Вход B", "Выход" }, {
            { "inputs", "Число входов (2…63)", "", 2.0, 2.0, 63.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, nullptr, Dynamic::GateInputs },
        { T::GateBuffer, "gate_buffer", "Буфер (повторитель)", C::Logic, { "Вход", "Выход" }, {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        } },
        { T::SchmittTrigger, "schmitt", "Триггер Шмитта", C::Logic, { "Вход", "Выход" }, {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "vup", "Порог включения", "V", 3.0, 0.0, 1000.0 },
            { "vdown", "Порог выключения", "V", 1.5, 0.0, 1000.0 },
        } },
        { T::SchmittInverting, "schmitt_inv", "Триггер Шмитта инвертирующий", C::Logic, { "Вход", "Выход" }, {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "vup", "Порог включения", "V", 3.0, 0.0, 1000.0 },
            { "vdown", "Порог выключения", "V", 1.5, 0.0, 1000.0 },
        } },
        chipInfo(T::TriStateBuffer, "tristate", "Буфер с тремя состояниями", C::Logic, "1▽;A,EN|Y", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::DFlipFlop, "dff", "D-триггер", C::Digital, "D;D,>C|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::DFlipFlopSR, "dff_sr", "D-триггер с входами S и R", C::Digital, "D;S,D,>C,R|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::JKFlipFlop, "jkff", "JK-триггер", C::Digital, "JK;J,>C,K|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::TFlipFlop, "tff", "T-триггер", C::Digital, "T;T,>C|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::SRLatch, "srlatch", "RS-триггер", C::Digital, "RS;S,R|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::DLatch, "dlatch", "D-защёлка", C::Digital, "D;D,C|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::DecadeCounter, "decade_counter", "Десятичный счётчик (4017)", C::Digital, "CT10;>C,R|O0,O1,O2,O3,O4,O5,O6,O7,O8,O9,CO", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::HalfAdder, "half_adder", "Полусумматор", C::Digital, "HS;A,B|S,C", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::FullAdder, "full_adder", "Полный сумматор", C::Digital, "SM;A,B,CI|S,CO", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::Decoder7Seg, "decoder_7seg", "Дешифратор для семисегментного индикатора", C::Digital, "DC;A,B,C,D|a,b,c,d,e,f,g", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::Monostable, "monostable", "Одновибратор", C::Digital, "1;>T|Q,/Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "pulse", "Длительность импульса", "s", 0.001, 1e-09, 1000000.0 },
        }),
        chipInfo(T::SeqGen, "seq_gen", "Генератор последовательности", C::Digital, "SEQ;>C,R|Q", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "pattern", "Последовательность (биты, младший первый)", "", 178.0, 0.0, 65535.0 },
            { "length", "Длина последовательности", "", 8.0, 1.0, 16.0 },
        }),
        chipInfo(T::DelayLine, "delay_line", "Линия задержки сигнала", C::Digital, "τ;Вх|Вых", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "delay", "Время задержки", "s", 1e-06, 0.0, 1000.0 },
        }),
        chipInfo(T::RAM, "ram", "ОЗУ (RAM)", C::Memory, "RAM 16×4;A0,A1,A2,A3,/CS,/WE,/OE|D0,D1,D2,D3", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "addrBits", "Объём памяти (число слов)", "", 3.0, 0.0, 15.0, "2 слова (1 бит адреса)|4 слова (2 бита)|8 слов (3 бита)|16 слов (4 бита)|32 слова (5 бит)|64 слова (6 бит)|128 слов (7 бит)|256 слов (8 бит)|512 слов (9 бит)|1 К слов (10 бит)|2 К слов (11 бит)|4 К слов (12 бит)|8 К слов (13 бит)|16 К слов (14 бит)|32 К слов (15 бит)|64 К слов (16 бит)" },
            { "dataBits", "Разрядность слова, бит (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "init", "Начальное содержимое", "", 0.0, 0.0, 3.0, "Нули|Единицы|Случайное|Счётчик" },
        }, Dynamic::Memory),
        chipInfo(T::ROM, "rom", "ПЗУ (ROM)", C::Memory, "ROM 16×4;A0,A1,A2,A3,/CS|D0,D1,D2,D3", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "addrBits", "Объём памяти (число слов)", "", 3.0, 0.0, 15.0, "2 слова (1 бит адреса)|4 слова (2 бита)|8 слов (3 бита)|16 слов (4 бита)|32 слова (5 бит)|64 слова (6 бит)|128 слов (7 бит)|256 слов (8 бит)|512 слов (9 бит)|1 К слов (10 бит)|2 К слов (11 бит)|4 К слов (12 бит)|8 К слов (13 бит)|16 К слов (14 бит)|32 К слов (15 бит)|64 К слов (16 бит)" },
            { "dataBits", "Разрядность слова, бит (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "content", "Содержимое", "", 2.0, 0.0, 3.0, "Нули|Единицы|Счётчик|Шахматка" },
        }, Dynamic::Memory),
        chipInfo(T::DiodeMatrix, "diode_matrix", "Диодная матрица", C::Memory, "@dmat;I0,I1,I2,I3|O0,O1,O2,O3", {
            { "inputs", "Число входов (строк), 1…16", "", 4.0, 1.0, 16.0, nullptr, true },
            { "outputs", "Число выходов (столбцов), 1…16", "", 4.0, 1.0, 16.0, nullptr, true },
            { "preset", "Рисунок диодов", "", 0.0, 0.0, 3.0, "Диагональ|Все пересечения|Шахматка|Нет диодов" },
            { "fwdDrop", "Падение напряжения на диоде", "V", 0.7, 0.0, 10.0 },
        }, Dynamic::DiodeMatrix),
        chipInfo(T::SegDisplay, "seg_display", "Сегментный индикатор", C::Display, "@seg7;a,b,c,d,e,f,g,DP|K", {
            { "segments", "Тип индикатора", "", 2.0, 0.0, 5.0, "3 сегмента (1 и знак)|5 сегментов|7 сегментов|9 сегментов|14 сегментов (звезда)|16 сегментов" },
            { "digits", "Число разрядов (1…16)", "", 1.0, 1.0, 16.0, nullptr, true },
            { "vf", "Прямое падение напряжения сегмента", "V", 2.0, 0.0, 10.0 },
            { "common", "Общий вывод", "", 0.0, 0.0, 1.0, "Общий катод|Общий анод" },
            { "glow", "Цвет свечения", "", 15026237.0, 0.0, 16777215.0, nullptr, true, true },
        }, Dynamic::SegDisplay),
        chipInfo(T::LedMatrix, "led_matrix", "Светодиодная матрица", C::Display, "@ledm;R0,R1,R2,R3,R4,R5,R6,R7|C0,C1,C2,C3,C4,C5,C6,C7", {
            { "rows", "Число строк (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "cols", "Число столбцов (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "pinout", "Распиновка выводов", "", 0.0, 0.0, 1.0, "Как у реальной матрицы 8×8|По порядку" },
            { "vf", "Прямое падение напряжения", "V", 2.0, 0.0, 10.0 },
            { "glow", "Цвет свечения", "", 15026237.0, 0.0, 16777215.0, nullptr, true, true },
        }, Dynamic::LedMatrix),
        // RGB-светодиод: обычный знак светодиода с тремя входами (анодами) R, G, B и общим катодом K.
        { T::LedRGB, "led_rgb", "RGB-светодиод (общий катод)", C::Display, { "R", "G", "B", "K" }, {
            { "vfr", "Падение, красный", "V", 2.0, 0.0, 10.0 },
            { "vfg", "Падение, зелёный", "V", 3.0, 0.0, 10.0 },
            { "vfb", "Падение, синий", "V", 3.0, 0.0, 10.0 },
        } },
        { T::DecimalDisplay, "decimal_display", "Цифровой индикатор напряжения", C::Display, { "+", "−" }, {
            { "digits", "Число знаков", "", 4.0, 1.0, 8.0, nullptr, true },
            { "resistance", "Входное сопротивление", "Ω", 10000000.0, 1.0, 1000000000000000.0 },
            { "glow", "Цвет цифр", "", 65142.0, 0.0, 16777215.0, nullptr, true, true },
        } },
        { T::LogicOutput, "logic_out", "Логический выход (индикатор 0/1)", C::Display, { "A" }, {
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "glow", "Цвет свечения", "", 8376221.0, 0.0, 16777215.0, nullptr, true, true },
        } },
        { T::Ohmmeter, "ohmmeter", "Омметр", C::Meters, { "1", "2" }, {
            { "current", "Измерительный ток", "A", 0.001, 1e-09, 1.0 },
        } },
        chipInfo(T::Wattmeter, "wattmeter", "Ваттметр", C::Meters, "W;U+,U−|I+,I−", {
            { "resistance", "Входное сопротивление цепи напряжения", "Ω", 10000000.0, 1.0, 1000000000000000.0 },
        }),
        { T::Scope, "scope", "Осциллограф (канал)", C::Meters, { "+", "−" }, {
            { "timeDiv", "Развёртка (время на деление)", "s", 0.001, 1e-09, 1000.0 },
            { "voltDiv", "Чувствительность (вольт на деление)", "V", 1.0, 1e-06, 1000000.0 },
        } },
        { T::Probe, "probe", "Щуп (маркер напряжения)", C::Meters, { "A" }, {} },
        { T::Crystal, "crystal", "Кварцевый резонатор", C::Radio, { "1", "2" }, {
            { "frequency", "Резонансная частота", "Hz", 32768.0, 1.0, 1000000000.0 },
            { "q", "Добротность", "", 100000.0, 1.0, 1000000000.0 },
            { "cp", "Ёмкость держателя", "F", 5e-12, 1e-15, 1e-06 },
        } },
        { T::Antenna, "antenna", "Антенна", C::Radio, { "A" }, {
            { "frequency", "Частота принимаемого сигнала", "Hz", 100000.0, 1.0, 10000000000.0 },
            { "amplitude", "Амплитуда наведённого сигнала", "V", 0.001, 0.0, 1000.0 },
        } },
        { T::Speaker, "speaker", "Громкоговоритель (динамик)", C::Radio, { "1", "2" }, {
            { "impedance", "Сопротивление звуковой катушки", "Ω", 8.0, 0.1, 1000000.0 },
            { "power", "Номинальная мощность", "W", 1.0, 0.0, 100000.0 },
        } },
        { T::Microphone, "microphone", "Микрофон", C::Radio, { "+", "−" }, {
            { "amplitude", "Амплитуда сигнала", "V", 0.01, 0.0, 10.0 },
            { "frequency", "Частота", "Hz", 1000.0, 1.0, 100000.0 },
            { "resistance", "Выходное сопротивление", "Ω", 600.0, 0.0, 1000000.0 },
        } },
        { T::Triode, "triode", "Вакуумный триод", C::Radio, { "Сетка", "Анод", "Катод" }, {
            { "mu", "Коэффициент усиления μ", "", 100.0, 1.0, 10000.0 },
            { "rp", "Внутреннее сопротивление", "Ω", 62500.0, 1.0, 1000000000.0 },
        } },
        chipInfo(T::Optocoupler, "optocoupler", "Оптопара (оптрон)", C::Radio, "@opto;A,K|C,E", {
            { "ctr", "Коэффициент передачи тока", "%", 100.0, 0.0, 10000.0 },
            { "vf", "Падение на светодиоде", "V", 1.2, 0.0, 10.0 },
        }),
        { T::DiodeBridge, "diode_bridge", "Диодный мост", C::Radio, { "~1", "~2", "+", "−" }, {
            { "fwdDrop", "Падение напряжения на диоде", "V", 0.7, 0.0, 10.0 },
        } },
        { T::NeonLamp, "neon_lamp", "Неоновая лампа", C::Radio, { "1", "2" }, {
            { "strike", "Напряжение зажигания", "V", 90.0, 1.0, 10000.0 },
            { "hold", "Напряжение горения", "V", 60.0, 1.0, 10000.0 },
            { "ron", "Сопротивление при горении", "Ω", 500.0, 0.1, 1000000.0 },
            { "glow", "Цвет свечения", "", 16747069.0, 0.0, 16777215.0, nullptr, true, true },
        } },
        { T::Motor, "motor", "Электродвигатель постоянного тока", C::Radio, { "1", "2" }, {
            { "resistance", "Сопротивление обмотки", "Ω", 5.0, 0.001, 1000000.0 },
            { "km", "Постоянная двигателя", "", 0.01, 0.0, 100.0 },
            { "inertia", "Момент инерции ротора", "", 0.0001, 1e-09, 10.0 },
        } },
        chipInfo(T::TransmissionLine, "tline", "Линия передачи (длинная линия)", C::Radio, "Z₀;A+,A−|B+,B−", {
            { "z0", "Волновое сопротивление", "Ω", 50.0, 1.0, 10000.0 },
            { "delay", "Время задержки", "s", 1e-09, 0.0, 1.0 },
        }),
        { T::TextLabel, "text_label", "Надпись", C::Wiring, {  }, {
            { "size", "Размер шрифта, px", "", 14.0, 6.0, 96.0, nullptr, true },
        } },
        { T::Box, "box", "Прямоугольник (рамка)", C::Wiring, {  }, {} },
        { T::Line, "line", "Прямая линия (без соединений)", C::Wiring, {  }, {} },
        chipInfo(T::TransformerCustom, "transformer_custom", "Пользовательский трансформатор (1 первичная, 2 вторичных)", C::Passive, "TR;P1,P2|S1a,S1b,S2a,S2b", {
            { "inductance", "Индуктивность первичной обмотки", "H", 1.0, 1e-09, 10000.0 },
            { "ratio", "Коэффициент трансформации n₂/n₁ (обмотка S1)", "", 1.0, 0.001, 1000.0 },
            { "ratio2", "Коэффициент трансформации n₃/n₁ (обмотка S2)", "", 1.0, 0.001, 1000.0 },
            { "coupling", "Коэффициент связи", "", 0.99, 0.0, 1.0 },
        }),
        chipInfo(T::RelayCoil, "relay_coil", "Катушка реле (отдельная)", C::Passive, "К;+|−", {
            { "number", "Номер реле (связь с контактами)", "", 1.0, 1.0, 999.0, nullptr, true },
            { "coilR", "Сопротивление катушки", "Ω", 100.0, 0.1, 1000000.0 },
            { "pickup", "Ток срабатывания", "A", 0.02, 1e-06, 100.0 },
            { "release", "Ток отпускания", "A", 0.005, 1e-06, 100.0 },
        }),
        chipInfo(T::RelayContact, "relay_contact", "Контакт реле (отдельный)", C::Passive, "@rcont;Общ|НР,НЗ", {
            { "number", "Номер реле (связь с катушкой)", "", 1.0, 1.0, 999.0, nullptr, true },
            { "ron", "Сопротивление замкнутого контакта", "Ω", 0.05, 1e-06, 1000000.0 },
        }),
        chipInfo(T::CrossSwitch, "switch_cross", "Перекрёстный переключатель", C::Switches, "@xsw;A1,A2|B1,B2", {
            { "position", "Положение", "", 0.0, 0.0, 1.0, "Прямое (A1–B1, A2–B2)|Перекрёстное (A1–B2, A2–B1)" },
        }),
        chipInfo(T::Gyrator, "gyrator", "Гиратор", C::Passive, "G;A+,A−|B+,B−", {
            { "g", "Проводимость гирации", "S", 0.001, 1e-12, 1000.0 },
        }),
        { T::AudioInput, "audio_in", "Звуковой вход (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Амплитуда сигнала", "V", 1.0, 0.0, 1000.0 },
            { "offset", "Смещение", "V", 0.0, -1000.0, 1000.0 },
        } },
        { T::DataInput, "data_in", "Ввод данных (одновыводной)", C::Sources, { "A" }, {
            { "amplitude", "Масштаб", "V", 5.0, 0.0, 1000000.0 },
            { "period", "Период повтора данных", "s", 1.0, 1e-09, 1000000.0 },
        } },
        { T::ExtVoltage, "ext_voltage", "Внешнее напряжение (программный интерфейс)", C::Sources, { "A" }, {
            { "channel", "Номер канала", "", 1.0, 1.0, 999.0, nullptr, true },
            { "voltage", "Начальное напряжение", "V", 0.0, -1000000.0, 1000000.0 },
        } },
        { T::AnalogOutput, "analog_output", "Аналоговый выход (показ напряжения)", C::Display, { "A" }, {
            { "scale", "Масштаб", "", 1.0, 1e-06, 1000000.0 },
        } },
        { T::TestPoint, "test_point", "Контрольная точка", C::Meters, { "A" }, {} },
        chipInfo(T::InstructionDisplay, "instr_display", "Дисплей инструкций", C::Display, "INSTR;>C,D0,D1,D2,D3,D4,D5,D6,D7|", {
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "cpu", "Набор команд", "", 0.0, 0.0, 2.0, "Условные (NOP, LDA, ADD…)|Z80 (часть)|6502 (часть)" },
        }),
        { T::DataExport, "data_export", "Экспорт данных (запись в файл)", C::Meters, { "A" }, {
            { "samples", "Число отсчётов", "", 1000.0, 1.0, 10000000.0, nullptr, true },
        } },
        { T::AudioOutput, "audio_out", "Звуковой выход (одновыводной)", C::Display, { "A" }, {
            { "volume", "Громкость", "%", 50.0, 0.0, 100.0 },
        } },
        { T::StopTrigger, "stop_trigger", "Триггер, останавливающий симуляцию", C::Meters, { "A" }, {
            { "threshold", "Порог напряжения", "V", 2.5, -1000000.0, 1000000.0 },
            { "mode", "Условие", "", 0.0, 0.0, 1.0, "Напряжение выше порога|Напряжение ниже порога" },
        } },
        chipInfo(T::Motor3Phase, "motor_3ph", "Трёхфазный двигатель", C::Radio, "M 3~;U,V,W|", {
            { "resistance", "Сопротивление фазы", "Ω", 2.0, 0.001, 1000000.0 },
            { "inductance", "Индуктивность фазы", "H", 0.01, 1e-09, 1000.0 },
            { "poles", "Число пар полюсов", "", 2.0, 1.0, 32.0, nullptr, true },
        }),
        chipInfo(T::WattmeterOld, "wattmeter_old", "Ваттметр (старый)", C::Meters, "W;U+,U−|I+,I−", {
            { "resistance", "Входное сопротивление цепи напряжения", "Ω", 10000000.0, 1.0, 1000000000000000.0 },
        }),
        chipInfo(T::AnalogSwitchSPDT, "analog_switch_spdt", "Аналоговый переключатель (SPDT)", C::Switches, "SW;A,У|B1,B2", {
            { "ron", "Сопротивление открытого ключа", "Ω", 100.0, 0.001, 1000000000.0 },
            { "roff", "Сопротивление закрытого ключа", "Ω", 1000000000.0, 1.0, 1000000000000000.0 },
            { "threshold", "Порог управления", "V", 2.5, 0.0, 1000.0 },
        }),
        chipInfo(T::NortonAmp, "norton_amp", "Усилитель Нортона (LM3900)", C::Analog, "LM3900;I+,I−|Вых", {
            { "gain", "Коэффициент усиления по току", "", 100000.0, 1.0, 1000000000.0 },
            { "vmax", "Максимальный выход", "V", 12.0, 0.1, 100.0 },
        }),
        chipInfo(T::TimeDelayRelay, "time_delay_relay", "Реле времени", C::Switches, "@relay;К+,К−|Общ,НР,НЗ", {
            { "coilR", "Сопротивление катушки", "Ω", 100.0, 0.1, 1000000.0 },
            { "ondelay", "Задержка включения", "s", 1.0, 0.0, 1000000.0 },
            { "offdelay", "Задержка отключения", "s", 0.0, 0.0, 1000000.0 },
        }),
        chipInfo(T::LM317, "lm317", "Регулируемый стабилизатор LM317", C::Analog, "LM317;IN|OUT||ADJ", {
            { "vref", "Опорное напряжение", "V", 1.25, 0.1, 10.0 },
            { "dropout", "Минимальное падение", "V", 2.0, 0.0, 50.0 },
            { "imax", "Макс. ток", "A", 1.5, 0.0, 100.0 },
        }),
        chipInfo(T::MotorProtection, "motor_protection", "Защитный выключатель двигателя (тепловое реле)", C::Switches, "MPS;1,3,5|2,4,6", {
            { "current", "Ток срабатывания", "A", 5.0, 0.001, 10000.0 },
            { "delay", "Время срабатывания", "s", 2.0, 0.0, 10000.0 },
            { "state", "Состояние", "", 0.0, 0.0, 1.0, "Включён|Отключён (сработал)" },
        }),
        chipInfo(T::Mux, "mux", "Мультиплексор", C::Digital, "MUX;D0|Y", {
            { "bits", "Число адресных входов (2^n входов данных, 1…5)", "", 2.0, 1.0, 5.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Demux, "demux", "Демультиплексор", C::Digital, "DMX;D|Y0", {
            { "bits", "Число адресных входов (2^n выходов, 1…5)", "", 2.0, 1.0, 5.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Decoder, "decoder", "Дешифратор", C::Digital, "DC;A0|Y0", {
            { "bits", "Число адресных входов (2^n выходов, 1…5)", "", 2.0, 1.0, 5.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Encoder, "encoder", "Приоритетный шифратор", C::Digital, "CD;I0|A0", {
            { "bits", "Число выходов кода (2^n входов, 1…5)", "", 2.0, 1.0, 5.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Register, "register", "Регистр (параллельный)", C::Digital, "RG;D0|Q0", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Shift, "shift", "Сдвиговый регистр SIPO", C::Digital, "SRG;D|Q0", {
            { "bits", "Разрядность (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::PISO, "piso", "Сдвиговый регистр PISO", C::Digital, "PISO;D0|Q", {
            { "bits", "Разрядность (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Counter, "counter", "Двоичный счётчик", C::Digital, "CT;C|Q0", {
            { "bits", "Разрядность (1…32)", "", 4.0, 1.0, 32.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::CounterLoad, "counter_load", "Счётчик с параллельной загрузкой", C::Digital, "CT;C|Q0", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::RingCounter, "ring_counter", "Кольцевой счётчик", C::Digital, "CT;C|Q0", {
            { "bits", "Число выходов (2…32)", "", 4.0, 2.0, 32.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::Adder, "adder", "Сумматор", C::Digital, "SM;A0|S0", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::CodeComparator, "code_comparator", "Компаратор кодов", C::Digital, "CMP;A0|A>B", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::BusTransceiver, "bus_transceiver", "Шинный приёмопередатчик", C::Digital, "BUF;A0|B0", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::BusSplitter, "bus_splitter", "Разветвитель шины", C::Digital, "BUS;BUS|D0", {
            { "bits", "Число линий (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::BusInput, "bus_in", "Шинный вход (n-разрядный переключатель)", C::Logic, "BUS;|Q0", {
            { "bits", "Разрядность (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "value", "Значение (двоичное число на выходах)", "", 0.0, 0.0, 4294967295.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::LUT, "lut", "Таблица истинности", C::Logic, "LUT;I0|O", {
            { "bits", "Число входов (1…5)", "", 2.0, 1.0, 5.0, nullptr, true },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
            { "table", "Таблица (бит n — выход при коде входов n)", "", 6.0, 0.0, 4294967295.0, nullptr, true },
        }, Dynamic::Bits),
        chipInfo(T::ADC, "adc", "АЦП", C::Analog, "A/D;Вх|D0", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vref", "Опорное напряжение", "V", 5.0, 0.001, 1000.0 },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::DAC, "dac", "ЦАП", C::Analog, "D/A;D0|Вых", {
            { "bits", "Разрядность (1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "vref", "Опорное напряжение", "V", 5.0, 0.001, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::AnalogMux, "analog_mux", "Аналоговый мультиплексор", C::Switches, "MUX;I0|Y", {
            { "bits", "Число адресных входов (2^n каналов, 1…5)", "", 2.0, 1.0, 5.0, nullptr, true },
            { "ron", "Сопротивление открытого ключа", "Ω", 100.0, 0.001, 1000000000.0 },
            { "threshold", "Порог управления", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Bits),
        chipInfo(T::LCD, "lcd", "ЖК-дисплей (HD44780)", C::Modules, "@lcd;VSS|D0", {
            { "size", "Тип дисплея", "", 0.0, 0.0, 7.0, "16×2|16×1|8×2|20×2|20×4|16×4|40×2|40×4" },
            { "backlight", "Подсветка", "", 1.0, 0.0, 1.0, "Выключена|Включена" },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Lcd),
        chipInfo(T::Keypad, "keypad", "Матричная клавиатура", C::Modules, "@keypad;R1|C1", {
            { "rows", "Число строк (1…32)", "", 4.0, 1.0, 32.0, nullptr, true },
            { "cols", "Число столбцов (1…32)", "", 4.0, 1.0, 32.0, nullptr, true },
            { "key", "Нажатая клавиша (номер 1…строки×столбцы, 0 — нет)", "", 0.0, 0.0, 1024.0, nullptr, true },
            { "ron", "Сопротивление нажатой клавиши", "Ω", 10.0, 0.001, 1000000.0 },
        }, Dynamic::Keypad),
        chipInfo(T::DipSwitch, "dip_switch", "DIP-переключатель", C::Modules, "@dip;1A|1B", {
            { "positions", "Число позиций (1…32)", "", 8.0, 1.0, 32.0, nullptr, true },
            { "mask", "Положение переключателей (битовая маска; бит n — позиция n+1)", "", 0.0, 0.0, 4294967295.0, nullptr, true },
            { "ron", "Сопротивление замкнутого контакта", "Ω", 0.1, 1e-06, 1000000.0 },
        }, Dynamic::Dip),
        chipInfo(T::KarnaughMap, "karnaugh_map", "Карта Карно", C::Logic, "@kmap;A|F", {
            { "width", "Ширина (столбцов, 1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "height", "Высота (строк, 1…16)", "", 4.0, 1.0, 16.0, nullptr, true },
            { "output", "Что выводить", "", 2.0, 0.0, 3.0, "СДНФ|СКНФ|Минимальная ДНФ|Минимальная КНФ" },
            { "pins", "Выводы", "", 0.0, 0.0, 1.0, "Показывать|Убрать (только клики по клеткам)" },
            { "vhigh", "Уровень логической 1", "V", 5.0, 0.0, 1000.0 },
            { "threshold", "Порог переключения", "V", 2.5, 0.0, 1000.0 },
        }, Dynamic::Karnaugh),
    };
    return kAll;
}

inline const ElementInfo& elementInfo(ElementType t)
{
    const std::vector<ElementInfo>& all = allElements();
    const std::size_t i = static_cast<std::size_t>(t);
    if (i < all.size() && all[i].type == t)
        return all[i];
    for (const ElementInfo& e : all)          // страховка от рассинхрона enum и таблицы
        if (e.type == t)
            return e;
    return all.front();
}

} // namespace govno::core
