#include "mainwindow.h"
#include "editorwindow.h"
#include <QCursor>          // Для изменения курсора мыши и DEBUG: Для получения глобальной позиции курсора
#include <QFont>            // Работа со шрифтами
#include <QMessageBox>      // Всплывающие сообщения
#include <QWidget>          // Базовый класс для всех виджетов
#include <QVBoxLayout>      // Вертикальное расположение элементов (столбик)
#include <QHBoxLayout>      // Горизонтальное расположение (строка)
#include <QPushButton>      // Кнопки
#include <QPixmap>          // Для захвата содержимого экрана
#include <QImage>           // Для обработки пикселей (размытие)
#include <QScreen>
// ============================================================================
#ifdef DEBUG                // Классы выше исппользуются везде, а ниже только в версии дебаг
// ============================================================================
#include <QPainter>         // DEBUG: Класс для рисования (линии, текст, фигуры)
#include <QPaintEvent>      // DEBUG: Событие отрисовки
#include <QPoint>           // DEBUG: Точка (координаты X, Y)
#include <QApplication>     // DEBUG: Для установки event filter на всё приложение

// ============================================================================
// DEBUG: Класс оверлея для отладки — рисует красные линии центра экрана
// и отслеживает позицию курсора мыши
// ============================================================================
class DebugOverlay : public QWidget { // Создаём новый класс с именем DebugOverlay, который наследуется от QWidget.
public:
    // DEBUG: Конструктор оверлея, parent — родительский виджет (MainWindow)
    // конструктор принимает указатель на родительский виджет. Если родитель не передан, он будет nullptr (пустой указатель)
    // делает конструктор доступным извне (иначе мы не сможем создать объект)
    // explicit — запрещает компилятору неявно преобразовывать типы (защита от ошибок)
    explicit DebugOverlay(QWidget *parent = nullptr) : QWidget(parent) {

        /*позволяет кликам мыши проходить сквозь оверлей, чтобы кнопки под ним оставались доступны.
        setAttribute(Qt::WA_KeyCompression, false) - форма вызова
        == События мыши и клавиатуры ==
        WA_TransparentForMouseEvents - Мышь проходит сквозь виджет (клики не перехватываются)
        WA_MouseTracking - Отслеживает движение мыши даже без зажатой кнопки
        WA_UnderMouse - (Только чтение) Курсор находится над виджетом. Возвращает true, если курсор мыши физически находится над виджетом (даже если отключён).Можно проверять, находится ли мышь над холстом EditWindow
        WA_Disabled - 	(Только чтение) Виджет отключён. Возвращает true, если виджет отключён. Можно использовать в купе с подсказками (причина неработы)
        WA_NoMouseReplay - Отключает «повтор» мышиных событий. защита от "паразитных" событий мыши, которые приходят к виджету не напрямую, а через механизм повтора. Полезен, если у тебя есть перекрывающиеся виджеты.
        WA_NoMousePropagation - События мыши не передаются родителю
        WA_KeyCompression - Сжимает повторяющиеся нажатия клавиш (по умолчанию включено). вместо 100 идет 1 если они одинаковы
        WA_AcceptDrops - Разрешает drag-and-drop (перетаскивание). панель с готовыми элементами (резисторы, диоды и т.д.) и разрешить пользователю перетаскивать их на сцену.
            drag-and-drop - механизм, позволяющий перетаскивать данные. Включает приём сброса для виджета. Без него даже если ты переопределишь dropEvent(), он не будет вызываться

        == Отрисовка и прозрачность ==
        WA_TranslucentBackground - Полупрозрачный фон (поддерживает RGBA)
        WA_NoSystemBackground - Отключает системный фон (ускоряет отрисовку)
        WA_OpaquePaintEvent - Виджет полностью перерисовывает себя — Qt не стирает фон
        WA_PaintOutsidePaintEvent - Разрешает рисовать вне paintEvent() Можно использовать при осцилограмме

        == Управление памятью и жизненным циклом ==
        WA_DeleteOnClose - Виджет удаляет сам себя при закрытии
        WA_QuitOnClose - Закрытие виджета завершает всё приложение
        WA_ShowWithoutActivating - Показывает окно, не забирая фокус
        WA_AlwaysShowToolTips - Показывает подсказки даже для отключённых виджетов
        WA_NativeWindow - Создаёт настоящее окно ОС (HWND)

        == Ввод текста ==
        WA_InputMethodEnabled - Включает поддержку IME (китайский, японский и т.д.)
        WA_InputMethodTransparent - Не сбрасывает состояние IME при клике

        == Работа с дочерними виджетами ==
        WA_NoChildEventsFromChildren - Родитель не получает события о детях
        WA_NoChildEventsForParent - Дети не отправляют события родителю

        == Платформозависимые ==
        WA_MacAlwaysShowToolWindow - Всегда показывает окно в Dock
        WA_LinuxOwnershipWarning - Показывает предупреждение о владении X11-окном

        == Проверка Атрибута ==
        if (widget->testAttribute(Qt::WA_MouseTracking)) { ... }

        == Обновляй виджет после изменения ==
        widget->update();
        */

        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);

