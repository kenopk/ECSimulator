# PROJECT_MAP — GOVNO (симулятор электрических цепей, C++ / Qt)

> Файл-карта проекта. Отправляй его целиком в начале нового чата — дальше ничего объяснять не нужно.
> Состояние на 15.10.2026. Актуальный раздел «Статус» — в конце.

## 1. Что это за проект

Десктопный симулятор электрических цепей, вдохновлён https://www.falstad.com/circuit/circuitjs.html
(схема рисуется на бесконечном поле, потом цепь считается и показывается в реальном времени).

**Главное архитектурное требование автора:** проект делится на 2 независимые части,
которые можно обновлять по отдельности:

| Часть | Что делает | Зависимости |
|---|---|---|
| **core** (ядро) | Модель цепи, компоненты, расчёт (MNA), сохранение/загрузка. Без GUI. | Только C++17 (+ `QtCore` допустимо, `QtWidgets` — нельзя) |
| **gui** (оболочка) | Окна, холст, рисование, ввод мыши/клавиатуры, диалоги, темы. | Qt Widgets + публичный API ядра |

Связь только через узкий интерфейс (см. раздел 5). Ядро ничего не знает про `QGraphicsItem`.

## 2. Технологии

- C++17, Qt 6 (в сборке: Qt 6.11.1, MinGW 64-bit, Debug), CMake ≥ 3.16, `AUTOMOC/AUTOUIC/AUTORCC`
- IDE: Qt Creator. Платформа разработки: Windows. Репозиторий: git (ветка `master`, есть `origin`)
- Цель сборки: `GOVNO` (WIN32_EXECUTABLE), модуль Qt: только `Widgets`
- Папка проекта: `F:\project\qt\GOVNO`
- Язык комментариев в коде: русский

## 3. Структура папки (только то, что важно)

```
GOVNO/
├─ CMakeLists.txt            – один исполняемый target GOVNO, все файлы в одном списке
├─ main.cpp                  – QApplication + MainWindow
├─ MainWindow.h/.cpp/.ui     – стартовое меню (Создать / Открыть / Настройки / О компании), QStackedWidget
├─ EditorWindow.h/.cpp       – окно редактора: вкладки схем, меню, действия правки, настройки, темы
├─ infinitegraphicsscene.h/.cpp – QGraphicsScene: фон-сетка (drawBackground), цвет фона
├─ infinitegraphicsview.h/.cpp  – ГЛАВНЫЙ файл (~1800 строк): ввод, рисование, undo-команды, диалоги параметров, copy/paste
├─ wire.h/.cpp               – Wire : QGraphicsLineItem — провод (два конца-пина, hover, snap)
├─ component.h/.cpp          – Component : Wire — резистор/конденсаторы/..., параметры, отрисовка символов
├─ pin.h/.cpp                – Pin — структура контакта (owner, localOffset, standalonePos)
├─ resources.qrc, icon.rc, images/logo.png|ico – ресурсы
├─ .gitignore, .git/
├─ build/…                   – артефакты сборки (игнорировать)
└─ .qtcreator/…              – настройки Qt Creator (игнорировать)
```

Размеры (для ориентира): `infinitegraphicsview.cpp` 73 КБ, `EditorWindow.cpp` 41 КБ, `MainWindow.cpp` 32 КБ,
`component.cpp` 22 КБ, остальное — мелкие файлы.

## 4. Текущая архитектура (как есть СЕЙЧАС)

**Важно: отдельного ядра пока нет. Всё — GUI-слой, модель цепи и графика слиты вместе.**

```
main.cpp
 └─ MainWindow (QMainWindow)                       [GUI]
     └─ QStackedWidget
         ├─ startScreen (кнопки меню, логотип)
         └─ EditorWindow (QWidget)                 [GUI]
             └─ QTabWidget  (вкладка = одна схема; есть вкладка «Настройки»)
                 └─ InfiniteGraphicsView           [GUI + логика редактирования]
                     ├─ InfiniteGraphicsScene      [GUI]  сетка, сцена 20000×20000 (−10000…+10000)
                     ├─ QUndoStack                 [GUI]  команды правки
                     └─ items на сцене:
                         Wire : QGraphicsLineItem
                          └─ Component : Wire      (данные компонента в QVariantMap m_params)
```

