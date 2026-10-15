#include <QApplication>
#include <QIcon>

#include "app/MainWindow.h"
#include "theme/Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GOVNO"));
    QCoreApplication::setApplicationName(QStringLiteral("GOVNO"));
    app.setWindowIcon(QIcon(QStringLiteral(":/images/logo.png")));

    Theme::instance().load();
    Theme::instance().applyToApplication();

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