        // ====================================================================
        // DEBUG: Инициализируем позицию курсора (начальное значение — центр окна)
        // ====================================================================
        m_cursorPos = QPoint(0, 0);  // DEBUG: Начальные координаты курсора

        // ====================================================================
        // DEBUG: Устанавливаем event filter(перехватчик событий)  на всё приложение
        // Это позволяет перехватывать события мыши ДО того, как они дойдут до виджетов
        // ВАЖНО: WA_TransparentForMouseEvents остаётся — кнопки остаются кликабельными!
        // ====================================================================
        if (QApplication::instance()) {
            // получаем указатель на единственный экземпляр приложения (оно одно на всю программу)
            // if проверяем, что приложение действительно существует (защита от nullptr)
            QApplication::instance()->installEventFilter(this); // говорим приложению: "Все события мыши сначала отправляй мне (this), а потом уже целевому виджету"
        }
    }

protected: // метод доступен только внутри класса и его наследников (Qt сам его вызовет, пользователю он не нужен)
    bool eventFilter(QObject *obj, QEvent *event) override {
        /* Переопределяем метод eventFilter из базового класса QObject.
        * QObject *obj — объект, которому предназначалось событие (например, кнопка)
        * QEvent *event — само событие (движение мыши, клик и т.д.)
        * override — говорим компилятору: "Этот метод переопределяет виртуальный метод базового класса"
        * DEBUG: Проверяем, является ли событие движением мыши
        */
        if (event->type() == QEvent::MouseMove) { // Проверяем, является ли событие движением мыши.
            /* event->type() — возвращает перечисление (enum), указывающее на тип события.
              event имеет тип QEvent* (общий тип для всех событий). Но мы знаем, что это событие мыши (проверили выше).
              Чтобы получить доступ к методам QMouseEvent (например, globalPos()), нужно преобразовать тип. */
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event); // Преобразуем общий тип QEvent* в конкретный тип QMouseEvent*.
            /* static_cast<...> — оператор приведения типов в C++.
             * Переменная event имеет общий тип QEvent*,
             * у которого нет методов для работы с координатами мыши.
             * Поскольку мы уже проверили, что это событие мыши, мы безопасно "превращаем" (приводим)
             * общий указатель в указатель на конкретный класс QMouseEvent, чтобы получить доступ к его специфичным методам (например, к координатам).*/

            QPoint globalPos = mouseEvent->globalPosition().toPoint(); // Получаем глобальную позицию курсора (координаты относительно экрана).

            // DEBUG: Преобразуем глобальные координаты в локальные координаты этого виджета
            // mapFromGlobal принимает точку в экранных координатах и возвращает локальные
            m_cursorPos = mapFromGlobal(globalPos); // Преобразуем глобальные координаты в локальные координаты нашего оверлея.
            // Координаты относительно левого верхнего угла экрана -> относительно левого верхнего угла виджета

            // DEBUG: Принудительно перерисовываем виджет, чтобы обновить отображение координат
            update();
        }

        // DEBUG: ВАЖНО! Возвращаем false — это означает "не обрабатывать событие здесь,
        // передать его дальше целевому виджету". Благодаря этому кнопки остаются кликабельными!
        return false;
    }

    // DEBUG: Переопределяем метод отрисовки — вызывается автоматически при paintEvent
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);  // DEBUG: Подавляем предупреждение о неиспользуемом параметре
        //  это макрос Qt (расширяется препроцессором до специального кода). Он говорит компилятору: "Я знаю, что параметр event не используется в этом методе. Не выдавай предупреждение"

        QImage buffer(size(), QImage::Format_ARGB32_Premultiplied);
        buffer.fill(Qt::transparent);   // полностью прозрачный старт каждого кадра

        QPainter painter(&buffer);      // <-- дальше рисуем в buffer, а не в this
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setPen(QPen(Qt::red, 0.5, Qt::SolidLine));

        // ====================================================================
        // АДАПТИВНОСТЬ: Вычисляем коэффициент масштабирования
        // ====================================================================


        painter.setPen(QPen(Qt::red, 0.5, Qt::SolidLine));
        /*  Свойства пера:
         *      цвет - Qt::red
         *      толщина пера в пикселях
         *      стиль линии:
         *          Qt::SolidLine - сплошная
         *          Qt::DashLine - пунктир
         *          Qt::DotLine - точки
         *          Qt::DashDotLine - штрих-пунктир
         *          Qt::NoPen - Не писовать линии, только  запполнение
         *      QPen - конструктор пера.
         */

        // DEBUG: Вычисляем центр окна по горизонтали (половина ширины)
        float centerX = width() / 2;
        // DEBUG: Вычисляем центр окна по вертикали (половина высоты)
        float centerY = height() / 2;

        // DEBUG: Рисуем вертикальную красную линию от верха до низа по центру X
        painter.drawLine(centerX, 0, centerX, height());
        // DEBUG: Рисуем горизонтальную красную линию слева направо по центру Y
        painter.drawLine(0, centerY, width(), centerY);
        painter.drawLine(0,0, width(),height());
        painter.drawLine(width(),0, 0,height());

        // ====================================================================
        // DEBUG: Рисуем крестик в позиции курсора (для наглядности)
        // ====================================================================
        painter.setPen(QPen(Qt::green, 1, Qt::DashLine));  // DEBUG: Зелёный пунктирный крестик
        const int crossSize = 5;  // DEBUG: Размер крестика в пикселях

        // DEBUG: Вертикальная линия крестика (выше и ниже курсора)
        painter.drawLine(m_cursorPos.x(), m_cursorPos.y() - crossSize,
                         m_cursorPos.x(), m_cursorPos.y() + crossSize);
        // DEBUG: Горизонтальная линия крестика (слева и справа от курсора)
        painter.drawLine(m_cursorPos.x() - crossSize, m_cursorPos.y(),
                         m_cursorPos.x() + crossSize, m_cursorPos.y());

        // DEBUG: Маленький квадратик в центре крестика (точное попадание)
        painter.setPen(QPen(Qt::green, 2));
        painter.drawPoint(m_cursorPos);

        // ====================================================================
        // DEBUG: Рисуем информационную панель с координатами
        // ====================================================================
        painter.setRenderHint(QPainter::Antialiasing, 1);  // DEBUG: Включаем антиалиасинг для плавного текста
        //painter.setFont(QFont("Consolas", 11, QFont::Bold));  // DEBUG: Моноширинный шрифт для выравнивания цифр
        /*
         * QPainter::Antialiasing - сглаживает всё
         * QPainter::TextAntialiasing - сглаживает только текст
         * QPainter::SmoothPixmapTransform - сглаживает картинки ппри маштабировании
         * QPainter::LosslessImageRendering - Точное (без потерь) отображение картинок
        */

        // DEBUG: Создаём полупрозрачный фон под текстом (чёрный с прозрачностью 180/255)
        QRect textRect(10, 10, 140, 140);  // DEBUG: Прямоугольник для фона текста
        QImage background;
        painter.fillRect(textRect, QColor(0, 0, 0, 40));  // DEBUG: Чёрный полупрозрачный фон
        /*
         *  fillRect - заполняет прямоугольник цветом (закрашивает внутри)
         *  textRect - обьект, который заливаем
         *  QColor(0,0,0,0) - цвет RGB + прозрачность
         *  Прозрачность:
         *      0 - полностью прозрачный
         *      255 - полностью непрозрачный
        */

        // DEBUG: Рамка вокруг панели (для наглядности)
        painter.setPen(QPen(Qt::darkGray, 1));
        painter.drawRect(textRect);     // рисует только контур прямоугольника (границы)



        // DEBUG: Возвращаем жёлтый цвет для основных координат
        painter.setPen(QPen(Qt::yellow, 1));

        // DEBUG: Рисуем текст с координатой X центра
        painter.drawText(20, 30, "Center X: " + QString::number(centerX));
        /*
         *  20 - x левого нижнеего края
         *  30 - y левого нижнего края
         *  QString::number(centerX) - статический метод QString, преобразующий число в строку.
        */
        // DEBUG: Рисуем текст с координатой Y центра
        painter.drawText(20, 50, "Center Y: " + QString::number(centerY));
        // DEBUG: Рисуем размеры окна
        painter.drawText(20, 70, "Size: " + QString::number(width()) + "x" + QString::number(height()));

        // ====================================================================
        // DEBUG: Рисуем координаты курсора
        // ====================================================================
        painter.setPen(QPen(Qt::cyan, 1));  // DEBUG: Голубой цвет для курсора (чтобы отличался)
        painter.drawText(20, 95, "Cursor: " + QString::number(m_cursorPos.x()) +
                                     ", " + QString::number(m_cursorPos.y()));

        // ====================================================================
        // DEBUG: Рисуем расстояние от курсора до центра
        // ====================================================================
        int distX = m_cursorPos.x() - centerX;  // DEBUG: Расстояние по горизонтали от центра
        int distY = m_cursorPos.y() - centerY;  // DEBUG: Расстояние по вертикали от центра

        painter.setPen(QPen(Qt::white, 1));  // DEBUG: Белый цвет для расстояний
        painter.drawText(20, 120, "Offset: " + QString::number(distX) +
                                      ", " + QString::number(distY));

        // ====================================================================
        // DEBUG: Рисуем процентное смещение от центра
        // ====================================================================
        float percentX = (width() > 0) ? (float)distX / (width() / 2) * 100 : 0; // защита от 0 ширины и высоты, то есть окна нет
        float percentY = (height() > 0) ? (float)distY / (height() / 2) * 100 : 0;

        painter.setPen(QPen(QColor(200, 200, 200), 1));  // DEBUG: Серый цвет для процентов
        painter.drawText(20, 140, QString("Offset: %1%, %2%") // плейсхолдеры - %1 %2 - место для заполнения на будущее
                                      .arg(percentX, 0, 'f', 1).arg(percentY, 0, 'f', 1));
        /*
         * percentX - на что заменют %1
         * 0 - минимальная ширина поля
         * f - формат(Fixed-point - фиксированная точка)
         * 1 - количество знаков после запятой
         */
    }

