#include "main_window.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QString>

static Ear6SystemType detect_system(const QString& path) {
    if (path.endsWith(".nes", Qt::CaseInsensitive)) {
        return EAR6_SYSTEM_NES;
    }
    return EAR6_SYSTEM_TEST;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Ear6 Emulator");

    QCommandLineParser parser;
    parser.setApplicationDescription("Ear6 multi-system emulator");
    parser.addHelpOption();
    parser.addPositionalArgument("rom", "ROM file to load (optional)");
    parser.process(app);

    Ear6SystemType system = EAR6_SYSTEM_TEST;
    QString rom_path;

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        rom_path = args.first();
        system = detect_system(rom_path);
    }

    MainWindow window(system, rom_path);
    window.show();

    return app.exec();
}
