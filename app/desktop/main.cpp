#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    MainWindow window(EAR6_SYSTEM_TEST);
    window.show();

    return app.exec();
}