private:
    // ========================================================================
    // DEBUG: Функция размытия изображения (Box Blur)
    QPoint m_cursorPos;
};
#endif  // DEBUG: Конец блока условной компиляции для класса DebugOverlay

// ============================================================================
// РЕАЛИЗАЦИЯ МЕТОДОВ КЛАССА MainWindow
// ============================================================================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Конструктор главного окна.
    qApp->setStyleSheet(buildAppStyleSheet(QColor(30, 30, 46)));
    // Здесь мы сразу вызываем две основные функции настройки.
    setupUI();      // 1. Создаём все видимые элементы (кнопки, текст)
    setupStyles();  // 2. Применяем красивые стили (цвета, градиенты)
    //setAttribute(Qt::WA_NoSystemBackground);



// ========================================================================
// DEBUG: Создаём оверлей для отладки (красные линии центра + курсор)
// ========================================================================
#ifdef DEBUG
    m_debugOverlay = new DebugOverlay(this);  // DEBUG: Создаём экземпляр оверлея с родителем this
    m_debugOverlay->show();  // DEBUG: Показываем оверлей поверх всех элементов
#endif  // DEBUG: Конец условной компиляции — в релизе этот код не попадёт в бинарник
}

MainWindow::~MainWindow() {}   // Деструктор. Пока пустой — Qt сам очистит память