### 4.1 Классы

| Класс | Файл | Роль | Ключевое |
|---|---|---|---|
| `MainWindow` | MainWindow.* | Стартовый экран + переключение на редактор | кнопки «Создать/Открыть/Настройки/О компании»; «Открыть» и «Настройки» — заглушки («soon»); есть `DebugOverlay` под `#ifdef DEBUG` |
| `EditorWindow` | EditorWindow.* | Контейнер редактора | вкладки (переименование по двойному клику, контекстное меню), `createEditActions()` — общие QAction (Wire/Resistor/Cap/PolarCap/Select/Delete/Copy/Cut/Paste/Undo/Redo), темы фона (`applyTheme`), тема курсора, `HexColorPalette`, зум (`onZoomIn/Out/FitToSize/ResetView`), `onNewProject`, `onOpenProject` (только создаёт вкладку с именем файла) |
| `InfiniteGraphicsScene` | infinitegraphicsscene.* | Сцена | сетка и фон, `setBackgroundColor` |
| `InfiniteGraphicsView` | infinitegraphicsview.* | Редактор схемы | режимы: `WireMode`, `ComponentMode(type)`, выделение, pan (средняя кнопка), зум колесом, привязка к сетке (`kGridSize = 5`, `snapToGrid → QPoint`), hover концов, перетаскивание концов/узлов, стрелки (шаг 50, Shift = 200), copy/cut/paste (свой MIME `application/x-ecsimulator-wire-group`), двойной клик → диалог параметров |
| `Wire` | wire.* | Провод | `m_startPin/m_endPin`, флаги connected/hovered/nearOther, кэш `shape()` |
| `Component` | component.* | Элемент цепи (наследник Wire) | `ComponentType`, `QVariantMap m_params`, `paint()` рисует символ (зигзаг резистора, пластины конденсатора, «+» полярного), авто-разворот подписи, если мешает проводу |
| `Pin` | pin.* | Контакт | `owner*` (пока не используется), `localOffset` (не используется), `standalonePos` (используется), `scenePos()` |

### 4.2 Undo-команды (все внутри infinitegraphicsview.cpp)

`AddComponentCommand`, `AddWireCommand`, `ChangeParamCommand`, `MoveWireEndpointCommand`,
`MoveMultipleEndpointsCommand`, `MoveItemsCommand`, `RemoveItemsCommand`.

### 4.3 Компоненты и параметры (`m_params`)

| `ComponentType` | Ключи параметров (по умолчанию) |
|---|---|
| `Resistor` | `resistance`=1000 Ом, `power`=0.25 Вт, `accuracy`=5 % |
| `Non_Polar_Capacitor` | `capacitance`=10e‑6 Ф, `voltage`=5 В, `tolerance`=10 %, `esr`=0.1 Ом |
| `Polar_Capacitor` | `capacitance`=100e‑6 Ф, `voltage`=25 В, `tolerance`=20 %, `esr`=0.5 Ом, `leakage_uA`=5 |
| `Inductor`, `Diode` | объявлены в enum, **не реализованы** (нет параметров/отрисовки/кнопок) |

### 4.4 Управление (горячие клавиши)

`W` — провод · `R` — резистор · `C` — конденсатор · `E` — электролитический конденсатор ·
`Esc` — выйти из режима · `Del/Backspace` — удалить · `Ctrl+Z / Ctrl+Y` — undo/redo ·
`Ctrl+A / C / X / V` — выделить всё/копировать/вырезать/вставить · стрелки (+Shift) — сдвиг выделенного ·
колесо — зум · средняя кнопка — панорама · двойной клик по элементу — параметры · ПКМ — контекстное меню правки

## 5. Целевая архитектура: core + gui

### 5.1 Раскладка папок (цель)

