#include "app/MainWindow.h"

#include <QFileDialog>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "app/EditorWindow.h"
#include "dialogs/SettingsDialog.h"
#include "project/ProjectFile.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Симулятор электрических цепей"));
    setWindowIcon(QIcon(QStringLiteral(":/images/logo.png")));
    resize(1360, 860);
    setMinimumSize(960, 620);

    m_stack = new QStackedWidget(this);
    m_start = buildStartScreen();
    m_stack->addWidget(m_start);
    setCentralWidget(m_stack);

    // имя проекта в заголовке окна
    auto updateTitle = [this] {
        const ProjectHub &hub = ProjectHub::instance();
        const ProjectInfo *p = hub.find(hub.activeId());
        setWindowTitle(p ? QStringLiteral("%1 — Симулятор электрических цепей").arg(p->name)
                         : QStringLiteral("Симулятор электрических цепей"));
    };
    connect(&ProjectHub::instance(), &ProjectHub::changed, this, updateTitle);
    updateTitle();
}

QWidget *MainWindow::buildStartScreen()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("startScreen"));
    page->setAttribute(Qt::WA_StyledBackground, true);

    auto *logo = new QLabel(page);
    const QPixmap pm(QStringLiteral(":/images/logo.png"));
    if (!pm.isNull())
        logo->setPixmap(pm.scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(QStringLiteral("Симулятор электрических цепей"), page);
    title->setObjectName(QStringLiteral("title"));
    title->setAlignment(Qt::AlignCenter);

    auto *subtitle = new QLabel(QStringLiteral("Electronic circuit simulator"), page);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    subtitle->setAlignment(Qt::AlignCenter);

    auto makeButton = [page](const QString &text, bool primary) {
        auto *b = new QPushButton(text, page);
        b->setProperty("menu", true);
        b->setProperty("primary", primary);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    auto *create   = makeButton(QStringLiteral("Создать схему"), true);
    auto *open     = makeButton(QStringLiteral("Открыть проект…"), false);
    auto *settings = makeButton(QStringLiteral("Настройки"), false);
    auto *about    = makeButton(QStringLiteral("О программе"), false);

    connect(create, &QPushButton::clicked, this, [this] { showEditor(true); });
    connect(open, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Открыть проект"), QString(),
                                                          QStringLiteral("Проект GOVNO (*.govno)"));
        if (path.isEmpty())
            return;
        showEditor(false);
        m_editor->openProjectFile(path);
    });
    connect(settings, &QPushButton::clicked, this, [this] {
        SettingsDialog dlg(this);
        dlg.exec();
    });
    connect(about, &QPushButton::clicked, this, [this] {
        QMessageBox::about(this, QStringLiteral("О программе"),
            QStringLiteral("<b>Симулятор электрических цепей</b><br>"
                           "Десктопный клон идеи Falstad Circuit Simulator на C++ / Qt.<br><br>"
                           "Сейчас готова визуальная оболочка (gui). "
                           "Расчёт цепей будет подключён из отдельного ядра (core)."));
    });

    auto *footer = new QLabel(QStringLiteral("GUI-прототип · расчёт цепей появится вместе с ядром"), page);
    footer->setObjectName(QStringLiteral("hint"));
    footer->setAlignment(Qt::AlignCenter);

    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(40, 40, 40, 28);
    lay->setSpacing(12);
    lay->addStretch(3);
    lay->addWidget(logo);
    lay->addSpacing(6);
    lay->addWidget(title);
    lay->addWidget(subtitle);
    lay->addSpacing(28);
    for (QPushButton *b : { create, open, settings, about })
        lay->addWidget(b, 0, Qt::AlignHCenter);
    lay->addStretch(4);
    lay->addWidget(footer);
    return page;
}

void MainWindow::showEditor(bool newDocument)
{
    if (!m_editor) {
        m_editor = new EditorWindow(this);          // сам создаёт первую вкладку
        connect(m_editor, &EditorWindow::backToMainMenu, this, &MainWindow::showStart);
        m_stack->addWidget(m_editor);
    } else if (newDocument || m_editor->tabCount() == 0) {
        m_editor->newTab();
    }
    m_stack->setCurrentWidget(m_editor);
}

void MainWindow::showStart()
{
    m_stack->setCurrentWidget(m_start);
}