void MainWindow::setupUI() {
    setWindowTitle("ECSimulator");
    setMinimumSize(500, 500);
    showMaximized();

    // Создаём главный переключатель сцен (одно окно, несколько состояний)
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    // ====================== СЦЕНА 1: Главное меню ======================
    startScreen = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(startScreen);
    mainLayout->setSpacing(20);                    // твой параметр
    mainLayout->setContentsMargins(10, 20, 0, 10); // твой параметр

    // Название
    mainLayout->addStretch(1);
    titleLabel = new QLabel("Electronic circuit simulator");
    titleLabel->setObjectName("titleLabel");
    QFont titleFont("Sans Serif", 36, QFont::Bold, 0);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    titleLabel->setStyleSheet("color: #00d9ff;");
    titleLabel->setWordWrap(false);
    mainLayout->addWidget(titleLabel);
    mainLayout->addStretch(1);

    // Блок кнопок
    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    buttonsLayout->setSpacing(5);

    btnCreate = new QPushButton("Создать проект");
    btnOpen = new QPushButton("Открыть проект");
    btnSettings = new QPushButton("Настройки");

    for (auto btn : {btnCreate, btnOpen, btnSettings}) {
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumWidth(300);
        btn->setMinimumHeight(50);
        buttonsLayout->addWidget(btn, 0, Qt::AlignCenter);
    }

    mainLayout->addLayout(buttonsLayout);
    mainLayout->addStretch(1);

    // Кнопка "?" внизу
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *btnAbout = new QPushButton("?");
    btnAbout->setFixedSize(50, 50);
    btnAbout->setToolTip("О компании");
    btnAbout->setCursor(Qt::PointingHandCursor);
    bottomLayout->addWidget(btnAbout);
    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    // ====================== СЦЕНА 2: Редактор ======================
    editorScreen = new EditorWindow(this);  // Теперь это полноценный класс

    QVBoxLayout *editorLayout = new QVBoxLayout(editorScreen);
    editorLayout->setContentsMargins(0, 0, 0, 0); // твой параметр

    // Добавляем обе сцены в переключатель
    stackedWidget->addWidget(startScreen);
    stackedWidget->addWidget(editorScreen);
    stackedWidget->setCurrentWidget(startScreen);

    // ====================== СИГНАЛЫ ======================
    connect(btnCreate, &QPushButton::clicked, this, [this]() {
        stackedWidget->setCurrentWidget(editorScreen);
    });

    connect(btnOpen, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Открыть проект", "Функция будет реализована soon!");
    });

    connect(btnSettings, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Настройки", "Функция будет реализована soon!");
    });

    connect(btnAbout, &QPushButton::clicked, this, [this]() {
        QMessageBox::about(this, "О компании",
                           "CIRCUIT SIMULATOR\n\nВерсия 1.0.0\n\nСоздатель: Кеноби");
    });
    connect(editorScreen, &EditorWindow::backToMainMenu, this, [this]() {
        stackedWidget->setCurrentWidget(startScreen);
    });

}
void MainWindow::setupStyles() {
    // === ОСНОВНОЙ ФОН ОКНА ===
    this->setStyleSheet(R"(
        QMainWindow {
            background-color: #0a0e27;   // Тёмно-синий, почти чёрный — современный техно-стиль
        }
        QWidget {
            background-color: #0a0e27;
        }
    )");

    // === СТИЛЬ ОСНОВНЫХ КНОПОК ===
    QString buttonStyle = R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,   // Градиент сверху вниз
                        stop:0 #1e3c72, stop:1 #2a5298);         // Тёмно-синие оттенки
            color: white;
            border: none;
            border-radius: 10px;          // Скруглённые углы
            font-size: 18px;
            font-weight: bold;
        }
        QPushButton:hover {               // При наведении — светлее
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 #2a5298, stop:1 #1e3c72);
        }
        QPushButton:pressed {             // При нажатии — ещё темнее
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 #162d5a, stop:1 #1e3c72);
        }
    )";

    // Применяем стиль ко всем кнопкам, кроме "?"
    QWidget *centralWidget = this->centralWidget();
    QList<QPushButton*> buttons = centralWidget->findChildren<QPushButton*>();
    for (QPushButton *btn : buttons) {
        if (btn->text() != "?") {
            btn->setStyleSheet(buttonStyle);
        }
    }

    // Специальный стиль для кнопки "?"
    QPushButton *btnAbout = centralWidget->findChild<QPushButton*>("?", Qt::FindChildrenRecursively);
    if (btnAbout) {
        btnAbout->setStyleSheet(R"(
            QPushButton {
                background-color: #00d9ff;   // Ярко-голубой неон
                color: #1e3c72;
                border: 3px solid #00d9ff;
                border-radius: 25px;
                font-size: 24px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: white;
                border-color: #00d9ff;
                color: #00d9ff;
            }
        )");
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

// ========================================================================
// DEBUG: Обновляем размер оверлея при изменении размера окна
// ========================================================================
#ifdef DEBUG
    if (m_debugOverlay) {  // DEBUG: Проверяем, существует ли оверлей (защита от nullptr)
        m_debugOverlay->setGeometry(this->rect());  // DEBUG: Растягиваем оверлей на всё окно
        m_debugOverlay->update();  // DEBUG: Принудительно перерисовываем оверлей (вызывает paintEvent)
    }
#endif  // DEBUG: Конец условной компиляции для resizeEvent

    // Адаптируем размер шрифта под ширину окна
    if (titleLabel) {
        int windowWidth = this->width();
        // Формула: базовый размер 36, но не меньше 16 и не больше 48
        int fontSize = qMax(16, qMin(48, windowWidth / 25));

        QFont font = titleLabel->font();
        font.setPointSize(fontSize);
        titleLabel->setFont(font);
    }
}