```
GOVNO/
├─ CMakeLists.txt            – корневой: add_subdirectory(core), add_subdirectory(gui)
├─ core/                     – STATIC-библиотека `govno_core`, БЕЗ Qt Widgets
│   ├─ CMakeLists.txt
│   ├─ include/govno/core/   – публичные заголовки (единственное, что видит gui)
│   │   ├─ Circuit.h         – контейнер: элементы, узлы, соединения
│   │   ├─ Element.h         – базовый элемент (id, тип, пины, параметры)
│   │   ├─ ElementTypes.h    – enum ElementType + фабрика/реестр типов
│   │   ├─ Node.h            – электрический узел (объединение пинов)
│   │   ├─ Simulator.h       – шаг/запуск/пауза, результаты (U узлов, I элементов)
│   │   └─ Serializer.h      – save/load (JSON), версия формата
│   └─ src/                  – реализация, MNA-решатель, модели элементов
├─ gui/                      – исполняемый файл `GOVNO`, зависит от govno_core + Qt Widgets
│   ├─ CMakeLists.txt
│   ├─ main.cpp, MainWindow.*, EditorWindow.*
│   ├─ canvas/               – InfiniteGraphicsScene/View, команды undo, режимы ввода
│   ├─ items/                – WireItem, ComponentItem (графика), PinItem
│   ├─ dialogs/              – диалоги параметров, настройки, палитра цвета
│   └─ resources/            – qrc, иконки
└─ docs/PROJECT_MAP.md       – этот файл
```

### 5.2 Правила границы (чтобы обновлять части независимо)

1. `core` **не** включает `<QGraphicsItem>`, `<QWidget>`, `<QPainter>`, `<QUndoCommand>`. Допустимо `QtCore` (`QString`, `QVariantMap`, `QJson*`) — или чистый STL.
2. `gui` обращается к ядру **только** через заголовки `govno/core/*.h`. Никаких прямых полей и внутренних типов.
3. Данные элемента живут в ядре (`Element::params`), графический `ComponentItem` держит лишь `ElementId` и читает/пишет через `Circuit`.
4. Соединения — по `PinId`/`NodeId`, а не по совпадению координат на холсте (сейчас «подключено» определяется геометрически).
5. Версионирование: `govno_core` имеет `CORE_API_VERSION` (major.minor). Меняется major — gui обязан обновиться; minor — совместимо.
6. Формат файла схемы описывает ядро (`Serializer`), gui только вызывает `save(path)` / `load(path)` и затем перестраивает сцену из `Circuit`.

### 5.3 Минимальный API ядра (набросок)

```cpp
namespace govno::core {
using ElementId = uint32_t; using PinId = uint32_t; using NodeId = uint32_t;

enum class ElementType { Wire, Resistor, Capacitor, PolarCapacitor, Inductor, Diode,
                         VoltageSource, CurrentSource, Ground /* ... */ };

class Circuit {
public:
    ElementId addElement(ElementType t);
    void removeElement(ElementId);
    void connect(PinId a, PinId b);          // объединить в один узел
    void disconnect(PinId a, PinId b);
    std::array<PinId,2> pins(ElementId) const;
    Params  params(ElementId) const;         // map<string, double>
    void    setParam(ElementId, const std::string& key, double value);
};

class Simulator {
public:
    explicit Simulator(Circuit&);
    void setTimeStep(double dt);
    bool step();                              // один шаг MNA
    double nodeVoltage(NodeId) const;
    double elementCurrent(ElementId) const;
};

bool saveCircuit(const Circuit&, const std::string& path);
bool loadCircuit(Circuit&, const std::string& path);
}
```

### 5.4 Соответствие «что сейчас → куда переедет»

| Сейчас | Куда |
|---|---|
| `ComponentType`, `m_params`, значения по умолчанию (`component.cpp`, конструктор) | core: `ElementType`, `Element::params`, реестр типов |
| `Pin` (owner/localOffset/standalonePos) | core: `PinId` + связи; gui: `PinItem` с координатой |
| Логика «концы совпали → соединены» (`collectNodeWires`, `findWireEndpointNear`, флаги `m_startConnected`) | core: `connect()/Node`; gui: только подсветка |
| `Wire`, `Component::paint`, `shape()`, hover | gui/items: `WireItem`, `ComponentItem` |
| `InfiniteGraphicsView`, `InfiniteGraphicsScene` | gui/canvas |
| `QUndoCommand`-классы | gui/canvas/commands (вызывают API `Circuit`) |
| Диалоги параметров, форматирование «1 kΩ», «10 pF» | gui/dialogs (парсинг единиц можно вынести в core/units) |
| `MainWindow`, `EditorWindow`, темы, курсоры | gui |
| (нет) расчёт, save/load | core: `Simulator`, `Serializer` |

