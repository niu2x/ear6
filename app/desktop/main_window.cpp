#include "main_window.h"

#include <QAction>
#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(Ear6SystemType system, const QString& rom_path, QWidget* parent)
    : QMainWindow(parent), system_(system) {
    resize(640, 480);

    emulator_ = new EmulatorWidget(system_, rom_path, this);
    setCentralWidget(emulator_);

    connect(emulator_, &EmulatorWidget::load_failed, this, [this](const QString& path) {
        statusBar()->showMessage(tr("Failed to load: %1").arg(path), 5000);
    });

    create_menus();
    update_title(rom_path);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::create_menus() {
    QMenu* file_menu = menuBar()->addMenu(tr("&File"));

    QAction* open_action = file_menu->addAction(tr("&Open ROM..."));
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &MainWindow::on_open_rom);

    file_menu->addSeparator();

    QAction* quit_action = file_menu->addAction(tr("&Quit"));
    quit_action->setShortcut(QKeySequence::Quit);
    connect(quit_action, &QAction::triggered, this, &QWidget::close);

    QMenu* emulation_menu = menuBar()->addMenu(tr("&Emulation"));

    start_action_ = emulation_menu->addAction(tr("Start"));
    connect(start_action_, &QAction::triggered, this, &MainWindow::on_start);

    stop_action_ = emulation_menu->addAction(tr("Pause"));
    stop_action_->setEnabled(false);
    connect(stop_action_, &QAction::triggered, this, &MainWindow::on_stop);

    emulation_menu->addSeparator();

    QAction* reset_action = emulation_menu->addAction(tr("Reset"));
    connect(reset_action, &QAction::triggered, this, &MainWindow::on_reset);

    QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
    QAction* about_action = help_menu->addAction(tr("&About"));
    connect(about_action, &QAction::triggered, this, [this]() {
        statusBar()->showMessage(tr("Ear6 Emulator - v0.1.0"), 3000);
    });
}

void MainWindow::update_title(const QString& rom_path) {
    if (rom_path.isEmpty()) {
        setWindowTitle(tr("Ear6 Emulator"));
    } else {
        setWindowTitle(tr("Ear6 Emulator - %1").arg(rom_path));
    }
}

void MainWindow::on_open_rom() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open ROM"),
        {}, tr("ROM files (*.nes *.bin);;All files (*)"));

    if (path.isEmpty()) {
        return;
    }

    emulator_->stop();
    system_ = (path.endsWith(".nes", Qt::CaseInsensitive)) ? EAR6_SYSTEM_NES : EAR6_SYSTEM_TEST;
    emulator_->reset(system_, path);
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    update_title(path);
    statusBar()->showMessage(tr("Loaded: %1").arg(path));
}

void MainWindow::on_start() {
    if (!emulator_) {
        return;
    }
    emulator_->start();
    start_action_->setEnabled(false);
    stop_action_->setEnabled(true);
    statusBar()->showMessage(tr("Running"));
}

void MainWindow::on_stop() {
    if (!emulator_ || !emulator_->is_running()) {
        return;
    }
    emulator_->stop();
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    statusBar()->showMessage(tr("Paused"));
}

void MainWindow::on_reset() {
    if (!emulator_) {
        return;
    }
    emulator_->reset(system_);
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    statusBar()->showMessage(tr("Reset"));
}


