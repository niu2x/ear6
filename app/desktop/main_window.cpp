#include "main_window.h"

#include <QAction>
#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(Ear6SystemType system, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("Ear6 Emulator"));
    resize(640, 480);

    emulator_ = new EmulatorWidget(system, this);
    setCentralWidget(emulator_);

    create_menus();

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::create_menus() {
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
    if (!emulator_) {
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
    emulator_->reset(EAR6_SYSTEM_TEST);
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    statusBar()->showMessage(tr("Reset"));
}