## 6. Статус (что есть / чего нет)

**Работает (GUI-редактор):**
- Стартовое меню, редактор с вкладками, переименование/закрытие вкладок
- Бесконечное поле с сеткой, зум, панорама, привязка к сетке
- Рисование проводов и компонентов: резистор, неполярный и полярный конденсатор
- Выделение, перемещение (мышь/стрелки), перетаскивание концов, узлов
- Copy / Cut / Paste (через буфер обмена, свой MIME), Delete
- Undo / Redo
- Диалог параметров по двойному клику (с отменой через undo)
- Темы фона, тема курсора, вкладка «Настройки»

**Не реализовано:**
- **Ядро** — вообще нет: нет модели цепи, узлов, решателя, симуляции, источников питания, земли
- Save/Load схемы (`onOpenProject` только создаёт вкладку; «Открыть» в стартовом меню — заглушка; «Сохранить» — TODO)
- Индуктивность и диод (есть в enum, нет реализации)
- Визуализация результатов (токи, напряжения, осциллограф — как в Falstad)
- `Pin::owner/localOffset` не используются (TODO в `pin.h`)
- Подгонка масштаба «Fit to size» — только центрирование (TODO)

## 7. Известные проблемы и замечания по коду

1. **Монолит:** `infinitegraphicsview.cpp` ~1800 строк — ввод, undo-команды, диалоги, clipboard в одном файле. Разбить при выделении gui/canvas.
2. **Модель = вид:** параметры элемента хранятся в графическом `QGraphicsItem` → нельзя считать цепь без GUI. Главная причина, почему нужен рефакторинг.
3. **Компонент наследует Wire:** компонент — «провод со значком». Для ядра лучше: `Element` с двумя пинами; провод — частный случай.
4. **Регистр имён в CMake:** в `CMakeLists.txt` указаны `mainwindow.cpp`, `editorwindow.h`, а файлы называются `MainWindow.cpp`, `EditorWindow.h`. На Windows работает, на Linux/macOS сборка упадёт. Привести к одному стилю.
5. **Дублирование `#include "editorwindow.h"`/`"mainwindow.h"`** — то же самое про регистр.
6. **Мёртвый/служебный код:** `DebugOverlay` под `#ifdef DEBUG`, строка с `add_compile_definitions(DEBUG)` закомментирована в CMake.
7. Все цифры/единицы форматируются в нескольких местах (`component.cpp` и `infinitegraphicsview.cpp` — «Ω», «pF») — вынести в одну утилиту.
8. Папки `build/` и `.qtcreator/` не должны попадать в git (проверить `.gitignore`).

## 8. Предлагаемый порядок работ (дорожная карта)

1. **Этап 0.** Привести регистр имён файлов в CMake; убедиться, что `build/` в `.gitignore`.
2. **Этап 1. Выделить core как библиотеку.** `Circuit`, `Element`, `Pin/Node`, `ElementType`, `params`. GUI создаёт/меняет элементы через `Circuit`, `ComponentItem` хранит `ElementId`.
3. **Этап 2. Save/Load** (`Serializer`, JSON, версия формата) — подключить «Открыть/Сохранить».
4. **Этап 3. Симуляция:** построение узлов из соединений → MNA (резистор → источник напряжения/земля → конденсатор/индуктивность через companion-модели, шаг по времени).
5. **Этап 4. Визуализация:** цвет проводов по напряжению, ток «бегущими точками», осциллограф/графики.
6. **Этап 5.** Недостающие элементы: источники, земля, индуктивность, диод, переключатель, транзисторы…
7. **Этап 6.** Юнит-тесты ядра (CTest, без GUI), версии API, разбиение `InfiniteGraphicsView`.

## 9. Как работать с этим файлом

- Меняешь структуру или API — обнови разделы 3, 4, 5.4 и 6.
- Новый чат: приложи этот файл + (при необходимости) конкретные исходники, о которых идёт речь.
- Правило для любого запроса: «это core или gui?» — если затрагивает расчёт, формат файла, параметры элементов → core; отрисовка, ввод, окна → gui.
